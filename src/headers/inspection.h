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

JsonArray *vtx_load_device_launch_entries ();

void vtx_inspection ();

JsonArray *vtx_media_device_inspection (void);

#define MAX_ALLOWED_CODECS 16

JsonObject *vtx_supported_codecs_inspection (void);

void vtx_expand_enums (const gchar * expr, GPtrArray * expanded_exprs);

void vtx_expand_range (GPtrArray * in_exprs, GPtrArray * out_exprs);
