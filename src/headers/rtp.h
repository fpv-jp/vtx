#pragma once

#include <gst/gst.h>

void vtx_rtp_add_video_header_extensions(GstElement* videopay);

void vtx_rtp_add_audio_header_extensions(GstElement* audiopay);

void vtx_rtp_set_transceiver_priority(GArray* transceivers, const gchar* video_priority, const gchar* audio_priority);
