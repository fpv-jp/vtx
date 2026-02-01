#include "glib.h"
#include "glibconfig.h"
#include "headers/data_channel.h"
#include "headers/wpa.h"

// Handles a DataChannel open event by registering the appropriate periodic telemetry sender for the channel label.
static void vtx_dc_on_open(GObject *dc, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_println("[DataChannel] Channel %s opened ", label);

  // CMD channel
  if (g_strcmp0(label, CHANNEL_TYPE_CMD) == 0)
  {
    g_signal_connect(dc, "on-message-string", G_CALLBACK(vtx_dc_on_message_command), NULL);
  }

  // Multiwii Serial Protocol (MSP)

  // MSP_RAW_IMU channel (high frequency: 50Hz) — 生値のため送信無効
  // else if (g_strcmp0(label, CHANNEL_TYPE_MSP_RAW_IMU) == 0)
  // {
  //   timeout_id_msp_raw_imu = g_timeout_add(1000 / 50, vtx_send_msp_raw_imu, dc);
  // }
  // MSP_RAW_GPS channel (1Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_RAW_GPS) == 0)
  {
    timeout_id_msp_raw_gps = g_timeout_add_seconds(1, vtx_send_msp_raw_gps, dc);
  }
  // MSP_COMP_GPS channel (1Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_COMP_GPS) == 0)
  {
    timeout_id_msp_comp_gps = g_timeout_add_seconds(1, vtx_send_msp_comp_gps, dc);
  }
  // MSP_ATTITUDE channel (30Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_ATTITUDE) == 0)
  {
    timeout_id_msp_attitude = g_timeout_add(1000 / 30, vtx_send_msp_attitude, dc);
  }
  // MSP_ALTITUDE channel (10Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_ALTITUDE) == 0)
  {
    timeout_id_msp_altitude = g_timeout_add(1000 / 10, vtx_send_msp_altitude, dc);
  }
  // MSP_ANALOG channel (2Hz) — MSP_BATTERY_STATE で代替するため送信無効
  // else if (g_strcmp0(label, CHANNEL_TYPE_MSP_ANALOG) == 0)
  // {
  //   timeout_id_msp_analog = g_timeout_add(1000 / 2, vtx_send_msp_analog, dc);
  // }
  // MSP_SONAR channel (10Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_SONAR) == 0)
  {
    timeout_id_msp_sonar = g_timeout_add(1000 / 10, vtx_send_msp_sonar, dc);
  }
  // MSP_BATTERY_STATE channel (2Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_MSP_BATTERY_STATE) == 0)
  {
    timeout_id_msp_battery_state = g_timeout_add(1000 / 2, vtx_send_msp_battery_state, dc);
  }

  // Wi-Fi Protected Access (WPA)

  // WPA_SUPPLICANT channel (1Hz)
  else if (g_strcmp0(label, CHANNEL_TYPE_WPA_SUPPLICANT) == 0)
  {
    timeout_id_wpa_supplicant = g_timeout_add_seconds(1, vtx_wpa_send_status, dc);
  }
}

// Logs an error message when a DataChannel encounters an error.
static void vtx_dc_on_error(GObject *dc, GError *error, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_printerrln("[DataChannel] Channel %s error: %s", label, error ? error->message : "unknown error");
}

// Logs a message when a DataChannel is closed.
static void vtx_dc_on_close(GObject *dc, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_println("[DataChannel] Channel %s closed ", label);
}

// Creates a WebRTC DataChannel with the given configuration and connects open/error/close signal handlers.
static GObject *vtx_dc_create_data_channel(GstElement *webrtc, const ChannelConfig *config)
{
  GstStructure *dc_config;
  GObject *dc = NULL;

  if (config->use_lifetime)
  {
    dc_config = gst_structure_new("application/x-datachannel", "ordered", G_TYPE_BOOLEAN, config->ordered, "max-packet-lifetime", G_TYPE_UINT, config->value, NULL);
  }
  else
  {
    dc_config = gst_structure_new("application/x-datachannel", "ordered", G_TYPE_BOOLEAN, config->ordered, "max-retransmits", G_TYPE_UINT, config->value, NULL);
  }

  g_signal_emit_by_name(webrtc, "create-data-channel", config->channel_type, dc_config, &dc);
  gst_structure_free(dc_config);

  if (dc)
  {
    g_signal_connect(dc, "on-open", G_CALLBACK(vtx_dc_on_open), (gpointer) config->channel_type);
    g_signal_connect(dc, "on-error", G_CALLBACK(vtx_dc_on_error), (gpointer) config->channel_type);
    g_signal_connect(dc, "on-close", G_CALLBACK(vtx_dc_on_close), (gpointer) config->channel_type);
  }

  return dc;
}

