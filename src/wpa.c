#include "headers/wpa.h"

#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "headers/utils.h"

#define WPA_CTRL_SOCK_PATH "/var/run/wpa_supplicant"
#define WPA_BUFFER_SIZE 4096

struct WpaSupplicant
{
  char *interface;
  char *ctrl_path;

  struct wpa_ctrl *ctrl;
  struct wpa_ctrl *monitor;

  GIOChannel *event_channel;
  guint event_source_id;

  WpaSignalChangeCallback signal_callback;
  gpointer signal_callback_data;

  WpaStatusChangeCallback status_callback;
  gpointer status_callback_data;

  WpaStatus current_status;
};

// wpa_ctrl minimal implementation
struct wpa_ctrl
{
  int s;
  struct sockaddr_un local;
  struct sockaddr_un dest;
};

static struct wpa_ctrl *wpa_ctrl_open(const char *ctrl_path)
{
  struct wpa_ctrl *ctrl;
  static int counter = 0;
  int tries = 0;
  size_t res;

  ctrl = calloc(1, sizeof(*ctrl));
  if (ctrl == NULL) return NULL;

  ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
  if (ctrl->s < 0)
  {
    free(ctrl);
    return NULL;
  }

  ctrl->local.sun_family = AF_UNIX;
  counter++;

try_again:
  snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path), "/tmp/wpa_ctrl_%d-%d", getpid(), counter);

  tries++;
  if (bind(ctrl->s, (struct sockaddr *) &ctrl->local, sizeof(ctrl->local)) < 0)
  {
    if (errno == EADDRINUSE && tries < 2)
    {
      unlink(ctrl->local.sun_path);
      goto try_again;
    }
    close(ctrl->s);
    free(ctrl);
    return NULL;
  }

  ctrl->dest.sun_family = AF_UNIX;
  res = strlcpy(ctrl->dest.sun_path, ctrl_path, sizeof(ctrl->dest.sun_path));
  if (res >= sizeof(ctrl->dest.sun_path))
  {
    close(ctrl->s);
    free(ctrl);
    return NULL;
  }

  if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest, sizeof(ctrl->dest)) < 0)
  {
    close(ctrl->s);
    unlink(ctrl->local.sun_path);
    free(ctrl);
    return NULL;
  }

  return ctrl;
}

static void wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
  if (ctrl == NULL) return;
  unlink(ctrl->local.sun_path);
  if (ctrl->s >= 0) close(ctrl->s);
  free(ctrl);
}

static int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len, char *reply, size_t *reply_len)
{
  if (send(ctrl->s, cmd, cmd_len, 0) < 0) return -1;

  ssize_t res = recv(ctrl->s, reply, *reply_len - 1, 0);
  if (res < 0) return -1;

  reply[res] = '\0';
  *reply_len = res;
  return 0;
}

static int wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
  char buf[10];
  size_t len = sizeof(buf);
  return wpa_ctrl_request(ctrl, "ATTACH", 6, buf, &len);
}

static int wpa_ctrl_detach(struct wpa_ctrl *ctrl)
{
  char buf[10];
  size_t len = sizeof(buf);
  return wpa_ctrl_request(ctrl, "DETACH", 6, buf, &len);
}

static int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
  ssize_t res = recv(ctrl->s, reply, *reply_len - 1, 0);
  if (res < 0) return -1;
  reply[res] = '\0';
  *reply_len = res;
  return 0;
}

// Parse wpa_cli status output
static void parse_wpa_status(const char *response, WpaStatus *status)
{
  char *line = strtok((char *) response, "\n");
  memset(status, 0, sizeof(WpaStatus));
  status->mode = WPA_MODE_UNKNOWN;

  while (line != NULL)
  {
    char *value = strchr(line, '=');
    if (value)
    {
      *value = '\0';
      value++;

      if (strcmp(line, "wpa_state") == 0)
      {
        strncpy(status->state, value, sizeof(status->state) - 1);
        status->connected = (strcmp(value, "COMPLETED") == 0);
      }
      else if (strcmp(line, "ip_address") == 0)
      {
        strncpy(status->ip_address, value, sizeof(status->ip_address) - 1);

        // Detect mode based on IP address
        if (strncmp(value, "192.168.49.", 11) == 0)
        {
          status->mode = WPA_MODE_P2P_CLIENT;
        }
        else if (strncmp(value, "192.168.50.", 11) == 0)
        {
          status->mode = WPA_MODE_STA;
        }
      }
      else if (strcmp(line, "ssid") == 0)
      {
        strncpy(status->ssid, value, sizeof(status->ssid) - 1);
      }
      else if (strcmp(line, "bssid") == 0)
      {
        strncpy(status->signal.bssid, value, sizeof(status->signal.bssid) - 1);
      }
    }
    line = strtok(NULL, "\n");
  }
}

