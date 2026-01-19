#include "headers/data_channel.h"
#include "headers/utils.h"

// Global CMD data channel reference
GObject *dc_cmd = NULL;

// --- vtx_dc_on_message_command ----------------------------------
void vtx_dc_on_message_command(GObject *dc, gchar *str, gpointer user_data)
{
  JsonParser *parser = json_parser_new();
  if (!json_parser_load_from_data(parser, str, -1, NULL))
  {
    gst_printerrln("Failed to parse message: %s", str);
    g_object_unref(parser);
    g_free(str);
    return;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root))
  {
    gst_printerrln("Message is not a JSON object");
    g_object_unref(parser);
    g_free(str);
    return;
  }

  JsonObject *object = json_node_get_object(root);
  guint cmd = json_object_get_int_member(object, "cmd");
  g_object_unref(parser);

  switch (cmd)
  {
    case CMD_HANG_UP:
      gst_println("Received: HANG_UP");
      vtx_cleanup_connection("Hang-up command received");
      break;

    case CMD_PING:
    {
      // gst_println("Received: PING");
      gchar *message = g_strdup_printf("{\"cmd\": %d}", CMD_PONG);
      g_signal_emit_by_name(dc, "send-string", message);
      g_free(message);
      break;
    }

      // case CMD_SEND_KEYFRAME_REQUEST:
      //   gst_println("Received: SEND_KEYFRAME_REQUEST");
      //   break;

      // case CMD_SPS_PPS:
      //   gst_println("Received: SPS_PPS");
      //   break;

    case CMD_ERROR:
      gst_println("Received: ERROR");
      break;

    default:
      gst_println("Received: UNKNOWN COMMAND (%d)", cmd);
      break;
  }
}
