#pragma once

#include <gst/gst.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

typedef enum AppState
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED,
  // SERVER_REGISTERING,
  // SERVER_REGISTRATION_ERROR,
  // SERVER_REGISTERED,
  SERVER_CLOSED,
  // PEER_CONNECTING = 3000,
  // PEER_CONNECTION_ERROR,
  // PEER_CONNECTED,
  PEER_CALL_NEGOTIATING = 4000,
  // PEER_CALL_STARTED,
  // PEER_CALL_STOPPING,
  // PEER_CALL_STOPPED,
  PEER_CALL_ERROR,
} AppState;

#define SENDER_SESSION_ID_ISSUANCE 100
#define SENDER_MEDIA_DEVICE_LIST_REQUEST 101
#define SENDER_MEDIA_STREAM_START 102
#define SENDER_SDP_ANSWER 103
#define SENDER_ICE 104
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
extern GstElement *pipe1, *webrtc1;

extern JsonArray *device_list;
extern JsonObject *codec_list;
