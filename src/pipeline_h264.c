#include "headers/pipeline.h"

static gchar *
build_rtp_h264_pay (int payload_type)
{
  return
      g_strdup_printf
      ("h264parse ! rtph264pay config-interval=-1 aggregate-mode=zero-latency pt=%d ! "
      COMMON_QUEUE
      "application/x-rtp,media=video,encoding-name=H264,payload=%d",
      payload_type, payload_type);
}

// --- vtx_h264_pipeline ----------------------------------
gchar *
vtx_h264_pipeline (const MediaParams * params)
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

  gchar *rtp = build_rtp_h264_pay (params->video_payload_type);

  if (g_strrstr (params->video_capture, "video/x-h264")) {

    pipeline = g_strdup_printf ("%s ! %s", params->video_capture, rtp);

  } else {
    switch (params->platform) {

      case INTEL_MAC:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "vtenc_h264_hw realtime=true ! %s", params->video_capture, rtp);
        break;

      case RPI4_V4L2:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "v4l2h264enc capture-io-mode=dmabuf output-io-mode=dmabuf ! "
            "video/x-h264,level=(string)%s ! %s", params->video_capture, // h264parse
            level_str, rtp);
        break;

      case JETSON_NANO_2GB:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "nvv4l2h264enc preset-level=3 profile=4 bitrate=20000000 ! "
            "video/x-h264,level=(string)%s ! %s", params->video_capture, // h264parse
            level_str, rtp);
        break;

      case ROCK_B5:
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "mpph264enc level=40 profile=100 ! %s", params->video_capture, rtp); // h264parse
        break;

      case LINUX_X86:
// v4l2src device=/dev/video0 do-timestamp=true io-mode=dmabuf ! vaapipostproc ! video/x-raw\(memory:VASurface\),format=NV12,width=640,height=480,framerate=30/1
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "vaapih264enc ! h264parse ! %s", params->video_capture, rtp);
        break;

      case RPI4_LIBCAM:
// libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=30/1,format=YUY2,interlace-mode=progressive
        pipeline = g_strdup_printf ("%s ! "
            COMMON_QUEUE
            "v4l2h264enc extra-controls=\"encode,h264_profile=4,h264_level=12,video_bitrate=20000000\" ! "
            "video/x-h264,level=(string)%s ! %s", params->video_capture,
            level_str, rtp);
        break;

      case UNKNOWN:
      default:
        break;
    }
  }

  g_free (rtp);
  return pipeline;
}
