#include "headers/data_channel.h"
#include "headers/utils.h"

GObject *dc_imu = NULL;
GObject *dc_gnss = NULL;
GObject *dc_cmd = NULL;

// --- vtx_send_dummy_quaternion ----------------------------------
static gboolean
vtx_send_dummy_quaternion (gpointer user_data)
{
  float epsilon = 0.1f;
  float q[4] = { epsilon * ((float) rand () / RAND_MAX - 0.5f), epsilon
        * ((float) rand () / RAND_MAX - 0.5f), epsilon
        * ((float) rand () / RAND_MAX - 0.5f), 1.0f
        + epsilon * ((float) rand () / RAND_MAX - 0.5f),
  };

  if (dc_imu) {
    GBytes *bytes = g_bytes_new (q, sizeof (q));
    g_signal_emit_by_name (dc_imu, "send-data", bytes, NULL);
    g_bytes_unref (bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_dummy_gnss ----------------------------------
static gboolean
vtx_send_dummy_gnss (gpointer user_data)
{
  if (!dc_gnss || !G_IS_OBJECT (dc_gnss))
    return G_SOURCE_REMOVE;

  double latitude = 35.0 + ((double) rand () / RAND_MAX - 0.5) * 0.01;
  double longitude = 139.0 + ((double) rand () / RAND_MAX - 0.5) * 0.01;
  double altitude = 10.0 + ((double) rand () / RAND_MAX) * 100.0;
  double speed = ((double) rand () / RAND_MAX) * 5.0;
  double heading = ((double) rand () / RAND_MAX) * 360.0;

  gchar *message =
      g_strdup_printf
      ("{\"latitude\": %.6f, \"longitude\": %.6f, \"altitude\": %.2f, \"speed\": %.2f, \"heading\": %.2f}",
      latitude, longitude, altitude, speed, heading);

  g_signal_emit_by_name (dc_gnss, "send-string", message);
  g_free (message);

  return G_SOURCE_CONTINUE;
}

// --- vtx_on_command_message ----------------------------------
static void
vtx_on_command_message (GObject * dc, gchar * str, gpointer user_data)
{
  JsonParser *parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, str, -1, NULL)) {
    gst_printerr ("Failed to parse message: %s\n", str);
    g_object_unref (parser);
    g_free (str);
    return;
  }

  JsonNode *root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root)) {
    gst_printerr ("Message is not a JSON object\n");
    g_object_unref (parser);
    g_free (str);
    return;
  }

  JsonObject *object = json_node_get_object (root);
  guint cmd = json_object_get_int_member (object, "cmd");
  g_object_unref (parser);

  switch (cmd) {
    case CMD_HANG_UP:
      gst_print ("Received: HANG_UP\n");
      vtx_cleanup_and_quit_loop ("Terminate command received", 0);
      break;

    case CMD_PING:{
      // gst_print("Received: PING\n");
      JsonObject *msg = json_object_new ();
      json_object_set_int_member (msg, "cmd", CMD_PONG);
      vtx_dc_send (dc_cmd, msg);
      break;
    }

    case CMD_SEND_KEYFRAME_REQUEST:
      gst_print ("Received: SEND_KEYFRAME_REQUEST\n");
      break;

      // case CMD_SPS_PPS:
      //   gst_print("Received: SPS_PPS\n");
      //   break;

    case CMD_ERROR:
      gst_print ("Received: ERROR\n");
      break;

    default:
      gst_print ("Received: UNKNOWN COMMAND (%d)\n", cmd);
      break;
  }
}

// --- vtx_on_open ----------------------------------
static void
vtx_on_open (GObject * dc, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_print ("[DataChannel] Channel %s opened \n", label);
  // IMU channel
  if (g_strcmp0 (label, CHANNEL_TYPE_IMU) == 0) {
    g_timeout_add (1000 / 15, vtx_send_dummy_quaternion, dc);
    // GNSS channel
  } else if (g_strcmp0 (label, CHANNEL_TYPE_GNSS) == 0) {
    g_timeout_add_seconds (1, vtx_send_dummy_gnss, NULL);
    // CMD channel
  } else if (g_strcmp0 (label, CHANNEL_TYPE_CMD) == 0) {
    g_signal_connect (dc, "on-message-string", G_CALLBACK (vtx_on_command_message), NULL);
  }
}

