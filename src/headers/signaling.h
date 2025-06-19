#pragma once

#include <libsoup/soup.h>

#define DEFAULT_SIGNALING_ENDPOINT "wss://fpv/signaling"

#define DEFAULT_SERVER_CERTIFICATE_AUTHORITY "server-ca-cert.pem"

void vtx_soup_session_websocket_connect_async(void);

void vtx_soup_on_message(SoupWebsocketConnection* conn, SoupWebsocketDataType type, GBytes* message, gpointer user_data);