// Parse wpa_cli signal_poll output
static void parse_signal_poll(const char *response, WpaSignalInfo *signal)
{
  char *line = strtok((char *) response, "\n");

  while (line != NULL)
  {
    char *value = strchr(line, '=');
    if (value)
    {
      *value = '\0';
      value++;

      if (strcmp(line, "RSSI") == 0)
      {
        signal->rssi = atoi(value);
      }
      else if (strcmp(line, "LINKSPEED") == 0)
      {
        signal->link_speed = atoi(value);
      }
      else if (strcmp(line, "NOISE") == 0)
      {
        signal->noise = atoi(value);
      }
      else if (strcmp(line, "FREQUENCY") == 0)
      {
        signal->frequency = atoi(value);
      }
    }
    line = strtok(NULL, "\n");
  }
}

// Event handler callback for GIOChannel
static gboolean on_wpa_event(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
  WpaSupplicant *wpa = (WpaSupplicant *) user_data;
  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf) - 1;

  if (condition & G_IO_IN)
  {
    if (wpa_ctrl_recv(wpa->monitor, buf, &len) == 0)
    {
      buf[len] = '\0';

      // Handle different event types
      if (strstr(buf, "CTRL-EVENT-SIGNAL-CHANGE") != NULL)
      {
        // Signal change event - update signal info
        WpaSignalInfo signal;
        memset(&signal, 0, sizeof(signal));

        // Parse event message for signal info
        // Format: <3>CTRL-EVENT-SIGNAL-CHANGE above=1 signal=-50 noise=9999 txrate=65000
        char *signal_str = strstr(buf, "signal=");
        if (signal_str)
        {
          signal.rssi = atoi(signal_str + 7);
        }

        char *noise_str = strstr(buf, "noise=");
        if (noise_str)
        {
          signal.noise = atoi(noise_str + 6);
        }

        char *txrate_str = strstr(buf, "txrate=");
        if (txrate_str)
        {
          signal.link_speed = atoi(txrate_str + 7) / 1000;  // Convert to Mbps
        }

        // Get full signal info
        wpa_supplicant_get_signal_poll(wpa, &signal);

        // Update status signal info
        wpa->current_status.signal = signal;

        // Call callback
        if (wpa->signal_callback)
        {
          wpa->signal_callback(wpa, &signal, wpa->signal_callback_data);
        }
      }
      else if (strstr(buf, "CTRL-EVENT-CONNECTED") != NULL || strstr(buf, "CTRL-EVENT-DISCONNECTED") != NULL || strstr(buf, "P2P-GROUP-STARTED") != NULL || strstr(buf, "P2P-GROUP-REMOVED") != NULL)
      {
        // Connection status change - update full status
        WpaStatus status;
        if (wpa_supplicant_get_status(wpa, &status))
        {
          wpa->current_status = status;

          if (wpa->status_callback)
          {
            wpa->status_callback(wpa, &status, wpa->status_callback_data);
          }
        }
      }
    }
  }

  return G_SOURCE_CONTINUE;
}

