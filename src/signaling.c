#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <json-glib/json-glib.h>

#include "headers/pipeline.h"
#include "headers/signaling.h"
#include "headers/utils.h"

SoupWebsocketConnection *ws_conn = NULL;
gchar *ws1Id = NULL;
gchar *ws2Id = NULL;

// --- vtx_on_server_message ----------------------------------
void
vtx_on_server_message (SoupWebsocketConnection * conn, SoupWebsocketDataType type, GBytes * message, gpointer user_data)
{
  gsize size;
  const gchar *data = g_bytes_get_data (message, &size);
  gchar *text = g_strndup (data, size);

  // gst_print("[WebSocket-Recv] Size: %lu, Content: %.*s\n", (unsigned long) size, (int) size, data);

  JsonParser *parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, text, -1, NULL)) {
    gst_printerr ("Failed to parse message: %s\n", text);
    g_object_unref (parser);
    g_free (text);
    return;
  }

  JsonNode *root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root)) {
    gst_printerr ("Message is not a JSON object\n");
    g_object_unref (parser);
    g_free (text);
    return;
  }

  JsonObject *object = json_node_get_object (root);
  gint type_value = json_object_get_int_member (object, "type");

  switch (type_value) {

    case SENDER_SESSION_ID_ISSUANCE:
    {
      gst_println (">>> %d SENDER_SESSION_ID_ISSUANCE", SENDER_SESSION_ID_ISSUANCE);

      if (json_object_has_member (object, "sessionId")) {
        ws1Id = g_strdup (json_object_get_string_member (object, "sessionId"));
        gst_println ("assigned session id : %s", ws1Id);
      }

      break;
    }

    case SENDER_MEDIA_DEVICE_LIST_REQUEST:
    {
      gst_println (">>> %d SENDER_MEDIA_DEVICE_LIST_REQUEST", SENDER_MEDIA_DEVICE_LIST_REQUEST);

      if (json_object_has_member (object, "ws2Id")) {

        gchar *ws2Id_ = g_strdup (json_object_get_string_member (object, "ws2Id"));

        JsonObject *vtx_capabilities = json_object_new ();
        json_object_set_string_member (vtx_capabilities, "source", "gstreamer");
        json_object_set_string_member (vtx_capabilities, "platform", vtx_platform_to_string (PLATFORM));
        json_object_set_array_member (vtx_capabilities, "devices", device_list);
        json_object_set_object_member (vtx_capabilities, "codecs", codec_list);

        gst_println ("<<< %d RECEIVER_MEDIA_DEVICE_LIST_RESPONSE", RECEIVER_MEDIA_DEVICE_LIST_RESPONSE);
        vtx_ws_send (ws_conn, RECEIVER_MEDIA_DEVICE_LIST_RESPONSE, ws1Id, ws2Id_, vtx_capabilities);
      }

      break;
    }

    case SENDER_MEDIA_STREAM_START:
    {
      gst_println (">>> %d SENDER_MEDIA_STREAM_START", SENDER_MEDIA_STREAM_START);

      if (!ws2Id && json_object_has_member (object, "ws2Id")) {
        ws2Id = g_strdup (json_object_get_string_member (object, "ws2Id"));
        MediaParams params = { 0 };
        if (!vtx_parse_media_params (object, &params)) {
          vtx_cleanup_and_quit_loop ("Failed to parse mediaParams", PEER_CALL_ERROR);
        }
        if (!vtx_start_pipeline (&params)) {
          vtx_cleanup_and_quit_loop ("Failed to start pipeline", PEER_CALL_ERROR);
        }
      }
      break;
    }

    case SENDER_SDP_ANSWER:
    {
      gst_println (">>> %d SENDER_SDP_ANSWER", SENDER_SDP_ANSWER);
      JsonObject *answer_obj = json_object_get_object_member (object, "answer");
      const gchar *sdp_str = json_object_get_string_member (answer_obj, "sdp");

      GstSDPMessage *sdp;
      if (gst_sdp_message_new (&sdp) != GST_SDP_OK)
        break;
      if (gst_sdp_message_parse_buffer ((guint8 *) sdp_str, strlen (sdp_str), sdp) != GST_SDP_OK) {
        gst_sdp_message_free (sdp);
        break;
      }

      GstWebRTCSessionDescription *desc = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
      GstPromise *promise = gst_promise_new ();
      g_signal_emit_by_name (webrtc1, "set-remote-description", desc, promise);
      gst_promise_interrupt (promise);
      gst_promise_unref (promise);
      gst_webrtc_session_description_free (desc);
      break;
    }

    case SENDER_ICE:
    {
      gst_println (">>> %d SENDER_ICE", SENDER_ICE);

      JsonObject *cand = json_object_get_object_member (object, "candidate");
      const gchar *cand_str = json_object_get_string_member (cand, "candidate");
      gint mline = json_object_get_int_member (cand, "sdpMLineIndex");
      g_signal_emit_by_name (webrtc1, "add-ice-candidate", mline, cand_str);
      break;
    }

    case SENDER_SYSTEM_ERROR:
    {
      gst_println (">>> %d SENDER_SYSTEM_ERROR", SENDER_SYSTEM_ERROR);

      gst_printerr ("%s\n", text);
      break;
    }
    default:
      gst_print ("Unhandled message type: %d\n", type_value);
  }

  g_object_unref (parser);
  g_free (text);

}

