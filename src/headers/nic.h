#pragma once

#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>

#include "wpa.h"

typedef void (*VtxKeyValueFunc)(const char *key, const char *value, gpointer user_data);

JsonObject *vtx_nic_parse_wpa_for_cli(const gchar *output);

void vtx_nic_parse_wifi_info(char *line, VtxKeyValueFunc callback, gpointer user_data);
void vtx_nic_parse_iw_iface_add_attribute(const char *key, const char *value, gpointer user_data);
void vtx_nic_parse_wifi_info_txq_block(FILE *fp, JsonObject *iface);

JsonArray *vtx_nic_parse_ethtool_info(FILE *fp);

JsonObject *vtx_nic_parse_interface_new(const gchar *name);
void vtx_nic_parse_interface_set_address(JsonObject *iface, const gchar *address);
void vtx_nic_parse_interface_set_flags(JsonObject *iface, gboolean up, gboolean running);
void vtx_nic_parse_interface_attach_iw(JsonObject *iface, JsonObject *iw, gboolean from_iw_only);
void vtx_nic_parse_interface_attach_wifi(JsonObject *iface, JsonObject *status, JsonObject *signal);
void vtx_nic_parse_interface_attach_ethtool(JsonObject *iface, JsonArray *info);
void vtx_nic_parse_interface_add(JsonArray *interfaces, JsonObject *iface);
void vtx_nic_parse_wpa_foreach_key_value(const char *response, VtxWpaKeyValueFunc callback, gpointer user_data);
void vtx_nic_parse_wpa_for_datachannel(JsonBuilder *builder, const char *response);