WpaSupplicant *wpa_supplicant_new(const char *interface_name, const char *ctrl_path)
{
  WpaSupplicant *wpa = g_new0(WpaSupplicant, 1);

  wpa->interface = g_strdup(interface_name);
  wpa->ctrl_path = ctrl_path ? g_strdup(ctrl_path) : g_strdup_printf("%s/%s", WPA_CTRL_SOCK_PATH, interface_name);

  // Open control interface
  wpa->ctrl = wpa_ctrl_open(wpa->ctrl_path);
  if (!wpa->ctrl)
  {
    gst_printerrln("[WPA] Failed to open control interface: %s", wpa->ctrl_path);
    wpa_supplicant_free(wpa);
    return NULL;
  }

  // Open monitor interface
  wpa->monitor = wpa_ctrl_open(wpa->ctrl_path);
  if (!wpa->monitor)
  {
    gst_printerrln("[WPA] Failed to open monitor interface: %s", wpa->ctrl_path);
    wpa_supplicant_free(wpa);
    return NULL;
  }

  // Attach to event notifications
  if (wpa_ctrl_attach(wpa->monitor) != 0)
  {
    gst_printerrln("[WPA] Failed to attach to wpa_supplicant");
    wpa_supplicant_free(wpa);
    return NULL;
  }

  gst_println("[WPA] Connected to wpa_supplicant on %s", interface_name);

  return wpa;
}

void wpa_supplicant_free(WpaSupplicant *wpa)
{
  if (!wpa) return;

  wpa_supplicant_stop_monitor(wpa);

  if (wpa->monitor)
  {
    wpa_ctrl_detach(wpa->monitor);
    wpa_ctrl_close(wpa->monitor);
  }

  if (wpa->ctrl)
  {
    wpa_ctrl_close(wpa->ctrl);
  }

  g_free(wpa->interface);
  g_free(wpa->ctrl_path);
  g_free(wpa);
}

gboolean wpa_supplicant_start_monitor(WpaSupplicant *wpa)
{
  if (!wpa || !wpa->monitor) return FALSE;

  if (wpa->event_source_id > 0) return TRUE;  // Already monitoring

  // Create GIOChannel for monitor socket
  wpa->event_channel = g_io_channel_unix_new(wpa->monitor->s);
  g_io_channel_set_encoding(wpa->event_channel, NULL, NULL);
  g_io_channel_set_buffered(wpa->event_channel, FALSE);

  // Add watch for events
  wpa->event_source_id = g_io_add_watch(wpa->event_channel, G_IO_IN, on_wpa_event, wpa);

  gst_println("[WPA] Event monitoring started");

  return TRUE;
}

void wpa_supplicant_stop_monitor(WpaSupplicant *wpa)
{
  if (!wpa) return;

  if (wpa->event_source_id > 0)
  {
    g_source_remove(wpa->event_source_id);
    wpa->event_source_id = 0;
  }

  if (wpa->event_channel)
  {
    g_io_channel_unref(wpa->event_channel);
    wpa->event_channel = NULL;
  }

  gst_println("[WPA] Event monitoring stopped");
}

gboolean wpa_supplicant_get_status(WpaSupplicant *wpa, WpaStatus *status)
{
  if (!wpa || !wpa->ctrl || !status) return FALSE;

  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf);

  if (wpa_ctrl_request(wpa->ctrl, "STATUS", 6, buf, &len) != 0)
  {
    return FALSE;
  }

  parse_wpa_status(buf, status);

  // Also get signal info if connected
  if (status->connected)
  {
    wpa_supplicant_get_signal_poll(wpa, &status->signal);
  }

  return TRUE;
}

gboolean wpa_supplicant_get_signal_poll(WpaSupplicant *wpa, WpaSignalInfo *signal)
{
  if (!wpa || !wpa->ctrl || !signal) return FALSE;

  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf);

  if (wpa_ctrl_request(wpa->ctrl, "SIGNAL_POLL", 11, buf, &len) != 0)
  {
    return FALSE;
  }

  parse_signal_poll(buf, signal);

  return TRUE;
}

void wpa_supplicant_set_signal_callback(WpaSupplicant *wpa, WpaSignalChangeCallback callback, gpointer user_data)
{
  if (!wpa) return;
  wpa->signal_callback = callback;
  wpa->signal_callback_data = user_data;
}

void wpa_supplicant_set_status_callback(WpaSupplicant *wpa, WpaStatusChangeCallback callback, gpointer user_data)
{
  if (!wpa) return;
  wpa->status_callback = callback;
  wpa->status_callback_data = user_data;
}

WpaNetworkMode wpa_supplicant_get_mode(WpaSupplicant *wpa)
{
  if (!wpa) return WPA_MODE_UNKNOWN;
  return wpa->current_status.mode;
}

