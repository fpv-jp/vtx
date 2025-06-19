#include "headers/pipeline.h"

static gchar *
build_rtp_vp9_pay (int payload_type)
{
  return g_strdup_printf
      ("vp9parse ! rtpvp9pay pt=%d ! "
      COMMON_QUEUE
      "application/x-rtp,media=video,encoding-name=VP9,payload=%d",
      payload_type, payload_type);
}

// --- vtx_vp9_pipeline ----------------------------------
gchar *
vtx_vp9_pipeline (const MediaParams * params)
{
  gchar *pipeline = NULL;
  gchar *rtp = build_rtp_vp9_pay (params->video_payload_type);
  switch (params->platform) {
    case INTEL_MAC:
    case LINUX_X86:
      pipeline = g_strdup_printf (
        "%s ! " 
        COMMON_QUEUE 
        "vp9enc deadline=1 cpu-used=8 threads=4 lag-in-frames=0 ! "
         "vp9parse ! "
        "%s", 
        params->video_capture, rtp);
      break;
    case JETSON_NANO_2GB:
    // !!! Doesn't actually work
      pipeline = g_strdup_printf ("%s ! "
         COMMON_QUEUE 
         "nvv4l2vp9enc ! " 
         "vp9parse ! "
         "%s", 
         params->video_capture, rtp);
      break;
    case ROCK_B5:
    case RPI4_V4L2:
    case RPI4_LIBCAM:
    case UNKNOWN:
    default:
      break;
  }

  g_free (rtp);
  return pipeline;
}
