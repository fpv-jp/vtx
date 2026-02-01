#include <string.h>

#include "headers/nic.h"
#include "headers/wpa.h"

typedef struct
{
  JsonObject *object;
  gboolean has_data;
} JsonKeyValueContext;

// Iterates over each "key=value" line in a wpa_cli response string and invokes the callback for each pair.
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

// Callback that inserts a key-value pair as a string member into a JsonObject held in the context.
static void vtx_nic_parse_json_object_add_entry(const char *key, const char *value, gpointer user_data)
{
  JsonKeyValueContext *ctx = (JsonKeyValueContext *) user_data;
  if (!ctx || !key || !ctx->object) return;

  json_object_set_string_member(ctx->object, key, value ? value : "");
  ctx->has_data = TRUE;
}

// Parses a wpa_cli key=value response into a JsonObject, returning NULL if no data is found.
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

// Callback that adds a key-value string pair into a JsonBuilder object currently being constructed.
static void vtx_nic_parse_json_add_entry(const char *key, const char *value, gpointer user_data)
{
  JsonBuilder *builder = (JsonBuilder *) user_data;
  if (!builder || !key) return;

  json_builder_set_member_name(builder, key);
  json_builder_add_string_value(builder, value ? value : "");
}

// Parses a wpa_cli response and writes each key-value pair into the given JsonBuilder for DataChannel use.
void vtx_nic_parse_wpa_for_datachannel(JsonBuilder *builder, const char *response)
{
  if (!builder || !response || response[0] == '\0') return;

  vtx_nic_parse_wpa_foreach_key_value(response, vtx_nic_parse_json_add_entry, builder);
}

// Splits a single "key value" line from iw output and invokes the callback with the key and trailing value.
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

// Callback that sets a string attribute on a JsonObject representing an iw interface entry.
void vtx_nic_parse_iw_iface_add_attribute(const char *key, const char *value, gpointer user_data)
{
  JsonObject *iface = (JsonObject *) user_data;
  if (!iface || !key) return;
  json_object_set_string_member(iface, key, value ? value : "");
}

// Reads the multicast TXQ header and values lines from iw output and stores them as a nested JsonObject on iface.
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

// Reads ethtool output from fp and returns a JsonArray of {key, value} objects for each colon-separated line.
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

// Allocates and returns a new JsonObject representing a network interface with the given name.
JsonObject *vtx_nic_parse_interface_new(const gchar *name)
{
  JsonObject *iface = json_object_new();
  if (name) json_object_set_string_member(iface, "name", name);
  return iface;
}

// Sets the "address" string field on the given interface JsonObject.
void vtx_nic_parse_interface_set_address(JsonObject *iface, const gchar *address)
{
  if (!iface || !address || *address == '\0') return;
  json_object_set_string_member(iface, "address", address);
}

// Sets the "up" and "running" boolean flags on the given interface JsonObject.
void vtx_nic_parse_interface_set_flags(JsonObject *iface, gboolean up, gboolean running)
{
  if (!iface) return;
  json_object_set_boolean_member(iface, "up", up);
  json_object_set_boolean_member(iface, "running", running);
}

// Attaches an iw JsonObject to the interface and optionally marks it as iw-only if it had no ifaddrs entry.
void vtx_nic_parse_interface_attach_iw(JsonObject *iface, JsonObject *iw, gboolean from_iw_only)
{
  if (!iface || !iw) return;
  json_object_set_object_member(iface, "iw", json_object_ref(iw));
  if (from_iw_only) json_object_set_boolean_member(iface, "from_iw_only", TRUE);
}

// Attaches wpa_status and wpa_signal JsonObjects to the interface, representing WiFi connection details.
void vtx_nic_parse_interface_attach_wifi(JsonObject *iface, JsonObject *status, JsonObject *signal)
{
  if (!iface) return;
  if (status) json_object_set_object_member(iface, "wpa_status", json_object_ref(status));
  if (signal) json_object_set_object_member(iface, "wpa_signal", json_object_ref(signal));
}

// Attaches an ethtool JsonArray to the interface object under the "ethtool" key.
void vtx_nic_parse_interface_attach_ethtool(JsonObject *iface, JsonArray *info)
{
  if (!iface || !info) return;
  json_object_set_array_member(iface, "ethtool", json_array_ref(info));
}

// Appends an interface JsonObject to the interfaces JsonArray.
void vtx_nic_parse_interface_add(JsonArray *interfaces, JsonObject *iface)
{
  if (!interfaces || !iface) return;
  json_array_add_object_element(interfaces, iface);
}