// Creates the ordered CMD DataChannel used for control messages such as hang-up and ping.
static void vtx_dc_create_cmd_channel(GstElement *webrtc)
{
  GstStructure *cmd_config = gst_structure_new("application/x-datachannel", "ordered", G_TYPE_BOOLEAN, TRUE, NULL);
  g_signal_emit_by_name(webrtc, "create-data-channel", CHANNEL_TYPE_CMD, cmd_config, &dc_cmd);
  gst_structure_free(cmd_config);

  if (dc_cmd)
  {
    g_signal_connect(dc_cmd, "on-open", G_CALLBACK(vtx_dc_on_open), CHANNEL_TYPE_CMD);
    g_signal_connect(dc_cmd, "on-error", G_CALLBACK(vtx_dc_on_error), CHANNEL_TYPE_CMD);
    g_signal_connect(dc_cmd, "on-close", G_CALLBACK(vtx_dc_on_close), CHANNEL_TYPE_CMD);
  }
}

// Creates all MSP telemetry DataChannels (IMU, GPS, attitude, altitude, analog, sonar, battery) with appropriate reliability settings.
static void vtx_dc_create_msp_channels(GstElement *webrtc)
{
  ChannelConfig configs[] = {
      // {CHANNEL_TYPE_MSP_RAW_IMU, &dc_msp_raw_imu, FALSE, TRUE, 50},         // high frequency, low-latency preferred 生値のため送信から除外 他のチャンネルで計算済みのものを送信
      {CHANNEL_TYPE_MSP_RAW_GPS, &dc_msp_raw_gps, TRUE, FALSE, 5},             // low frequency, reliability preferred
      {CHANNEL_TYPE_MSP_COMP_GPS, &dc_msp_comp_gps, TRUE, FALSE, 5},           // low frequency, reliability preferred
      {CHANNEL_TYPE_MSP_ATTITUDE, &dc_msp_attitude, FALSE, TRUE, 100},         // medium frequency, low-latency preferred
      {CHANNEL_TYPE_MSP_ALTITUDE, &dc_msp_altitude, TRUE, FALSE, 3},           // medium frequency, balanced
      // {CHANNEL_TYPE_MSP_ANALOG, &dc_msp_analog, TRUE, FALSE, 5},            // low frequency, reliability preferred MSP_BATTERY_STATE で代替するため送信から除外
      {CHANNEL_TYPE_MSP_SONAR, &dc_msp_sonar, TRUE, FALSE, 3},                 // medium frequency, balanced
      {CHANNEL_TYPE_MSP_BATTERY_STATE, &dc_msp_battery_state, TRUE, FALSE, 5}  // low frequency, reliability preferred
  };

  for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++)
  {
    *configs[i].dc_ref = vtx_dc_create_data_channel(webrtc, &configs[i]);
  }
}

// Creates the WPA_SUPPLICANT DataChannel for periodic Wi-Fi status reporting.
static void vtx_dc_create_wpa_channels(GstElement *webrtc)
{
  ChannelConfig configs[] = {
      {CHANNEL_TYPE_WPA_SUPPLICANT, &dc_wpa_supplicant, TRUE, FALSE, 5}  // reliability preferred
  };

  for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++)
  {
    *configs[i].dc_ref = vtx_dc_create_data_channel(webrtc, &configs[i]);
  }
}

// Creates all DataChannels (CMD, MSP, and optionally WPA_SUPPLICANT) on the given webrtcbin element.
void vtx_dc_create_offer(GstElement *webrtc)
{
  // https://www.w3.org/TR/webrtc/#dom-rtcdatachannelinit

  // CMD channel (always created)
  vtx_dc_create_cmd_channel(webrtc);

  // MSP channels always created (returns dummy data when FC is not connected)
  vtx_dc_create_msp_channels(webrtc);

  if (g_wpa_supplicant)
  {
    // WPA_SUPPLICANT channel (always created when available)
    vtx_dc_create_wpa_channels(webrtc);
  }
}

// Incoming open data channel (Do not use)

// Logs a string message received on an incoming DataChannel (not currently used in production).
static void vtx_dc_on_message_string(GObject *dc, gchar *str, gpointer user_data)
{
  gst_println("Received message: %s", str);
}

// Handles an incoming DataChannel created by the remote peer and attaches a message handler to it.
void vtx_webrtc_on_data_channel(GstElement *webrtc, GObject *data_channel, gpointer user_data)
{
  gchar *label = NULL;
  g_object_get(data_channel, "label", &label, NULL);
  if (!label)
  {
    gst_printerrln("Unnamed DataChannel");
    return;
  }
  gst_println("[RECV] DataChannel: %s", label);
  g_signal_connect(data_channel, "on-message-string", G_CALLBACK(vtx_dc_on_message_string), NULL);
  g_free(label);
}
