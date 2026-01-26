#include "headers/signaling.h"

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <json-glib/json-glib.h>

#include "headers/data_channel.h"
#include "headers/inspection.h"
#include "headers/msp.h"
#include "headers/pipeline.h"
#include "headers/utils.h"
#include "headers/wpa.h"

SoupWebsocketConnection *ws_conn = NULL;
gchar *ws1Id = NULL;
gchar *ws2Id = NULL;

// --- vtx_sdp_media_is_rejected ----------------------------------
static gboolean vtx_sdp_media_is_rejected(const GstSDPMedia *media)
{
  if (!media) return FALSE;

  if (gst_sdp_media_get_port(media) == 0) return TRUE;

  const gchar *inactive = gst_sdp_media_get_attribute_val(media, "inactive");
  return inactive != NULL;
}

// --- vtx_sdp_handle_rejected_media ----------------------------------
static gboolean vtx_sdp_handle_rejected_media(const GstSDPMessage *sdp)
{
  if (!sdp) return FALSE;

  guint media_count = gst_sdp_message_medias_len(sdp);
  gboolean video_rejected = FALSE;
  gboolean audio_rejected = FALSE;

  for (guint i = 0; i < media_count; i++)
  {
    const GstSDPMedia *media = gst_sdp_message_get_media(sdp, i);
    if (!vtx_sdp_media_is_rejected(media)) continue;

    const gchar *media_type = gst_sdp_media_get_media(media);
    if (g_strcmp0(media_type, "video") == 0)
    {
      video_rejected = TRUE;
    }
    else if (g_strcmp0(media_type, "audio") == 0)
    {
      audio_rejected = TRUE;
    }
  }

  if (!video_rejected && !audio_rejected) return FALSE;

  GString *err_msg = g_string_new("Peer rejected ");
  if (video_rejected && audio_rejected)
  {
    g_string_append(err_msg, "both video and audio tracks. The viewer likely lacks the required codecs (H264 video / Opus audio).");
  }
  else if (video_rejected)
  {
    g_string_append(err_msg, "the video track. Ensure the viewer supports H264 decoding.");
  }
  else
  {
    g_string_append(err_msg, "the audio track. Ensure the viewer supports Opus decoding.");
  }

  gst_printerrln("%s", err_msg->str);

  if (ws2Id)
  {
    JsonObject *error_obj = json_object_new();
    json_object_set_string_member(error_obj, "message", err_msg->str);
    vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id, error_obj);
    json_object_unref(error_obj);
  }

  vtx_cleanup_connection("Remote SDP rejected offered media");

  g_string_free(err_msg, TRUE);
  return TRUE;
}

