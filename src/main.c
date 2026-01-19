#include "headers/common.h"
#include "headers/msp.h"
#include "headers/signaling.h"
#include "headers/utils.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

// --- main ----------------------------------
int main(int argc, char *argv[])
{
  gst_debug_set_default_threshold(GST_LEVEL_FIXME);
  gst_init(&argc, &argv);

  if (!vtx_check_gst_plugins())
  {
    goto out;
  }

  char **ports;
  int count = vtx_msp_detect_all(&ports);
  gst_println("Number of detected ports: %d", count);
  for (int i = 0; i < count; i++)
  {
    printf("  [%d] %s\n", i, ports[i]);
    free(ports[i]);
  }
  free(ports);

  loop = g_main_loop_new(NULL, FALSE);

  vtx_soup_session_websocket_connect_async();

  g_main_loop_run(loop);

  if (loop) g_main_loop_unref(loop);
  if (pipeline) gst_object_unref(pipeline);

  g_free(ws1Id);
  g_free(ws2Id);

out:
  return 0;
}