// --- vtx_on_server_closed ----------------------------------
static void
vtx_on_server_closed (SoupWebsocketConnection * conn, gpointer user_data)
{
  gst_println ("Disconnected signaling server.");
  app_state = SERVER_CLOSED;
  vtx_cleanup_and_quit_loop ("Server connection closed", 0);
}

// --- vtx_on_server_connected ----------------------------------
static void
vtx_on_server_connected (GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  SoupSession *session = SOUP_SESSION (source_object);
  GError *error = NULL;
  ws_conn = soup_session_websocket_connect_finish (session, res, &error);

  if (error) {
    vtx_cleanup_and_quit_loop (error->message, SERVER_CONNECTION_ERROR);
    g_error_free (error);
    return;
  }

  app_state = SERVER_CONNECTED;
  g_signal_connect (ws_conn, "closed", G_CALLBACK (vtx_on_server_closed), NULL);
  g_signal_connect (ws_conn, "message", G_CALLBACK (vtx_on_server_message), NULL);
}

// --- vtx_soup_session_websocket_connect_async ----------------------------------
void
vtx_soup_session_websocket_connect_async (void)
{
  gst_println ("----- Connect signaling -----");

  SoupMessage *message = soup_message_new (SOUP_METHOD_GET, SIGNALING_ENDPOINT);
  char *protocols[] = { "sender", NULL };
  char *server_ca_cert = realpath ("server-ca-cert.pem", NULL);

  if (!server_ca_cert) {
    g_printerr ("Failed to resolve path to CA cert.\n");
    return;
  }

  gst_println ("[Config] SIGNALING_ENDPOINT: %s", SIGNALING_ENDPOINT);
  gst_println ("Connecting to signaling server...");

#if SOUP_CHECK_VERSION(3, 0, 0)
  // --- libsoup 3.x ---
  GError *error = NULL;
  GTlsDatabase *tls_db = g_tls_file_database_new (server_ca_cert, &error);
  if (!tls_db) {
    g_printerr ("Failed to load CA file: %s\n", error->message);
    g_clear_error (&error);
    free (server_ca_cert);
    return;
  }

  SoupSession *session = soup_session_new_with_options ("tls-database", tls_db, NULL);

  SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
  soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
  g_object_unref (logger);

  soup_session_websocket_connect_async (session, message, NULL, protocols, G_PRIORITY_DEFAULT, NULL, vtx_on_server_connected, NULL);

#else
  // --- libsoup 2.4 ---
  SoupSession *session = soup_session_new_with_options (SOUP_SESSION_SSL_CA_FILE, server_ca_cert, NULL);

  SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
  soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
  g_object_unref (logger);

  soup_session_websocket_connect_async (session, message, NULL, protocols, NULL, (GAsyncReadyCallback) vtx_on_server_connected, message);

#endif

  free (server_ca_cert);
  app_state = SERVER_CONNECTING;
}
