#pragma once

#include <gst/gst.h>
#include <json-glib/json-glib.h>

#define STUN_SERVER "stun://stun.l.google.com:19302"
#define COMMON_QUEUE "queue max-size-buffers=1 leaky=downstream ! "

typedef enum
{
  UNKNOWN = 0,
  INTEL_MAC,
  RPI4_V4L2,
  JETSON_NANO_2GB,
  ROCK_B5,
  LINUX_X86,
  RPI4_LIBCAM,
} PlatformType;

typedef struct
{
  const gchar *video_capture;
  const gchar *video_encoder;
  const gchar *audio_sampling;
  const gchar *audio_encoder;
  const gchar *video_priority;
  const gchar *audio_priority;
  guint video_payload_type;
  guint audio_payload_type;
  PlatformType platform;
} MediaParams;

gchar *vtx_platform_to_string (PlatformType platform);

gboolean vtx_parse_media_params (JsonObject * root_obj, MediaParams * mediaParams);

gchar *vtx_build_pipeline_description (const MediaParams * params);

gchar *vtx_h264_pipeline (const MediaParams * params);

gchar *vtx_h265_pipeline (const MediaParams * params);

gchar *vtx_vp8_pipeline (const MediaParams * params);

gchar *vtx_vp9_pipeline (const MediaParams * params);

gchar *vtx_av1_pipeline (const MediaParams * params);

gchar *vtx_opus_pipeline (const MediaParams * params);

gchar *vtx_pcmu_pipeline (const MediaParams * params);

gboolean vtx_start_pipeline (const MediaParams * params);
