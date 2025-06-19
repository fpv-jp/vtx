#include "headers/pipeline.h"

static gchar *
build_rtp_vp8_pay (int payload_type)
{
  return g_strdup_printf
      ("rtpvp8pay pt=%d ! "
      COMMON_QUEUE
      "application/x-rtp,media=video,encoding-name=VP8,payload=%d",
      payload_type, payload_type);
}

// --- vtx_vp8_pipeline ----------------------------------
gchar *
vtx_vp8_pipeline (const MediaParams * params)
{
  gchar *pipeline = NULL;
  gchar *rtp = build_rtp_vp8_pay (params->video_payload_type);
  switch (params->platform) {
    case INTEL_MAC:
    case LINUX_X86:
      pipeline = g_strdup_printf ("%s ! " COMMON_QUEUE "vp8enc deadline=1 ! " "%s", params->video_capture, rtp);
      break;
    case JETSON_NANO_2GB:
      pipeline = g_strdup_printf ("%s ! " COMMON_QUEUE "nvv4l2vp8enc ! " "%s", params->video_capture, rtp);
      break;
    case ROCK_B5:
      pipeline = g_strdup_printf ("%s ! " COMMON_QUEUE "mppvp8enc ! " "%s", params->video_capture, rtp);
      break;
    case RPI4_V4L2:
    case RPI4_LIBCAM:
    case UNKNOWN:
    default:
      break;
  }

  g_free (rtp);
  return pipeline;
}
