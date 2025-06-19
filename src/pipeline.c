#include <stdio.h>
#include <string.h>
#include "headers/common.h"
#include "headers/webrtc.h"
#include "headers/data_channel.h"
#include "headers/pipeline.h"
#include "headers/rtp.h"

GstElement *pipe1 = NULL;
GstElement *webrtc1 = NULL;

static void
print_pipeline_pretty (const char *desc)
{
  const char *p = desc;

  while (*p) {
    putchar (*p);
    if (*p == '!') {
      putchar ('\n');
      p++;
      while (*p == ' ')
        p++;
      continue;
    }
    p++;
  }

}

// --- vtx_start_pipeline ----------------------------------
gboolean
vtx_start_pipeline (const MediaParams * params)
{

  gchar *desc = vtx_build_pipeline_description (params);
  if (!desc)
    return FALSE;

  gst_println ("=== Pipeline description ===");
  print_pipeline_pretty (desc);

  GError *error = NULL;
  pipe1 = gst_parse_launch (desc, &error);
  g_free (desc);

  if (!pipe1 || error) {
    gst_printerr ("Pipeline parse error: %s\n", error ? error->message : "unknown");
    g_clear_error (&error);
    return FALSE;
  }

  webrtc1 = gst_bin_get_by_name (GST_BIN (pipe1), "webrtcbin");
  if (!webrtc1) {
    gst_printerr ("webrtcbin not found in pipeline\n");
    return FALSE;
  }

  GstWebRTCICE *agent = GST_WEBRTC_ICE (customice_agent_new ("ice-agent"));
  g_object_set (webrtc1, "ice-agent", agent, NULL);
  g_object_unref (agent);

  // header extensions
  GstElement *videopay = gst_bin_get_by_name (GST_BIN (pipe1), "videopay");
  if (videopay) {
    vtx_add_video_rtp_header_extensions (videopay);
  }

  GstElement *audiopay = gst_bin_get_by_name (GST_BIN (pipe1), "audiopay");
  if (audiopay) {
    vtx_add_audio_rtp_header_extensions (audiopay);
  }
  // set priority
  GArray *transceivers = NULL;
  g_signal_emit_by_name (webrtc1, "get-transceivers", &transceivers);
  if (transceivers && transceivers->len > 1) {
    vtx_set_transceiver_priority (transceivers, params->video_priority, params->audio_priority);
  }
  // callbacks
  g_signal_connect (webrtc1, "on-negotiation-needed", G_CALLBACK (vtx_on_negotiation_needed), NULL);
  g_signal_connect (webrtc1, "on-ice-candidate", G_CALLBACK (vtx_on_ice_candidate), NULL);
  g_signal_connect (webrtc1, "on-data-channel", G_CALLBACK (vtx_on_data_channel), NULL);
  g_signal_connect (webrtc1, "notify::ice-gathering-state", G_CALLBACK (vtx_on_ice_gathering_state_notify), NULL);
  g_signal_connect (webrtc1, "notify::ice-connection-state", G_CALLBACK (vtx_on_ice_connection_state_notify), NULL);

  // set state
  return gst_element_set_state (pipe1, GST_STATE_PLAYING)
      != GST_STATE_CHANGE_FAILURE;
}
