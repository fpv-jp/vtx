#pragma once

#include <gst/gst.h>
#include <json-glib/json-glib.h>

#include "utils.h"
#include "webrtc.h"

#define STUN_SERVER "stun://stun.l.google.com:19302"

typedef struct
{
  const gchar *video_pipeline;
  const gchar *audio_pipeline;
  const gchar *video_priority;
  const gchar *audio_priority;
  guint video_payload_type;
  guint audio_payload_type;
  PlatformType platform;
  const gchar *network_interface;
  const gchar *video_profile;
  const gchar *flight_controller;
} MediaParams;

gboolean vtx_pipeline_parse_media_params(JsonObject *root_obj, MediaParams *mediaParams);

GstElement *vtx_pipeline_build(const MediaParams *params);

gboolean vtx_pipeline_start(const MediaParams *params);