// --- vtx_on_error ----------------------------------
static void
vtx_on_error (GObject * dc, GError * error, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_printerr ("[DataChannel] Channel %s error: %s\n", label, error ? error->message : "unknown error");
}

// --- vtx_on_close ----------------------------------
static void
vtx_on_close (GObject * dc, gpointer user_data)
{
  const char *label = user_data ? (const char *) user_data : "unknown";
  gst_print ("[DataChannel] Channel %s closed \n", label);
}

// --- vtx_create_sender_data_channels ----------------------------------
void
vtx_create_sender_data_channels (GstElement * webrtc)
{
  // https://www.w3.org/TR/webrtc/#dom-rtcdatachannelinit

  // IMU channel
  GstStructure *imu_config = gst_structure_new ("application/x-datachannel", "ordered", G_TYPE_BOOLEAN, FALSE, "max-packet-lifetime", G_TYPE_UINT, 50, NULL);
  g_signal_emit_by_name (webrtc, "create-data-channel", CHANNEL_TYPE_IMU, imu_config, &dc_imu);
  gst_structure_free (imu_config);
  if (dc_imu) {
    g_signal_connect (dc_imu, "on-open", G_CALLBACK (vtx_on_open), CHANNEL_TYPE_IMU);
    g_signal_connect (dc_imu, "on-error", G_CALLBACK (vtx_on_error), CHANNEL_TYPE_IMU);
    g_signal_connect (dc_imu, "on-close", G_CALLBACK (vtx_on_close), CHANNEL_TYPE_IMU);
  }
  // GNSS channel
  GstStructure *gnss_config = gst_structure_new ("application/x-datachannel",
      "ordered", G_TYPE_BOOLEAN, TRUE, "max-retransmits", G_TYPE_UINT, 3,
      NULL);
  g_signal_emit_by_name (webrtc, "create-data-channel", CHANNEL_TYPE_GNSS, gnss_config, &dc_gnss);
  gst_structure_free (gnss_config);
  if (dc_gnss) {
    g_signal_connect (dc_gnss, "on-open", G_CALLBACK (vtx_on_open), CHANNEL_TYPE_GNSS);
    g_signal_connect (dc_gnss, "on-error", G_CALLBACK (vtx_on_error), CHANNEL_TYPE_GNSS);
    g_signal_connect (dc_gnss, "on-close", G_CALLBACK (vtx_on_close), CHANNEL_TYPE_GNSS);
  }
  // CMD channel
  GstStructure *cmd_config = gst_structure_new ("application/x-datachannel", "ordered", G_TYPE_BOOLEAN, TRUE, NULL);
  g_signal_emit_by_name (webrtc, "create-data-channel", CHANNEL_TYPE_CMD, cmd_config, &dc_cmd);
  gst_structure_free (cmd_config);
  if (dc_cmd) {
    g_signal_connect (dc_cmd, "on-open", G_CALLBACK (vtx_on_open), CHANNEL_TYPE_CMD);
    g_signal_connect (dc_cmd, "on-error", G_CALLBACK (vtx_on_error), CHANNEL_TYPE_CMD);
    g_signal_connect (dc_cmd, "on-close", G_CALLBACK (vtx_on_close), CHANNEL_TYPE_CMD);
  }
}

// Incoming open data channel (Do not use)

// --- vtx_on_message_string ----------------------------------
static void
vtx_on_message_string (GObject * dc, gchar * str, gpointer user_data)
{
  gst_print ("Received message: %s\n", str);
}

// --- on_data_channel ----------------------------------
void
vtx_on_data_channel (GstElement * webrtc, GObject * data_channel, gpointer user_data)
{
  gchar *label = NULL;
  g_object_get (data_channel, "label", &label, NULL);
  if (!label) {
    gst_printerr ("Unnamed DataChannel\n");
    return;
  }
  gst_print ("[RECV] DataChannel: %s\n", label);
  g_signal_connect (data_channel, "on-message-string", G_CALLBACK (vtx_on_message_string), NULL);
  g_free (label);
}
