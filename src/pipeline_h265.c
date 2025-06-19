#include "headers/pipeline.h"

static gchar *
build_rtp_h265_pay (int payload_type)
{
  return
      g_strdup_printf
      ("h265parse ! rtph265pay config-interval=-1 aggregate-mode=zero-latency pt=%d ! "
      COMMON_QUEUE
      "application/x-rtp,media=video,encoding-name=H265,payload=%d",
      payload_type, payload_type);
}

// --- vtx_h265_pipeline ----------------------------------
gchar *
vtx_h265_pipeline (const MediaParams * params)
{
  GRegex *regex = g_regex_new ("height=(\\d+)", 0, 0, NULL);
  GMatchInfo *match_info = NULL;
  gint height = 0;
  gchar *level_str = NULL;

  g_regex_match (regex, params->video_capture, 0, &match_info);
  if (g_match_info_matches (match_info)) {
    gchar *height_str = g_match_info_fetch (match_info, 1);
    height = atoi (height_str);
    g_free (height_str);
  }

  g_match_info_free (match_info);
  g_regex_unref (regex);

  if (height > 1080) {
    level_str = "5.1";
  } else {
    level_str = "4.2";
  }

  gchar *pipeline = NULL;

  gchar *rtp = build_rtp_h265_pay (params->video_payload_type);

  if (g_strrstr (params->video_capture, "video/x-h265")) {

    pipeline = g_strdup_printf ("%s ! %s", params->video_capture, rtp);

  } else {
    switch (params->platform) {

      case INTEL_MAC:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "vtenc_h265_hw realtime=true allow-frame-reordering=false ! %s", params->video_capture, rtp);
        break;

      case JETSON_NANO_2GB:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "nvv4l2h265enc preset-level=3 profile=0 bitrate=30000000 ! "
            "video/x-h265,level=(string)%s ! %s", params->video_capture,
            level_str, rtp);
        break;

      case ROCK_B5:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "mpph265enc level=40 profile=100 ! %s", params->video_capture, rtp);
        break;

      case LINUX_X86:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "vaapih265enc ! h265parse ! %s", params->video_capture, rtp);
        break;

      case RPI4_V4L2:
      case RPI4_LIBCAM:
      case UNKNOWN:
      default:
        break;
    }
  }

  g_free (rtp);
  return pipeline;
}
