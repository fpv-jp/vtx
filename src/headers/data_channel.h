#pragma once

#include <gst/gst.h>

// ChannelType constants
#define CHANNEL_TYPE_IMU "IMU"
#define CHANNEL_TYPE_GNSS "GNSS"
#define CHANNEL_TYPE_CMD "CMD"

// Command Channel enum
typedef enum
{
  CMD_HANG_UP = 0,
  CMD_PING = 1,
  CMD_PONG = 2,
  CMD_SEND_KEYFRAME_REQUEST = 3,
  CMD_SPS_PPS = 4,
  CMD_ERROR = 9
} CommandType;

void vtx_on_data_channel (GstElement * webrtc, GObject * data_channel, gpointer user_data);

void vtx_create_sender_data_channels (GstElement * webrtc);
