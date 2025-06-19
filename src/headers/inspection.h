#pragma once

#include <gst/gst.h>
#include <json-glib/json-glib.h>

typedef struct
{
  gchar *name;
  gchar *class;
  GPtrArray *caps;
  GHashTable *properties;
  gchar *gst_launch;
} DeviceEntry;

JsonArray *vtx_device_load_launch_entries();

#define MAX_ALLOWED_CODECS 16

JsonObject *vtx_codecs_supported_inspection(void);

JsonArray *vtx_network_interfaces_inspection(void);
