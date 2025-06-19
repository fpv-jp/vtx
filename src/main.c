#include "headers/common.h"
#include "headers/signaling.h"
#include "headers/utils.h"
#include "headers/inspection.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

// --- main ----------------------------------
int
main (int argc, char *argv[])
{

  gst_init (&argc, &argv);

  if (!vtx_check_plugins ()) {
    goto out;
  }

  JsonArray *device_capabilities = vtx_load_device_launch_entries ();
  gst_println ("----- Device capabilities -----");
  print_json_array (device_capabilities);

  JsonObject *codecs_capabilities = vtx_supported_codecs_inspection ();
  gst_println ("----- Codecs capabilities -----");
  print_json_object (codecs_capabilities);

  loop = g_main_loop_new (NULL, FALSE);

  vtx_soup_session_websocket_connect_async ();

  g_main_loop_run (loop);

  if (loop)
    g_main_loop_unref (loop);
  if (pipe1)
    gst_object_unref (pipe1);

  g_free (ws1Id);
  g_free (ws2Id);

out:
  return 0;
}
