#include <gst/webrtc/webrtc.h>

#include "headers/data_channel.h"
#include "headers/utils.h"

// --- vtx_send_sdp_offer ----------------------------------
static void
vtx_send_sdp_offer (GstWebRTCSessionDescription * desc)
{
  gchar *sdp = gst_sdp_message_as_text (desc->sdp);
  JsonObject *offer = json_object_new ();
  json_object_set_string_member (offer, "type", "offer");
  json_object_set_string_member (offer, "sdp", sdp);

  JsonObject *msg = json_object_new ();
  json_object_set_object_member (msg, "offer", offer);

  gst_println ("<<< %d RECEIVER_SDP_OFFER", RECEIVER_SDP_OFFER);
  vtx_ws_send (ws_conn, RECEIVER_SDP_OFFER, ws1Id, ws2Id, msg);
  g_free (sdp);
  json_object_unref (msg);
}

// --- on_ice_candidate ----------------------------------
void
vtx_on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate, gpointer user_data)
{
  JsonObject *ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);

  JsonObject *msg = json_object_new ();
  json_object_set_object_member (msg, "candidate", ice);

  gst_println ("<<< %d RECEIVER_ICE", RECEIVER_ICE);
  vtx_ws_send (ws_conn, RECEIVER_ICE, ws1Id, ws2Id, msg);
  json_object_unref (msg);
}

// --- vtx_on_ice_gathering_state_notify ----------------------------------
void
vtx_on_ice_gathering_state_notify (GstElement * webrtc, GParamSpec * pspec, gpointer user_data)
{
  GstWebRTCICEGatheringState state;
  g_object_get (webrtc, "ice-gathering-state", &state, NULL);
  const gchar *state_str = "unknown";
  switch (state) {
    case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
      state_str = "new";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
      state_str = "gathering";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
      state_str = "complete";
      break;
  }
  gst_print ("--- ICE gathering state: %s\n", state_str);
}

// --- vtx_on_ice_connection_state_notify ----------------------------------
void
vtx_on_ice_connection_state_notify (GstElement * webrtc, GParamSpec * pspec, gpointer user_data)
{
  GstWebRTCICEConnectionState state;
  g_object_get (webrtc, "ice-connection-state", &state, NULL);
  const gchar *state_str = "unknown";
  switch (state) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_NEW:
      state_str = "new";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING:
      state_str = "checking";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED:
      state_str = "connected";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED:
      state_str = "completed";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED:
      state_str = "failed";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED:
      state_str = "disconnected";
      break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED:
      state_str = "closed";
      break;
  }
  gst_print ("--- ICE connection state: %s\n", state_str);
}

// --- vtx_on_offer_created ----------------------------------
static void
vtx_on_offer_created (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

  GstPromise *local_desc_promise = gst_promise_new ();
  g_signal_emit_by_name (webrtc1, "set-local-description", offer, local_desc_promise);
  gst_promise_interrupt (local_desc_promise);
  gst_promise_unref (local_desc_promise);

  vtx_send_sdp_offer (offer);
  gst_webrtc_session_description_free (offer);
}

// --- vtx_on_negotiation_needed ----------------------------------
void
vtx_on_negotiation_needed (GstElement * element, gpointer user_data)
{
  app_state = PEER_CALL_NEGOTIATING;
  // create data channel
  vtx_create_sender_data_channels (webrtc1);
  GstPromise *promise = gst_promise_new_with_change_func (vtx_on_offer_created, NULL, NULL);
  g_signal_emit_by_name (webrtc1, "create-offer", NULL, promise);
}
