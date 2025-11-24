#include <string.h>

#include "headers/nic.h"
#include "headers/wpa.h"

typedef struct
{
  JsonObject *object;
  gboolean has_data;
} JsonKeyValueContext;

// --- vtx_nic_parse_wpa_foreach_key_value ----------------------------------
void vtx_nic_parse_wpa_foreach_key_value(const char *response, VtxWpaKeyValueFunc callback, gpointer user_data)
{
  if (!response || response[0] == '\0' || !callback) return;

  char *copy = g_strdup(response);
  if (!copy) return;

  char *saveptr = NULL;
  char *line = strtok_r(copy, "\n", &saveptr);

  while (line != NULL)
  {
    if (line[0] != '\0')
    {
      char *value = strchr(line, '=');
      if (value)
      {
        *value = '\0';
        value++;

        if (line[0] != '\0')
        {
          callback(line, value, user_data);
        }
      }
    }

    line = strtok_r(NULL, "\n", &saveptr);
  }

  g_free(copy);
}

// --- vtx_nic_parse_json_object_add_entry ----------------------------------
static void vtx_nic_parse_json_object_add_entry(const char *key, const char *value, gpointer user_data)
{
  JsonKeyValueContext *ctx = (JsonKeyValueContext *) user_data;
  if (!ctx || !key || !ctx->object) return;

  json_object_set_string_member(ctx->object, key, value ? value : "");
  ctx->has_data = TRUE;
}

// --- vtx_nic_parse_wpa_for_cli ----------------------------------
JsonObject *vtx_nic_parse_wpa_for_cli(const gchar *output)
{
  if (!output || *output == '\0') return NULL;

  JsonKeyValueContext ctx = {.object = json_object_new(), .has_data = FALSE};
  vtx_nic_parse_wpa_foreach_key_value(output, vtx_nic_parse_json_object_add_entry, &ctx);

  if (!ctx.has_data)
  {
    json_object_unref(ctx.object);
    return NULL;
  }

  return ctx.object;
}

// --- vtx_nic_parse_json_add_entry ----------------------------------
static void vtx_nic_parse_json_add_entry(const char *key, const char *value, gpointer user_data)
{
  JsonBuilder *builder = (JsonBuilder *) user_data;
  if (!builder || !key) return;

  json_builder_set_member_name(builder, key);
  json_builder_add_string_value(builder, value ? value : "");
}

// --- vtx_nic_parse_wpa_for_datachannel ----------------------------------
void vtx_nic_parse_wpa_for_datachannel(JsonBuilder *builder, const char *response)
{
  if (!builder || !response || response[0] == '\0') return;

  vtx_nic_parse_wpa_foreach_key_value(response, vtx_nic_parse_json_add_entry, builder);
}

// --- vtx_nic_parse_wifi_info ----------------------------------
void vtx_nic_parse_wifi_info(char *line, VtxKeyValueFunc callback, gpointer user_data)
{
  if (!line || !*line || !callback) return;

  gchar *key_start = line;
  gchar *key_end = key_start;
  while (*key_end && !g_ascii_isspace((guchar) *key_end)) key_end++;
  if (key_end == key_start) return;

  gchar saved = *key_end;
  *key_end = '\0';

  gchar *value = (gchar *) g_strchug(key_end + 1);
  callback(key_start, value, user_data);

  *key_end = saved;
}

// --- vtx_nic_parse_iw_iface_add_attribute ----------------------------------
void vtx_nic_parse_iw_iface_add_attribute(const char *key, const char *value, gpointer user_data)
{
  JsonObject *iface = (JsonObject *) user_data;
  if (!iface || !key) return;
  json_object_set_string_member(iface, key, value ? value : "");
}

// --- vtx_nic_parse_wifi_info_txq_block ----------------------------------
void vtx_nic_parse_wifi_info_txq_block(FILE *fp, JsonObject *iface)
{
  if (!fp || !iface) return;

  char header_line[512];
  char values_line[512];

  if (!fgets(header_line, sizeof(header_line), fp)) return;
  if (!fgets(values_line, sizeof(values_line), fp)) return;

  g_strchomp(header_line);
  g_strchomp(values_line);

  gchar *header = g_strchug(header_line);
  gchar *values = g_strchug(values_line);
  if (*header == '\0' || *values == '\0') return;

  gchar **keys = g_strsplit_set(header, "\t ", -1);
  gchar **data = g_strsplit_set(values, "\t ", -1);

  JsonObject *txq = json_object_new();
  gint value_index = 0;

  for (gint i = 0; keys && keys[i]; i++)
  {
    if (keys[i][0] == '\0') continue;

    while (data && data[value_index] && data[value_index][0] == '\0') value_index++;

    if (!data || !data[value_index]) break;

    json_object_set_string_member(txq, keys[i], data[value_index]);
    value_index++;
  }

  json_object_set_object_member(iface, "multicast_txq", txq);

  g_strfreev(keys);
  g_strfreev(data);
}

// --- vtx_nic_parse_ethtool_info ----------------------------------
JsonArray *vtx_nic_parse_ethtool_info(FILE *fp)
{
  if (!fp) return NULL;

  JsonArray *entries = json_array_new();
  char line[256];

  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);

    char *colon = strchr(line, ':');
    if (!colon) continue;

    *colon = '\0';
    gchar *key = g_strstrip(line);
    gchar *value = colon + 1;
    value = (gchar *) g_strchug(value);

    if (!key || *key == '\0') continue;

    JsonObject *entry = json_object_new();
    json_object_set_string_member(entry, "key", key);
    json_object_set_string_member(entry, "value", value ? value : "");
    json_array_add_object_element(entries, entry);
  }

  if (json_array_get_length(entries) == 0)
  {
    json_array_unref(entries);
    return NULL;
  }

  return entries;
}

JsonObject *vtx_nic_parse_interface_new(const gchar *name)
{
  JsonObject *iface = json_object_new();
  if (name) json_object_set_string_member(iface, "name", name);
  return iface;
}

void vtx_nic_parse_interface_set_address(JsonObject *iface, const gchar *address)
{
  if (!iface || !address || *address == '\0') return;
  json_object_set_string_member(iface, "address", address);
}

void vtx_nic_parse_interface_set_flags(JsonObject *iface, gboolean up, gboolean running)
{
  if (!iface) return;
  json_object_set_boolean_member(iface, "up", up);
  json_object_set_boolean_member(iface, "running", running);
}

void vtx_nic_parse_interface_attach_iw(JsonObject *iface, JsonObject *iw, gboolean from_iw_only)
{
  if (!iface || !iw) return;
  json_object_set_object_member(iface, "iw", json_object_ref(iw));
  if (from_iw_only) json_object_set_boolean_member(iface, "from_iw_only", TRUE);
}

void vtx_nic_parse_interface_attach_wifi(JsonObject *iface, JsonObject *status, JsonObject *signal)
{
  if (!iface) return;
  if (status) json_object_set_object_member(iface, "wpa_status", json_object_ref(status));
  if (signal) json_object_set_object_member(iface, "wpa_signal", json_object_ref(signal));
}

void vtx_nic_parse_interface_attach_ethtool(JsonObject *iface, JsonArray *info)
{
  if (!iface || !info) return;
  json_object_set_array_member(iface, "ethtool", json_array_ref(info));
}

void vtx_nic_parse_interface_add(JsonArray *interfaces, JsonObject *iface)
{
  if (!interfaces || !iface) return;
  json_array_add_object_element(interfaces, iface);
}
