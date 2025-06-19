#include "headers/pipeline.h"

// --- vtx_av1_pipeline ----------------------------------
gchar *
vtx_av1_pipeline (const MediaParams * params)
{
  gchar *pipeline = NULL;
  switch (params->platform) {
    case INTEL_MAC:
    case LINUX_X86:
      pipeline = g_strdup_printf ("%s ! "
          "videoconvert ! "
          "video/x-raw,format=I420 ! "
          "queue max-size-buffers=1 leaky=downstream ! "
          "svtav1enc ! "
          "av1parse ! "
          "rtpav1pay pt=%d ! "
          "application/x-rtp,media=video,encoding-name=AV1,payload=%d",
          params->video_capture,
          params->video_payload_type, params->video_payload_type);
      break;
    case RPI4_V4L2:
    case JETSON_NANO_2GB:
    case ROCK_B5:
    case RPI4_LIBCAM:
    case UNKNOWN:
    default:
      pipeline = NULL;
      break;
  }
  return pipeline;
}
