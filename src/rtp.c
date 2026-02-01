#include "headers/rtp.h"

#include <gst/rtp/rtp.h>
#include <gst/webrtc/webrtc.h>

const gchar *rtp_video_header_extensions[] = {
    //
    "urn:ietf:params:rtp-hdrext:toffset",
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
    "urn:3gpp:video-orientation",
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
    "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type",
    "http://www.webrtc.org/experiments/rtp-hdrext/video-timing",
    "http://www.webrtc.org/experiments/rtp-hdrext/color-space",
    "urn:ietf:params:rtp-hdrext:sdes:mid",
    "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id",
    "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id"
    //
};

const gchar *rtp_audio_header_extensions[] = {
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level",                                //
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",                 //
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",  //
    "urn:ietf:params:rtp-hdrext:sdes:mid"
    //
};

// Adds a list of RTP header extensions identified by URI to the given payloader element, assigning sequential IDs.
static void vtx_rtp_add_header_extensions_with_list(GstElement *payloader, const gchar **uris, guint count)
{
  guint id = 1;
  for (guint i = 0; i < count; i++)
  {
    const gchar *uri = uris[i];
    GstRTPHeaderExtension *ext = gst_rtp_header_extension_create_from_uri(uri);
    if (!ext)
    {
      gst_printerrln("Failed to create RTP header extension for URI: %s", uri);
      continue;
    }
    gst_rtp_header_extension_set_id(ext, id++);
    g_signal_emit_by_name(payloader, "add-extension", ext);
    g_clear_object(&ext);
  }
}

// Attaches the standard set of WebRTC video RTP header extensions to the given video payloader element.
void vtx_rtp_add_video_header_extensions(GstElement *videopay)
{
  vtx_rtp_add_header_extensions_with_list(videopay, rtp_video_header_extensions, G_N_ELEMENTS(rtp_video_header_extensions));
  g_clear_object(&videopay);
}

// Attaches the standard set of WebRTC audio RTP header extensions to the given audio payloader element.
void vtx_rtp_add_audio_header_extensions(GstElement *audiopay)
{
  vtx_rtp_add_header_extensions_with_list(audiopay, rtp_audio_header_extensions, G_N_ELEMENTS(rtp_audio_header_extensions));
  g_clear_object(&audiopay);
}

// Converts a priority name or nick string to its corresponding GstWebRTCPriorityType enum value.
static GstWebRTCPriorityType vtx_rtp_priority_from_string(const gchar *s)
{
  GEnumClass *klass = (GEnumClass *) g_type_class_ref(GST_TYPE_WEBRTC_PRIORITY_TYPE);
  GEnumValue *en;

  g_return_val_if_fail(klass, 0);

  if (!(en = g_enum_get_value_by_name(klass, s))) en = g_enum_get_value_by_nick(klass, s);
  g_type_class_unref(klass);

  if (en) return en->value;

  return 0;
}

// Sets sendonly direction and optional DSCP priority on the first two WebRTC transceivers for video and audio.
void vtx_rtp_set_transceiver_priority(GArray *transceivers, const gchar *video_priority, const gchar *audio_priority)
{
  GstWebRTCRTPTransceiver *trans = g_array_index(transceivers, GstWebRTCRTPTransceiver *, 0);
  g_object_set(trans, "direction", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);

  if (video_priority)
  {
    GstWebRTCPriorityType priority = vtx_rtp_priority_from_string(video_priority);
    if (priority)
    {
      GstWebRTCRTPSender *sender = NULL;
      g_object_get(trans, "sender", &sender, NULL);
      gst_webrtc_rtp_sender_set_priority(sender, priority);
      g_object_unref(sender);
    }
  }

  trans = g_array_index(transceivers, GstWebRTCRTPTransceiver *, 1);
  g_object_set(trans, "direction", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);

  if (audio_priority)
  {
    GstWebRTCPriorityType priority = vtx_rtp_priority_from_string(audio_priority);
    if (priority)
    {
      GstWebRTCRTPSender *sender = NULL;
      g_object_get(trans, "sender", &sender, NULL);
      gst_webrtc_rtp_sender_set_priority(sender, priority);
      g_object_unref(sender);
    }
  }

  g_array_unref(transceivers);
}
