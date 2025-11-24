#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "headers/nic.h"
#include "headers/utils.h"
#include "headers/wpa.h"

#define WPA_CTRL_SOCK_PATH "/var/run/wpa_supplicant"
#define WPA_BUFFER_SIZE WPA_RESPONSE_MAX_LEN

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
};

// wpa_ctrl minimal implementation
struct wpa_ctrl
{
  int s;
  struct sockaddr_un local;
  struct sockaddr_un dest;
};

// --- vtx_wpa_ctrl_open ----------------------------------
static struct wpa_ctrl *vtx_wpa_ctrl_open(const char *ctrl_path)
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
  res = g_strlcpy(ctrl->dest.sun_path, ctrl_path, sizeof(ctrl->dest.sun_path));
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

// --- vtx_wpa_ctrl_close ----------------------------------
static void vtx_wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
  if (ctrl == NULL) return;
  unlink(ctrl->local.sun_path);
  if (ctrl->s >= 0) close(ctrl->s);
  free(ctrl);
}

// --- vtx_wpa_ctrl_request ----------------------------------
static int vtx_wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len, char *reply, size_t *reply_len)
{
  if (send(ctrl->s, cmd, cmd_len, 0) < 0) return -1;

  ssize_t res = recv(ctrl->s, reply, *reply_len - 1, 0);
  if (res < 0) return -1;

  reply[res] = '\0';
  *reply_len = res;
  return 0;
}

// --- vtx_wpa_copy_response ----------------------------------
static void vtx_wpa_copy_response(char *dest, size_t dest_size, const char *src, size_t src_len)
{
  if (!dest || dest_size == 0) return;

  size_t copy_len = MIN(src_len, dest_size - 1);
  memcpy(dest, src, copy_len);
  dest[copy_len] = '\0';
}

