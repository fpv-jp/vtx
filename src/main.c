#include <stdio.h>

#include "headers/common.h"
#include "headers/data_channel.h"
#include "headers/msp.h"
#include "headers/signaling.h"
#include "headers/utils.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

static void vtx_print_handler(const gchar *msg)
{
  fputs(msg, stdout);
  vtx_dc_notify_message_send(msg);
}

static void vtx_printerr_handler(const gchar *msg)
{
  fputs(msg, stderr);
  vtx_dc_notify_message_send(msg);
}

// GStreamer 内部デバッグログ (GST_DEBUG) をフックして vrx に転送する。
// WARNING 以上のみを対象とし、デフォルトのターミナル出力は維持する。
static void vtx_gst_log_handler(GstDebugCategory *category, GstDebugLevel level,
                                 const gchar *file, const gchar *function, gint line,
                                 GObject *object, GstDebugMessage *message, gpointer user_data)
{
  gst_debug_log_default(category, level, file, function, line, object, message, user_data);

  if (level > GST_LEVEL_WARNING) return;

  const gchar *text = gst_debug_message_get(message);
  gchar *formatted = g_strdup_printf("[%s:%s] %s\n",
      gst_debug_level_get_name(level),
      gst_debug_category_get_name(category),
      text);
  vtx_dc_notify_message_send(formatted);
  g_free(formatted);
}

// Initializes GStreamer, detects platform and GPU, starts the MSP port scan, connects to the signaling server, and runs the GLib main loop.
int main(int argc, char *argv[])
{
  gst_debug_set_default_threshold(GST_LEVEL_FIXME);
  gst_init(&argc, &argv);

  g_set_print_handler(vtx_print_handler);
  g_set_printerr_handler(vtx_printerr_handler);

  // デフォルトログ関数を vtx_gst_log_handler に置き換える（二重出力を防ぐため削除してから追加）
  gst_debug_remove_log_function(gst_debug_log_default);
  gst_debug_add_log_function(vtx_gst_log_handler, NULL, NULL);

  // Detect platform at runtime
  vtx_detect_platform();

  // Detect GPU vendor for LINUX_X86
  vtx_detect_gpu_vendor();

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
