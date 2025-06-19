#pragma once

#include <libsoup/soup.h>

#define SIGNALING_ENDPOINT "wss://fpv-jp/signaling"

void vtx_soup_session_websocket_connect_async (void);

void vtx_on_server_message (SoupWebsocketConnection * conn, SoupWebsocketDataType type, GBytes * message, gpointer user_data);
