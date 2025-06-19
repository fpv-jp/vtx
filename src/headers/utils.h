#pragma once

#include "common.h"

gchar *vtx_platform_to_string(PlatformType platform);

gboolean vtx_cleanup_and_quit_loop(const gchar *msg, AppState state);

gboolean vtx_cleanup_connection(const gchar *msg);

void vtx_ws_send(SoupWebsocketConnection *conn, int type, const gchar *ws1Id, const gchar *ws2Id, JsonObject *data);

gboolean vtx_check_gst_plugins(void);

void print_json_object(JsonObject *obj);

void print_json_array(JsonArray *array);
