#pragma once

#include <gst/gst.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

typedef enum
{
  UNKNOWN = 0,
  INTEL_MAC,
  LINUX_X86,
  RPI4_V4L2,
  JETSON_NANO_2GB,
  JETSON_ORIN_NANO_SUPER,
  RADXA_ROCK_5B,
  RADXA_ROCK_5T,
  RPI4_LIBCAM,
  RPI5_LIBCAM,
} PlatformType;

typedef enum
{
  GPU_VENDOR_UNKNOWN = 0,
  GPU_VENDOR_INTEL,
  GPU_VENDOR_AMD,
  GPU_VENDOR_NVIDIA,
} GpuVendor;

typedef enum AppState
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_REGISTERED_ERROR,
  SERVER_CONNECTED,
  SERVER_REGISTERED,
  SERVER_CLOSED,

  STREAM_REQUEST_ACCEPT = 3000,

  PEER_CALL_NEGOTIATING = 4000,
  PEER_CALL_SEND_OFFER,
  PEER_CALL_RECIVE_ANSWER,

} AppState;

#define SENDER_SESSION_ID_ISSUANCE 100
#define SENDER_MEDIA_DEVICE_LIST_REQUEST 101
#define SENDER_MEDIA_STREAM_START 102
#define SENDER_SDP_ANSWER 103
#define SENDER_ICE 104
#define SENDER_RECEIVER_CLOSE 108
#define SENDER_SYSTEM_ERROR 109

#define RECEIVER_SESSION_ID_ISSUANCE 200
#define RECEIVER_CHANGE_SENDER_ENTRIES 201
#define RECEIVER_MEDIA_DEVICE_LIST_RESPONSE 202
#define RECEIVER_SDP_OFFER 203
#define RECEIVER_ICE 204
#define RECEIVER_SYSTEM_ERROR 209

// External globals
extern GMainLoop *loop;
extern AppState app_state;
extern SoupWebsocketConnection *ws_conn;
extern gchar *ws1Id, *ws2Id;
extern GstElement *pipeline, *webrtc;

extern JsonArray *device_list;
extern JsonObject *codec_list;

// Platform detection (runtime)
extern PlatformType g_platform;

// GPU vendor detection (runtime, LINUX_X86 only)
extern GpuVendor g_gpu_vendor;