// --- vtx_soup_on_message ----------------------------------
void vtx_soup_on_message(SoupWebsocketConnection *conn, SoupWebsocketDataType type, GBytes *message, gpointer user_data)
{
  gsize size;
  const gchar *data = g_bytes_get_data(message, &size);
  gchar *text = g_strndup(data, size);

  // gst_println("[WebSocket-Recv] Size: %lu, Content: %.*s", (unsigned long) size, (int) size, data);

  JsonParser *parser = json_parser_new();
  if (!json_parser_load_from_data(parser, text, -1, NULL))
  {
    gst_printerrln("Failed to parse message: %s", text);
    g_object_unref(parser);
    g_free(text);
    return;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root))
  {
    gst_printerrln("Message is not a JSON objects");
    g_object_unref(parser);
    g_free(text);
    return;
  }

  JsonObject *object = json_node_get_object(root);
  gint type_value = json_object_get_int_member(object, "type");

  switch (type_value)
  {
    case SENDER_SESSION_ID_ISSUANCE:
    {
      gst_println(">>> %d SENDER_SESSION_ID_ISSUANCE", SENDER_SESSION_ID_ISSUANCE);
      if (json_object_has_member(object, "sessionId"))
      {
        ws1Id = g_strdup(json_object_get_string_member(object, "sessionId"));
        gst_println("assigned TX session ID : %s", ws1Id);
        app_state = SERVER_REGISTERED;
      }
      else
      {
        app_state = SERVER_REGISTERED_ERROR;
      }
      break;
    }

    case SENDER_MEDIA_DEVICE_LIST_REQUEST:
    {
      gst_println(">>> %d SENDER_MEDIA_DEVICE_LIST_REQUEST", SENDER_MEDIA_DEVICE_LIST_REQUEST);
      if (json_object_has_member(object, "ws2Id"))
      {
        gchar *ws2Id_ = g_strdup(json_object_get_string_member(object, "ws2Id"));

        JsonObject *vtx_capabilities = json_object_new();

        json_object_set_string_member(vtx_capabilities, "source", "gstreamer");
        json_object_set_string_member(vtx_capabilities, "platform", vtx_platform_to_string(g_platform));

        JsonArray *network_interfaces = vtx_nic_inspection();
        // gst_println("----- Network Interfaces capabilities -----");
        // print_json_array(network_interfaces);

        JsonArray *device_capabilities = vtx_device_load_launch_entries();
        // gst_println("----- Media Device capabilities -----");
        // print_json_array(device_capabilities);

        JsonObject *codecs_capabilities = vtx_supported_codec_inspection();
        // gst_println("----- Supported Codecs capabilities -----");
        // print_json_object(codecs_capabilities);

        JsonArray *flight_controllers = vtx_msp_flight_controller();
        // gst_println("----- Flight Controllers capabilities -----");
        // print_json_array(flight_controllers);

        json_object_set_array_member(vtx_capabilities, "network", network_interfaces);
        json_object_set_array_member(vtx_capabilities, "devices", device_capabilities);
        json_object_set_object_member(vtx_capabilities, "codecs", codecs_capabilities);
        json_object_set_array_member(vtx_capabilities, "flight_controllers", flight_controllers);

        gst_println("<<< %d RECEIVER_MEDIA_DEVICE_LIST_RESPONSE", RECEIVER_MEDIA_DEVICE_LIST_RESPONSE);
        vtx_ws_send(ws_conn, RECEIVER_MEDIA_DEVICE_LIST_RESPONSE, ws1Id, ws2Id_, vtx_capabilities);
      }
      break;
    }

    case SENDER_MEDIA_STREAM_START:
    {
      gst_println(">>> %d SENDER_MEDIA_STREAM_START", SENDER_MEDIA_STREAM_START);

      if (json_object_has_member(object, "ws2Id"))
      {
        // Clear old ws2Id if exists (for reconnection)
        if (ws2Id)
        {
          gst_println("Clearing old ws2Id for reconnection: %s", ws2Id);
          g_free(ws2Id);
        }

        ws2Id = g_strdup(json_object_get_string_member(object, "ws2Id"));
        gst_println("Starting stream with ws2Id: %s", ws2Id);

        MediaParams params = {0};
        gchar *ws2Id_ = g_strdup(json_object_get_string_member(object, "ws2Id"));
        if (!vtx_pipeline_parse_media_params(object, &params))
        {
          JsonObject *error_messeage = json_object_new();
          json_object_set_string_member(error_messeage, "message", "Failed to parse mediaParams");
          vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id_, error_messeage);
          g_free(ws2Id_);
        }
        else if (params.network_interface && !vtx_wpa_supplicant_init(params.network_interface))
        {
          JsonObject *error_messeage = json_object_new();
          json_object_set_string_member(error_messeage, "message", "Failed to initialize WPA supplicant");
          vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id_, error_messeage);
          g_free(ws2Id_);
        }
        else if (params.flight_controller)
        {
          // Cleanup existing MSP connection if any
          vtx_msp_cleanup_global();

          // Open new MSP connection with selected flight controller
          MSP *msp = g_malloc(sizeof(MSP));
          if (vtx_msp_init(msp, params.flight_controller, B115200) == 1)
          {
            vtx_msp_set_global(msp);
            gst_println("Flight controller opened: %s", params.flight_controller);

            if (!vtx_pipeline_start(&params))
            {
              JsonObject *error_messeage = json_object_new();
              json_object_set_string_member(error_messeage, "message", "Failed to start pipeline");
              vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id_, error_messeage);
              g_free(ws2Id_);
            }
            else
            {
              app_state = STREAM_REQUEST_ACCEPT;
              g_free(ws2Id_);
            }
          }
          else
          {
            g_free(msp);
            JsonObject *error_messeage = json_object_new();
            gchar *error_msg = g_strdup_printf("Failed to open flight controller: %s", params.flight_controller);
            json_object_set_string_member(error_messeage, "message", error_msg);
            vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id_, error_messeage);
            g_free(error_msg);
            g_free(ws2Id_);
          }
        }
        else if (!vtx_pipeline_start(&params))
        {
          JsonObject *error_messeage = json_object_new();
          json_object_set_string_member(error_messeage, "message", "Failed to start pipeline");
          vtx_ws_send(ws_conn, RECEIVER_SYSTEM_ERROR, ws1Id, ws2Id_, error_messeage);
          g_free(ws2Id_);
        }
        else
        {
          app_state = STREAM_REQUEST_ACCEPT;
          g_free(ws2Id_);
        }
      }
      else
      {
        gst_printerrln("SENDER_MEDIA_STREAM_START: ws2Id not found in message");
      }
      break;
    }

    case SENDER_SDP_ANSWER:
    {
      gst_println(">>> %d SENDER_SDP_ANSWER", SENDER_SDP_ANSWER);
      JsonObject *answer_obj = json_object_get_object_member(object, "answer");
      const gchar *sdp_str = json_object_get_string_member(answer_obj, "sdp");
      GstSDPMessage *sdp;
      if (gst_sdp_message_new(&sdp) != GST_SDP_OK) break;
      if (gst_sdp_message_parse_buffer((guint8 *) sdp_str, strlen(sdp_str), sdp) != GST_SDP_OK)
      {
        gst_sdp_message_free(sdp);
        break;
      }
      if (vtx_sdp_handle_rejected_media(sdp))
      {
        gst_sdp_message_free(sdp);
        break;
      }
      GstWebRTCSessionDescription *desc = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
      GstPromise *promise = gst_promise_new();
      g_signal_emit_by_name(webrtc, "set-remote-description", desc, promise);
      gst_promise_interrupt(promise);
      app_state = PEER_CALL_RECIVE_ANSWER;
      gst_promise_unref(promise);
      gst_webrtc_session_description_free(desc);
      break;
    }

    case SENDER_ICE:
    {
      JsonObject *cand = json_object_get_object_member(object, "candidate");
      const gchar *cand_str = json_object_get_string_member(cand, "candidate");
      gint mline = json_object_get_int_member(cand, "sdpMLineIndex");
      gst_println(">>> %d SENDER_ICE: %s", SENDER_ICE, cand_str);
      if (webrtc)
      {
        g_signal_emit_by_name(webrtc, "add-ice-candidate", mline, cand_str);
      }
      else
      {
        gst_printerrln("Ignoring ICE candidate because WebRTC pipeline has been cleaned up");
      }
      break;
    }

    case SENDER_RECEIVER_CLOSE:
    {
      gst_println(">>> %d SENDER_RECEIVER_CLOSE", SENDER_RECEIVER_CLOSE);
      vtx_cleanup_connection("Receiver closed connection");
      break;
    }

    case SENDER_SYSTEM_ERROR:
    {
      gst_println(">>> %d SENDER_SYSTEM_ERROR", SENDER_SYSTEM_ERROR);
      gst_printerrln("%s", text);
      break;
    }

    default:
      gst_println("Unhandled message type: %d", type_value);
  }

  g_object_unref(parser);
  g_free(text);
}

