#include "headers/pipeline.h"

#include "headers/common.h"
#include "headers/data_channel.h"
#include "headers/rtp.h"
#include "headers/webrtc.h"

GstElement *pipeline = NULL;
GstElement *webrtc = NULL;

// --- vtx_pipeline_start ----------------------------------
gboolean vtx_pipeline_start(const MediaParams *params)
{
  pipeline = vtx_pipeline_build(params);
  if (!pipeline) return FALSE;

  webrtc = gst_bin_get_by_name(GST_BIN(pipeline), "webrtcbin");
  if (!webrtc)
  {
    gst_printerrln("webrtcbin not found in pipeline");
    return FALSE;
  }

  // GstWebRTCICE *agent = GST_WEBRTC_ICE(customice_agent_new("ice-agent"));
  // g_object_set(webrtc, "ice-agent", agent, NULL);
  // g_object_unref(agent);

  // header extensions
  GstElement *videopay = gst_bin_get_by_name(GST_BIN(pipeline), "videopay");
  if (videopay)
  {
    vtx_rtp_add_video_header_extensions(videopay);
  }

  GstElement *audiopay = gst_bin_get_by_name(GST_BIN(pipeline), "audiopay");
  if (audiopay)
  {
    vtx_rtp_add_audio_header_extensions(audiopay);
  }

  // set priority
  GArray *transceivers = NULL;
  g_signal_emit_by_name(webrtc, "get-transceivers", &transceivers);
  if (transceivers && transceivers->len > 1)
  {
    vtx_rtp_set_transceiver_priority(transceivers, params->video_priority, params->audio_priority);
  }

  // callbacks
  g_signal_connect(webrtc, "on-negotiation-needed", G_CALLBACK(vtx_webrtc_on_negotiation_needed), NULL);
  g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(vtx_webrtc_on_ice_candidate), NULL);
  g_signal_connect(webrtc, "on-data-channel", G_CALLBACK(vtx_webrtc_on_data_channel), NULL);
  g_signal_connect(webrtc, "notify::ice-gathering-state", G_CALLBACK(vtx_webrtc_notify_ice_gathering_state), NULL);
  g_signal_connect(webrtc, "notify::ice-connection-state", G_CALLBACK(vtx_webrtc_notify_ice_connection_state), NULL);

  // set state
  return gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
}
