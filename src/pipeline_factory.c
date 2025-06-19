#include "headers/pipeline.h"

// --- vtx_platform_to_string ----------------------------------
gchar *
vtx_platform_to_string (PlatformType platform)
{
  switch (platform) {
    case INTEL_MAC:
      return "INTEL_MAC";
    case RPI4_V4L2:
      return "RPI4_V4L2";
    case JETSON_NANO_2GB:
      return "JETSON_NANO_2GB";
    case ROCK_B5:
      return "ROCK_B5";
    case RPI4_LIBCAM:
      return "RPI4_LIBCAM";
    case LINUX_X86:
      return "LINUX_X86";
    default:
      return "UNKNOWN";
  }
}

// --- vtx_parse_media_params ----------------------------------
gboolean
vtx_parse_media_params (JsonObject * o, MediaParams * p)
{
  p->video_capture = json_object_get_string_member (o, "video_capture");
  p->video_encoder = json_object_get_string_member (o, "video_encoder");
  p->audio_sampling = json_object_get_string_member (o, "audio_sampling");
  p->audio_encoder = json_object_get_string_member (o, "audio_encoder");
  p->video_priority = json_object_get_string_member (o, "video_priority");
  p->audio_priority = json_object_get_string_member (o, "audio_priority");
  p->video_payload_type = json_object_get_int_member (o, "video_payload_type");
  p->audio_payload_type = json_object_get_int_member (o, "audio_payload_type");
  p->platform = PLATFORM;

  gst_println ("=== MediaParams parsed ===");
  gst_println ("MediaParams {");
  gst_println ("  video_capture: %s", p->video_capture ? p->video_capture : "NULL");
  gst_println ("  video_encoder: %s", p->video_encoder ? p->video_encoder : "NULL");
  gst_println ("  audio_sampling: %s", p->audio_sampling ? p->audio_sampling : "NULL");
  gst_println ("  audio_encoder: %s", p->audio_encoder ? p->audio_encoder : "NULL");
  gst_println ("  video_priority: %s", p->video_priority ? p->video_priority : "NULL");
  gst_println ("  audio_priority: %s", p->audio_priority ? p->audio_priority : "NULL");
  gst_println ("  video_payload_type: %u", p->video_payload_type);
  gst_println ("  audio_payload_type: %u", p->audio_payload_type);
  gst_println ("  platform: %d", p->platform);
  gst_println ("}");

  return TRUE;
}

// --- vtx_build_pipeline_description ----------------------------------
gchar *
vtx_build_pipeline_description (const MediaParams * p)
{
  gchar *desc = NULL;
  gchar *video_pipeline = NULL;
  gchar *audio_pipeline = NULL;

  if (g_strstr_len (p->video_encoder, -1, "264") != NULL) {
    video_pipeline = vtx_h264_pipeline (p);
  } else if (g_strstr_len (p->video_encoder, -1, "265") != NULL) {
    video_pipeline = vtx_h265_pipeline (p);
  } else if (g_strstr_len (p->video_encoder, -1, "vp8") != NULL) {
    video_pipeline = vtx_vp8_pipeline (p);
  } else if (g_strstr_len (p->video_encoder, -1, "vp9") != NULL) {
    video_pipeline = vtx_vp9_pipeline (p);
  } else if (g_strstr_len (p->video_encoder, -1, "av1") != NULL) {
    video_pipeline = vtx_av1_pipeline (p);
  }

  if (g_strcmp0 (p->audio_encoder, "opusenc") == 0) {
    audio_pipeline = vtx_opus_pipeline (p);
  } else if (g_strcmp0 (p->audio_encoder, "mulawenc") == 0) {
    audio_pipeline = vtx_pcmu_pipeline (p);
  }

  if (video_pipeline && audio_pipeline) {

    desc =
        g_strdup_printf
        ("webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
        "stun-server=stun://stun.l.google.com:19302 " 
        "%s ! webrtcbin. "
        "%s ! webrtcbin.", 
        video_pipeline, 
        audio_pipeline);

  } else if (video_pipeline) {

    desc =
        g_strdup_printf
        ("webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
        "stun-server=stun://stun.l.google.com:19302 " 
        "%s ! webrtcbin.",
        video_pipeline);

  } else if (audio_pipeline) {

    desc =
        g_strdup_printf
        ("webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
        "stun-server=stun://stun.l.google.com:19302 "
        "%s ! webrtcbin.",
        audio_pipeline);

  }

  g_free (video_pipeline);
  g_free (audio_pipeline);
  return desc;
}
