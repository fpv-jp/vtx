#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "headers/inspection.h"

JsonArray *device_list = NULL;

static void free_device_entry(gpointer data)
{
  DeviceEntry *entry = (DeviceEntry *) data;
  if (!entry) return;

  g_free(entry->name);
  g_free(entry->class);
  if (entry->caps)
  {
    g_ptr_array_free(entry->caps, TRUE);
  }
  if (entry->properties)
  {
    g_hash_table_destroy(entry->properties);
  }
  g_free(entry->gst_launch);
  g_free(entry);
}

static char *vtx_trim_caps_types(const char *caps_str)
{
  if (!caps_str) return NULL;
  GString *result = g_string_new(NULL);
  const char *p = caps_str;
  while (*p)
  {
    if (g_str_has_prefix(p, "(string)"))
    {
      p += 8;
    }
    else if (g_str_has_prefix(p, "(int)"))
    {
      p += 5;
    }
    else if (g_str_has_prefix(p, "(fraction)"))
    {
      p += 10;
    }
    else
    {
      g_string_append_c(result, *p);
      p++;
    }
  }
  return g_string_free(result, FALSE);
}

static gchar *trim_string(const gchar *str)
{
  if (!str) return NULL;

  while (*str && g_ascii_isspace(*str)) str++;

  if (*str == '\0')
  {
    return g_strdup("");
  }

  gchar *end = (gchar *) (str + strlen(str) - 1);
  while (end > str && g_ascii_isspace(*end)) end--;

  return g_strndup(str, end - str + 1);
}

static gchar *parse_device_entry(FILE *fp, GPtrArray *devices)
{
  DeviceEntry *entry = g_new0(DeviceEntry, 1);
  entry->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  entry->caps = g_ptr_array_new_with_free_func(g_free);

  gboolean in_properties_section = FALSE;
  gboolean in_caps_section = FALSE;
  char line[1024];
  gchar *next_device_marker_line = NULL;

  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);

    if (strstr(line, "Device found:"))
    {
      next_device_marker_line = g_strdup(line);
      break;
    }

    if (strstr(line, "gst-launch-1.0"))
    {
      gchar *pipeline_segment_raw = line + strlen("gst-launch-1.0");
      gchar *pipeline_segment_trimmed = trim_string(pipeline_segment_raw);
      gchar *final_pipeline_value = NULL;

      if (pipeline_segment_trimmed && strlen(pipeline_segment_trimmed) > 1 && pipeline_segment_trimmed[0] == '0' && pipeline_segment_trimmed[1] == ' ')
      {
        final_pipeline_value = g_strdup(pipeline_segment_trimmed + 2);
      }
      else
      {
        final_pipeline_value = g_strdup(pipeline_segment_trimmed);
      }

      g_free(pipeline_segment_trimmed);
      entry->gst_launch = g_strdup_printf("%s", final_pipeline_value);
      g_free(final_pipeline_value);

      in_properties_section = FALSE;
      in_caps_section = FALSE;
    }
    else if (in_properties_section)
    {
      gchar **parts = g_strsplit(line, "=", 2);

      if (g_strv_length(parts) == 2)
      {
        gchar *key = trim_string(parts[0]);
        gchar *value = trim_string(parts[1]);
        g_hash_table_insert(entry->properties, key, value);
      }
      g_strfreev(parts);
    }
    else if (strstr(line, "properties:"))
    {
      in_properties_section = TRUE;
      in_caps_section = FALSE;
    }
    else if (strstr(line, "caps  :") || in_caps_section)
    {
      gchar *trimmed_cap_line;

      if (strstr(line, "caps  :"))
      {
        trimmed_cap_line = trim_string(line + strlen("caps  : "));
      }
      else
      {
        trimmed_cap_line = trim_string(line);
      }

      if (!g_str_has_prefix(trimmed_cap_line, "image/jpeg"))
      {
        g_ptr_array_add(entry->caps, vtx_trim_caps_types(trimmed_cap_line));
      }

      in_caps_section = TRUE;
    }
    else if (strstr(line, "name  :"))
    {
      entry->name = trim_string(line + strlen("name  : "));
    }
    else if (strstr(line, "class :"))
    {
      entry->class = trim_string(line + strlen("class : "));
    }
  }

  g_ptr_array_add(devices, entry);
  return next_device_marker_line;
}

static JsonArray *devices_to_json(GPtrArray *devices)
{
  JsonArray *devices_array = json_array_new();

  for (guint i = 0; i < devices->len; i++)
  {
    DeviceEntry *entry = g_ptr_array_index(devices, i);
    JsonObject *device_object = json_object_new();

    json_object_set_string_member(device_object, "name", entry->name ? entry->name : "");
    json_object_set_string_member(device_object, "klass", entry->class ? entry->class : "");

    JsonArray *caps_array = json_array_new();

    if (entry->caps)
    {
      for (guint j = 0; j < entry->caps->len; j++)
      {
        const gchar *cap = g_ptr_array_index(entry->caps, j);
        json_array_add_string_element(caps_array, cap);
      }
    }

    json_object_set_array_member(device_object, "caps", caps_array);

    JsonObject *properties_object = json_object_new();

    if (entry->properties)
    {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, entry->properties);
      while (g_hash_table_iter_next(&iter, &key, &value))
      {
        json_object_set_string_member(properties_object, (const gchar *) key, (const gchar *) value);
      }
    }

    json_object_set_object_member(device_object, "properties", properties_object);

    if (entry->gst_launch)
    {
      json_object_set_string_member(device_object, "launch", entry->gst_launch);
    }

    json_array_add_object_element(devices_array, device_object);
  }

  return devices_array;
}

JsonArray *vtx_device_load_launch_entries()
{
  FILE *fp = popen("gst-device-monitor-1.0 Video/Source Audio/Source", "r");
  if (!fp)
  {
    gst_printerrln("Failed to open gst-device-monitor-1.0 pipe");
    return NULL;
  }

  GPtrArray *devices = g_ptr_array_new_with_free_func(free_device_entry);
  char line[1024];
  gchar *current_device_header = NULL;

  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);
    if (strstr(line, "Device found:"))
    {
      current_device_header = g_strdup(line);
      break;
    }
  }

  while (current_device_header != NULL)
  {
    g_free(current_device_header);
    current_device_header = parse_device_entry(fp, devices);
  }

  pclose(fp);

  device_list = devices_to_json(devices);
  g_ptr_array_free(devices, TRUE);

  return device_list;
}
