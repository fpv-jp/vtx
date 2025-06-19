#pragma once

#include <gst/gst.h>

void vtx_add_video_rtp_header_extensions (GstElement * videopay);

void vtx_add_audio_rtp_header_extensions (GstElement * audiopay);

void vtx_set_transceiver_priority (GArray * transceivers, const gchar * video_priority, const gchar * audio_priority);