// Event handler callback for GIOChannel
// --- on_wpa_event ----------------------------------
static gboolean on_wpa_event(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
  WpaSupplicant *wpa = (WpaSupplicant *) user_data;
  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf) - 1;

  if (condition & G_IO_IN)
  {
    ssize_t res = recv(wpa->monitor->s, buf, len, 0);
    if (res >= 0)
    {
      len = (size_t) res;
      buf[len] = '\0';

      // Handle different event types
      if (strstr(buf, "CTRL-EVENT-SIGNAL-CHANGE") != NULL)
      {
        WpaSignalInfo signal;
        memset(&signal, 0, sizeof(signal));

        if (vtx_wpa_supplicant_get_signal_poll(wpa, &signal))
        {
          if (wpa->signal_callback)
          {
            wpa->signal_callback(wpa, &signal, wpa->signal_callback_data);
          }
        }
      }
      else if (strstr(buf, "CTRL-EVENT-CONNECTED") != NULL || strstr(buf, "CTRL-EVENT-DISCONNECTED") != NULL || strstr(buf, "P2P-GROUP-STARTED") != NULL || strstr(buf, "P2P-GROUP-REMOVED") != NULL)
      {
        // Connection status change - update full status
        WpaStatus status;
        memset(&status, 0, sizeof(status));
        if (vtx_wpa_supplicant_get_status(wpa, &status))
        {
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

// --- vtx_wpa_supplicant_new ----------------------------------
WpaSupplicant *vtx_wpa_supplicant_new(const char *interface_name, const char *ctrl_path)
{
  WpaSupplicant *wpa = g_new0(WpaSupplicant, 1);

  wpa->interface = g_strdup(interface_name);
  wpa->ctrl_path = ctrl_path ? g_strdup(ctrl_path) : g_strdup_printf("%s/%s", WPA_CTRL_SOCK_PATH, interface_name);

  // Open control interface
  wpa->ctrl = vtx_wpa_ctrl_open(wpa->ctrl_path);
  if (!wpa->ctrl)
  {
    gst_printerrln("[WPA] Failed to open control interface: %s", wpa->ctrl_path);
    vtx_wpa_supplicant_free(wpa);
    return NULL;
  }

  // Open monitor interface
  wpa->monitor = vtx_wpa_ctrl_open(wpa->ctrl_path);
  if (!wpa->monitor)
  {
    gst_printerrln("[WPA] Failed to open monitor interface: %s", wpa->ctrl_path);
    vtx_wpa_supplicant_free(wpa);
    return NULL;
  }

  // Attach to event notifications
  char attach_buf[10];
  size_t attach_len = sizeof(attach_buf);
  if (vtx_wpa_ctrl_request(wpa->monitor, "ATTACH", 6, attach_buf, &attach_len) != 0)
  {
    gst_printerrln("[WPA] Failed to attach to wpa_supplicant");
    vtx_wpa_supplicant_free(wpa);
    return NULL;
  }

  gst_println("[WPA] Connected to wpa_supplicant on %s", interface_name);

  return wpa;
}

// --- vtx_wpa_supplicant_free ----------------------------------
void vtx_wpa_supplicant_free(WpaSupplicant *wpa)
{
  if (!wpa) return;

  vtx_wpa_supplicant_stop_monitor(wpa);

  if (wpa->monitor)
  {
    char buf[10];
    size_t len = sizeof(buf);
    vtx_wpa_ctrl_request(wpa->monitor, "DETACH", 6, buf, &len);
    vtx_wpa_ctrl_close(wpa->monitor);
  }

  if (wpa->ctrl)
  {
    vtx_wpa_ctrl_close(wpa->ctrl);
  }

  g_free(wpa->interface);
  g_free(wpa->ctrl_path);
  g_free(wpa);
}

// --- vtx_wpa_supplicant_start_monitor ----------------------------------
gboolean vtx_wpa_supplicant_start_monitor(WpaSupplicant *wpa)
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

// --- vtx_wpa_supplicant_stop_monitor ----------------------------------
void vtx_wpa_supplicant_stop_monitor(WpaSupplicant *wpa)
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

// --- vtx_wpa_supplicant_get_status ----------------------------------
gboolean vtx_wpa_supplicant_get_status(WpaSupplicant *wpa, WpaStatus *status)
{
  if (!wpa || !wpa->ctrl || !status) return FALSE;

  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf);

  if (vtx_wpa_ctrl_request(wpa->ctrl, "STATUS", 6, buf, &len) != 0)
  {
    return FALSE;
  }

  vtx_wpa_copy_response(status->raw, sizeof(status->raw), buf, len);

  // Always try to fetch signal info as well
  if (!vtx_wpa_supplicant_get_signal_poll(wpa, &status->signal))
  {
    status->signal.raw[0] = '\0';
  }

  return TRUE;
}

// --- vtx_wpa_supplicant_get_signal_poll ----------------------------------
gboolean vtx_wpa_supplicant_get_signal_poll(WpaSupplicant *wpa, WpaSignalInfo *signal)
{
  if (!wpa || !wpa->ctrl || !signal) return FALSE;

  char buf[WPA_BUFFER_SIZE];
  size_t len = sizeof(buf);

  if (vtx_wpa_ctrl_request(wpa->ctrl, "SIGNAL_POLL", 11, buf, &len) != 0)
  {
    return FALSE;
  }

  vtx_wpa_copy_response(signal->raw, sizeof(signal->raw), buf, len);

  return TRUE;
}

// Global instance
WpaSupplicant *g_wpa_supplicant = NULL;
GObject *dc_wpa_supplicant = NULL;
guint timeout_id_wpa_supplicant = 0;

// --- vtx_wpa_supplicant_init ----------------------------------
gboolean vtx_wpa_supplicant_init(const char *interface_name)
{
  if (g_wpa_supplicant)
  {
    gst_printerrln("[WPA] Already initialized");
    return FALSE;
  }

  g_wpa_supplicant = vtx_wpa_supplicant_new(interface_name, NULL);
  if (!g_wpa_supplicant)
  {
    return FALSE;
  }

  // Get initial status
  WpaStatus status;
  memset(&status, 0, sizeof(status));
  // if (vtx_wpa_supplicant_get_status(g_wpa_supplicant, &status))
  // {
  //   gst_println("[WPA] Initial STATUS:\n%s", status.raw[0] ? status.raw : "(empty)");
  //   gst_println("[WPA] Initial SIGNAL_POLL:\n%s", status.signal.raw[0] ? status.signal.raw : "(empty)");
  // }

  // Start event monitoring
  vtx_wpa_supplicant_start_monitor(g_wpa_supplicant);

  return TRUE;
}

// --- vtx_wpa_supplicant_cleanup ----------------------------------
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
    vtx_wpa_supplicant_free(g_wpa_supplicant);
    g_wpa_supplicant = NULL;
  }

  gst_println("[WPA] Cleaned up");
}

// --- vtx_wpa_send_status ----------------------------------
gboolean vtx_wpa_send_status(gpointer user_data)
{
  GObject *dc = (GObject *) user_data;

  if (!g_wpa_supplicant || !dc) return G_SOURCE_REMOVE;

  WpaStatus status;
  memset(&status, 0, sizeof(status));
  if (!vtx_wpa_supplicant_get_status(g_wpa_supplicant, &status))
  {
    return G_SOURCE_CONTINUE;
  }

  // Create JSON object
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "status");
  json_builder_begin_object(builder);
  vtx_nic_parse_wpa_for_datachannel(builder, status.raw);
  json_builder_end_object(builder);

  json_builder_set_member_name(builder, "signal_poll");
  json_builder_begin_object(builder);
  vtx_nic_parse_wpa_for_datachannel(builder, status.signal.raw);
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