gboolean wpa_supplicant_is_connected(WpaSupplicant *wpa)
{
  if (!wpa) return FALSE;
  return wpa->current_status.connected;
}

// Global instance
WpaSupplicant *g_wpa_supplicant = NULL;
GObject *dc_wpa_supplicant = NULL;
guint timeout_id_wpa_supplicant = 0;

gboolean vtx_wpa_supplicant_init(const char *interface_name)
{
  if (g_wpa_supplicant)
  {
    gst_printerrln("[WPA] Already initialized");
    return FALSE;
  }

  g_wpa_supplicant = wpa_supplicant_new(interface_name, NULL);
  if (!g_wpa_supplicant)
  {
    return FALSE;
  }

  // Get initial status
  WpaStatus status;
  if (wpa_supplicant_get_status(g_wpa_supplicant, &status))
  {
    g_wpa_supplicant->current_status = status;
    gst_println("[WPA] Mode: %s, Connected: %s, IP: %s", status.mode == WPA_MODE_P2P_CLIENT ? "P2P Client" : (status.mode == WPA_MODE_STA ? "SoftAP Client" : "Unknown"), status.connected ? "Yes" : "No", status.ip_address);
  }

  // Start event monitoring
  wpa_supplicant_start_monitor(g_wpa_supplicant);

  return TRUE;
}

void vtx_wpa_supplicant_cleanup(void)
{
  if (timeout_id_wpa_supplicant > 0)
  {
    g_source_remove(timeout_id_wpa_supplicant);
    timeout_id_wpa_supplicant = 0;
  }

  if (dc_wpa_supplicant)
  {
    g_object_unref(dc_wpa_supplicant);
    dc_wpa_supplicant = NULL;
  }

  if (g_wpa_supplicant)
  {
    wpa_supplicant_free(g_wpa_supplicant);
    g_wpa_supplicant = NULL;
  }

  gst_println("[WPA] Cleaned up");
}

gboolean vtx_send_wpa_status(gpointer user_data)
{
  GObject *dc = (GObject *) user_data;

  if (!g_wpa_supplicant || !dc) return G_SOURCE_REMOVE;

  WpaStatus status;
  if (!wpa_supplicant_get_status(g_wpa_supplicant, &status))
  {
    return G_SOURCE_CONTINUE;
  }

  // Create JSON object
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "mode");
  json_builder_add_string_value(builder, status.mode == WPA_MODE_P2P_CLIENT ? "p2p_client" : (status.mode == WPA_MODE_STA ? "sta" : "unknown"));

  json_builder_set_member_name(builder, "connected");
  json_builder_add_boolean_value(builder, status.connected);

  json_builder_set_member_name(builder, "state");
  json_builder_add_string_value(builder, status.state);

  json_builder_set_member_name(builder, "ip_address");
  json_builder_add_string_value(builder, status.ip_address);

  json_builder_set_member_name(builder, "ssid");
  json_builder_add_string_value(builder, status.ssid);

  // Signal info
  json_builder_set_member_name(builder, "signal");
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "rssi");
  json_builder_add_int_value(builder, status.signal.rssi);

  json_builder_set_member_name(builder, "link_speed");
  json_builder_add_int_value(builder, status.signal.link_speed);

  json_builder_set_member_name(builder, "noise");
  json_builder_add_int_value(builder, status.signal.noise);

  json_builder_set_member_name(builder, "frequency");
  json_builder_add_int_value(builder, status.signal.frequency);

  json_builder_set_member_name(builder, "bssid");
  json_builder_add_string_value(builder, status.signal.bssid);

  json_builder_end_object(builder);

  json_builder_end_object(builder);

  // Generate JSON string
  JsonGenerator *generator = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(generator, root);
  gchar *json_string = json_generator_to_data(generator, NULL);

  // Send via data channel
  GBytes *bytes = g_bytes_new_take(json_string, strlen(json_string));
  g_signal_emit_by_name(dc, "send-string", json_string);
  g_bytes_unref(bytes);

  json_node_free(root);
  g_object_unref(generator);
  g_object_unref(builder);

  return G_SOURCE_CONTINUE;
}
