#include <locale.h>
#include "headers/utils.h"
#include "headers/pipeline.h"

// --- vtx_ws_send ----------------------------------
void
vtx_ws_send (SoupWebsocketConnection * conn, int type, const gchar * ws1Id, const gchar * ws2Id, JsonObject * data)
{
  JsonObject *msg = json_object_new ();

  json_object_set_int_member (msg, "type", type);
  json_object_set_string_member (msg, "ws1Id", ws1Id);
  json_object_set_string_member (msg, "ws2Id", ws2Id);

  GList *members = json_object_get_members (data);
  for (GList * l = members; l != NULL; l = l->next) {
    const gchar *key = l->data;
    JsonNode *value = json_object_get_member (data, key);
    json_object_set_member (msg, key, json_node_copy (value));
  }

  g_list_free (members);

  JsonNode *root = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (root, msg);

  JsonGenerator *gen = json_generator_new ();
  json_generator_set_pretty (gen, TRUE);
  json_generator_set_root (gen, root);

  gsize length = 0;
  gchar *json_str = json_generator_to_data (gen, &length);

  // gst_print("[WebSocket-Send] Size: %lu, Content: %s\n", (unsigned long) length, json_str);

  soup_websocket_connection_send_text (conn, json_str);

  g_free (json_str);
  g_object_unref (gen);
  json_node_free (root);
  json_object_unref (msg);
}

// --- vtx_dc_send ----------------------------------
void
vtx_dc_send (GObject * dc, JsonObject * msg)
{
  JsonNode *node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, msg);

  JsonGenerator *gen = json_generator_new ();
  json_generator_set_pretty (gen, TRUE);
  json_generator_set_root (gen, node);
  gchar *message = json_generator_to_data (gen, NULL);

  g_signal_emit_by_name (dc, "send-string", message);

  g_free (message);
  g_object_unref (gen);
  json_node_free (node);
  json_object_unref (msg);
}

// --- vtx_cleanup_and_quit_loop ----------------------------------
gboolean
vtx_cleanup_and_quit_loop (const gchar * msg, AppState state)
{
  if (msg)
    gst_printerr ("%s\n", msg);

  if (state > 0)
    app_state = state;

  if (ws_conn) {
    if (soup_websocket_connection_get_state (ws_conn) == SOUP_WEBSOCKET_STATE_OPEN)
      soup_websocket_connection_close (ws_conn, 1000, "");
    g_clear_object (&ws_conn);
  }

  if (loop) {
    g_main_loop_quit (loop);
    g_clear_pointer (&loop, g_main_loop_unref);
  }

  return TRUE;
}


// --- vtx_check_plugins ----------------------------------
gboolean
vtx_check_plugins (void)
{
  g_print ("----- Check plugins -----\n");

  GstRegistry *registry = gst_registry_get ();
  gboolean ret = TRUE;
  GstPlugin *plugin;

  GPtrArray *needed = g_ptr_array_new_with_free_func (g_free);

  const gchar *common_plugins[] = {
    "nice",
    "webrtc",
    "rtp",
    "rtpmanager",
    "dtls",
    "srtp",
    "videoparsersbad",
    "opus",
    "opusparse",
    NULL
  };
  for (int i = 0; common_plugins[i]; i++) {
    g_ptr_array_add (needed, g_strdup (common_plugins[i]));
  }

  switch (PLATFORM) {
    case INTEL_MAC:
      g_ptr_array_add (needed, g_strdup ("applemedia"));
      g_ptr_array_add (needed, g_strdup ("vpx"));
      g_ptr_array_add (needed, g_strdup ("svtav1"));
      g_ptr_array_add (needed, g_strdup ("osxvideo"));
      g_ptr_array_add (needed, g_strdup ("osxaudio"));
      break;

    case LINUX_X86:
      g_ptr_array_add (needed, g_strdup ("vaapi"));
      g_ptr_array_add (needed, g_strdup ("va"));
      g_ptr_array_add (needed, g_strdup ("vpx"));
      g_ptr_array_add (needed, g_strdup ("svtav1"));
      g_ptr_array_add (needed, g_strdup ("alsa"));
      g_ptr_array_add (needed, g_strdup ("video4linux2"));
      g_ptr_array_add (needed, g_strdup ("waylandsink"));
      // g_ptr_array_add (needed, g_strdup ("pulseaudio"));
      break;

    case RPI4_V4L2:
      g_ptr_array_add (needed, g_strdup ("video4linux2"));
      g_ptr_array_add (needed, g_strdup ("alsa"));
      break;

    case JETSON_NANO_2GB:
      g_ptr_array_add (needed, g_strdup ("nvarguscamerasrc"));
      g_ptr_array_add (needed, g_strdup ("nvv4l2camerasrc"));
      // g_ptr_array_add (needed, g_strdup ("nvvideo4linux2"));
      // g_ptr_array_add (needed, g_strdup ("nvv4l2h264enc"));
      // g_ptr_array_add (needed, g_strdup ("nvv4l2h265enc"));
      // g_ptr_array_add (needed, g_strdup ("nvv4l2vp8enc"));
      // g_ptr_array_add (needed, g_strdup ("nvv4l2vp9enc"));
      g_ptr_array_add (needed, g_strdup ("nvvidconv"));
      g_ptr_array_add (needed, g_strdup ("nvvideocuda"));
      g_ptr_array_add (needed, g_strdup ("alsa"));
      break;

    case ROCK_B5:
      g_ptr_array_add (needed, g_strdup ("rockchipmpp"));
      g_ptr_array_add (needed, g_strdup ("video4linux2"));
      g_ptr_array_add (needed, g_strdup ("rkximage"));
      g_ptr_array_add (needed, g_strdup ("alsa"));
      break;

    case RPI4_LIBCAM:
      g_ptr_array_add (needed, g_strdup ("libcamera"));
      g_ptr_array_add (needed, g_strdup ("alsa"));
      break;

    default:
      break;
  }

  for (guint i = 0; i < needed->len; i++) {

    const gchar *plugin_name = g_ptr_array_index (needed, i);
    plugin = gst_registry_find_plugin (registry, plugin_name);

    if (!plugin) {
      g_printerr ("Required GStreamer plugin '%s' not found\n", plugin_name);
      ret = FALSE;
    } else {
      gst_println ("  -> OK %s", plugin_name);
      gst_object_unref (plugin);
    }

  }

  g_ptr_array_free (needed, TRUE);
  return ret;
}

// --- print_json_object ----------------------------------
void
print_json_object (JsonObject * obj)
{
  JsonNode *node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, obj);
  gchar *str = json_to_string (node, TRUE);

  setlocale (LC_ALL, "en_US.UTF-8");
  gst_println ("%s", str);

  g_free (str);
  json_node_free (node);
}

// --- print_json_array ----------------------------------
void
print_json_array (JsonArray * array)
{
  JsonNode *node = json_node_new (JSON_NODE_ARRAY);
  json_node_set_array (node, array);
  gchar *str = json_to_string (node, TRUE);

  setlocale (LC_ALL, "en_US.UTF-8");
  gst_println ("%s", str);

  g_free (str);
  json_node_free (node);
}
