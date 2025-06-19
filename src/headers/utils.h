#pragma once

#include "common.h"

gboolean vtx_cleanup_and_quit_loop (const gchar * msg, AppState state);

void vtx_ws_send (SoupWebsocketConnection * conn, int type, const gchar * ws1Id, const gchar * ws2Id, JsonObject * data);

void vtx_dc_send (GObject * dc, JsonObject * msg);

gboolean vtx_check_plugins (void);

void print_json_object (JsonObject * obj);

void print_json_array (JsonArray * array);