// --- vtx_soup_on_closed ----------------------------------
static void vtx_soup_on_closed(SoupWebsocketConnection *conn, gpointer user_data)
{
  gst_println("Disconnected signaling server.");
  vtx_cleanup_and_quit_loop("Server connection closed", 0);
  app_state = SERVER_CLOSED;
}

// --- vtx_soup_on_connected ----------------------------------
static void vtx_soup_on_connected(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  SoupSession *session = SOUP_SESSION(source_object);
  GError *error = NULL;
  ws_conn = soup_session_websocket_connect_finish(session, res, &error);

  if (error)
  {
    vtx_cleanup_and_quit_loop(error->message, SERVER_CONNECTION_ERROR);
    g_error_free(error);
    return;
  }

  app_state = SERVER_CONNECTED;

  g_signal_connect(ws_conn, "closed", G_CALLBACK(vtx_soup_on_closed), NULL);
  g_signal_connect(ws_conn, "message", G_CALLBACK(vtx_soup_on_message), NULL);
}

// --- get_signaling_endpoint ----------------------------------
const char *get_signaling_endpoint(void)
{
  const char *endpoint = getenv("SIGNALING_ENDPOINT");
  return endpoint ? endpoint : DEFAULT_SIGNALING_ENDPOINT;
}

// --- get_certificate_authority ----------------------------------
const char *get_certificate_authority(void)
{
  const char *certificate = getenv("SERVER_CERTIFICATE_AUTHORITY");
  return certificate ? certificate : DEFAULT_SERVER_CERTIFICATE_AUTHORITY;
}

// --- vtx_soup_session_websocket_connect_async ----------------------------------
void vtx_soup_session_websocket_connect_async(void)
{
  gst_println("----- Connect signaling -----");
  const char *endpoint = get_signaling_endpoint();
  gst_println("[Config] SIGNALING_ENDPOINT: %s", endpoint);

  const char *certificate = get_certificate_authority();

  SoupMessage *message = soup_message_new(SOUP_METHOD_GET, endpoint);
  char *protocols[] = {"sender", NULL};

  // Check if certificate file exists
  char *server_ca_cert = realpath(certificate, NULL);
  gboolean use_custom_cert = (server_ca_cert != NULL);

  if (use_custom_cert)
  {
    gst_println("Using custom CA certificate: %s", server_ca_cert);
    // gst_println("[Config] SERVER_CERTIFICATE_AUTHORITY: %s", certificate);
  }
  else
  {
    gst_println("Custom CA certificate not found, using system default");
  }

  gst_println("Connecting to signaling server...");

#if SOUP_CHECK_VERSION(3, 0, 0)
  // --- libsoup 3.x ---
  SoupSession *session = NULL;

  if (use_custom_cert)
  {
    GError *error = NULL;
    GTlsDatabase *tls_db = g_tls_file_database_new(server_ca_cert, &error);
    if (!tls_db)
    {
      gst_printerrln("Failed to load CA file: %s", error->message);
      gst_println("Falling back to system default certificate store");
      g_clear_error(&error);
      session = soup_session_new();
    }
    else
    {
      session = soup_session_new_with_options("tls-database", tls_db, NULL);
      g_object_unref(tls_db);
    }
  }
  else
  {
    // Use system default certificate store for public CAs
    session = soup_session_new();
  }

  SoupLogger *logger = soup_logger_new(SOUP_LOGGER_LOG_BODY);
  soup_session_add_feature(session, SOUP_SESSION_FEATURE(logger));
  g_object_unref(logger);

  soup_session_websocket_connect_async(session, message, NULL, protocols, G_PRIORITY_DEFAULT, NULL, vtx_soup_on_connected, NULL);

#else
  // --- libsoup 2.4 ---
  SoupSession *session = NULL;

  if (use_custom_cert)
  {
    session = soup_session_new_with_options(SOUP_SESSION_SSL_CA_FILE, server_ca_cert, SOUP_SESSION_SSL_STRICT, TRUE, NULL);
  }
  else
  {
    // Use system default certificate store for public CAs
    session = soup_session_new_with_options(SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE, SOUP_SESSION_SSL_STRICT, TRUE, NULL);
  }

  SoupLogger *logger = soup_logger_new(SOUP_LOGGER_LOG_BODY, -1);
  soup_session_add_feature(session, SOUP_SESSION_FEATURE(logger));
  g_object_unref(logger);

  soup_session_websocket_connect_async(session, message, NULL, protocols, NULL, (GAsyncReadyCallback) vtx_soup_on_connected, message);

#endif

  if (server_ca_cert)
  {
    free(server_ca_cert);
  }
  app_state = SERVER_CONNECTING;
}
