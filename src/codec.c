#include <gst/gst.h>

#include "headers/inspection.h"
#include "headers/utils.h"

JsonObject *codec_list = NULL;
static const gchar *s_platform_serviceable_codecs[MAX_ALLOWED_CODECS] = {NULL};

static gboolean s_supported_codecs_initialized = FALSE;

static void vtx_platform_serviceable_codecs(void)
{
  switch (PLATFORM)
  {
    case INTEL_MAC:
      s_platform_serviceable_codecs[0] = "vtenc_h264_hw";
      s_platform_serviceable_codecs[1] = "vtenc_h265_hw";
      s_platform_serviceable_codecs[2] = "vp8enc";
      s_platform_serviceable_codecs[3] = "vp9enc";
      s_platform_serviceable_codecs[4] = "svtav1enc";  // This is for experimental purposes.
      s_platform_serviceable_codecs[5] = "opusenc";
      s_platform_serviceable_codecs[6] = "mulawenc";
      break;

    case LINUX_X86:
      s_platform_serviceable_codecs[0] = "vaapih264enc";  // Supported on PCs with Intel chips.
      s_platform_serviceable_codecs[1] = "vaapih265enc";  // Supported on PCs with Intel chips.
      s_platform_serviceable_codecs[2] = "vp8enc";
      s_platform_serviceable_codecs[3] = "vp9enc";
      s_platform_serviceable_codecs[4] = "svtav1enc";  // This is for experimental purposes.
      s_platform_serviceable_codecs[5] = "opusenc";
      s_platform_serviceable_codecs[6] = "mulawenc";
      break;

    case RPI4_V4L2:
      s_platform_serviceable_codecs[0] = "v4l2h264enc";
      s_platform_serviceable_codecs[1] = "opusenc";
      break;

    case JETSON_NANO_2GB:  // Jetson Nano 2GB is supports HW encoding.
      s_platform_serviceable_codecs[0] = "nvv4l2h264enc";
      s_platform_serviceable_codecs[1] = "nvv4l2h265enc";
      s_platform_serviceable_codecs[2] = "nvv4l2vp8enc";
      s_platform_serviceable_codecs[3] = "nvv4l2vp9enc";  // VP9 exists but the hardware does not support it.
      s_platform_serviceable_codecs[4] = "opusenc";
      break;

    case JETSON_ORIN_NANO_SUPER:
      // Orin Nano Super is does not support HW encoding. decoding only.
      // Use SW encoding.
      s_platform_serviceable_codecs[0] = "x264enc";
      s_platform_serviceable_codecs[1] = "x265enc";
      s_platform_serviceable_codecs[2] = "vp8enc";
      s_platform_serviceable_codecs[3] = "vp9enc";
      s_platform_serviceable_codecs[4] = "opusenc";
      break;

    case RADXA_ROCK_5B:
    case RADXA_ROCK_5T:
      s_platform_serviceable_codecs[0] = "mpph264enc";
      s_platform_serviceable_codecs[1] = "mpph265enc";
      s_platform_serviceable_codecs[2] = "mppvp8enc";
      s_platform_serviceable_codecs[3] = "opusenc";
      break;

    case RPI4_LIBCAM:
      // VideoCore IV is supports HW encoding.
      s_platform_serviceable_codecs[0] = "v4l2h264enc";
      s_platform_serviceable_codecs[1] = "opusenc";
      break;

    case RPI5_LIBCAM:
      // VideoCore VII is does not support HW encoding. decoding only.
      // Use SW encoding.
      s_platform_serviceable_codecs[0] = "openh264enc";
      s_platform_serviceable_codecs[1] = "opusenc";
      break;

    default:
      gst_printerrln("Undefined PLATFORM detected. Allowed codecs list may be incomplete.");
      break;
  }
  s_supported_codecs_initialized = TRUE;
}

// --- vtx_copy_structure_to_json ----------------------------------
static void vtx_copy_structure_to_json(const GstStructure *s, JsonObject *o)
{
  for (int i = 0; i < gst_structure_n_fields(s); ++i)
  {
    const gchar *key = gst_structure_nth_field_name(s, i);
    const GValue *val = gst_structure_get_value(s, key);

    if (GST_VALUE_HOLDS_LIST(val))
    {
      JsonArray *arr = json_array_new();
      for (guint j = 0; j < gst_value_list_get_size(val); ++j)
      {
        const GValue *item = gst_value_list_get_value(val, j);
        if (G_VALUE_HOLDS_STRING(item))
        {
          json_array_add_string_element(arr, g_value_get_string(item));
        }
        else if (G_VALUE_HOLDS_INT(item))
        {
          json_array_add_int_element(arr, g_value_get_int(item));
        }
        else if (G_VALUE_HOLDS_UINT(item))
        {
          json_array_add_int_element(arr, g_value_get_uint(item));
        }
        else
        {
          gchar *tmp = g_strdup_value_contents(item);
          json_array_add_string_element(arr, tmp);
          g_free(tmp);
        }
      }
      json_object_set_array_member(o, key, arr);
      continue;
    }

    if (G_VALUE_HOLDS_STRING(val))
    {
      json_object_set_string_member(o, key, g_value_get_string(val));
    }
    else if (G_VALUE_HOLDS_INT(val))
    {
      json_object_set_int_member(o, key, g_value_get_int(val));
    }
    else if (G_VALUE_HOLDS_UINT(val))
    {
      json_object_set_int_member(o, key, g_value_get_uint(val));
    }
    else if (G_VALUE_HOLDS_BOOLEAN(val))
    {
      json_object_set_boolean_member(o, key, g_value_get_boolean(val));
    }
    else if (G_VALUE_HOLDS_DOUBLE(val))
    {
      json_object_set_double_member(o, key, g_value_get_double(val));
    }
    else
    {
      gchar *value_str = g_strdup_value_contents(val);
      json_object_set_string_member(o, key, value_str);
      g_free(value_str);
    }
  }
}

JsonObject *vtx_supported_codec_inspection(void)
{
  JsonArray *video_codecs_array = json_array_new();
  JsonArray *audio_codecs_array = json_array_new();

  vtx_platform_serviceable_codecs();
  const gchar **supported_codecs = s_platform_serviceable_codecs;

  gst_init(NULL, NULL);

  GList *factories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ENCODER, GST_RANK_NONE);

  // Track already added codecs to avoid duplicates
  GHashTable *seen_video = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  GHashTable *seen_audio = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  for (GList *l = factories; l != NULL; l = l->next)
  {
    GstElementFactory *factory = (GstElementFactory *) l->data;
    const gchar *factory_name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    gboolean allowed = FALSE;

    for (int i = 0; supported_codecs[i] != NULL; ++i)
    {
      if (g_strcmp0(factory_name, supported_codecs[i]) == 0)
      {
        allowed = TRUE;
        break;
      }
    }

    if (!allowed) continue;

    const GList *src_templates = gst_element_factory_get_static_pad_templates(factory);

    for (const GList *t = src_templates; t != NULL; t = t->next)
    {
      GstStaticPadTemplate *tpl = (GstStaticPadTemplate *) t->data;

      if (tpl->direction != GST_PAD_SINK) continue;

      GstCaps *caps = gst_static_pad_template_get_caps(tpl);
      guint caps_size = gst_caps_get_size(caps);

      for (guint i = 0; i < caps_size; i++)
      {
        GstStructure *structure = gst_caps_get_structure(caps, i);
        const gchar *mime_type = gst_structure_get_name(structure);

        // Skip if already added for this mime type category
        if (g_str_has_prefix(mime_type, "video/"))
        {
          if (g_hash_table_contains(seen_video, factory_name))
          {
            continue;
          }
          g_hash_table_insert(seen_video, g_strdup(factory_name), GINT_TO_POINTER(1));
        }
        else if (g_str_has_prefix(mime_type, "audio/"))
        {
          if (g_hash_table_contains(seen_audio, factory_name))
          {
            continue;
          }
          g_hash_table_insert(seen_audio, g_strdup(factory_name), GINT_TO_POINTER(1));
        }
        else
        {
          continue;
        }

        JsonObject *codec_entry_object = json_object_new();
        json_object_set_string_member(codec_entry_object, "name", factory_name);
        vtx_copy_structure_to_json(structure, codec_entry_object);
        json_object_set_string_member(codec_entry_object, "mimeType", mime_type);

        if (g_str_has_prefix(mime_type, "video/"))
        {
          json_array_add_object_element(video_codecs_array, codec_entry_object);
        }
        else
        {
          json_array_add_object_element(audio_codecs_array, codec_entry_object);
        }
      }
      gst_caps_unref(caps);
    }
  }

  g_hash_table_destroy(seen_video);
  g_hash_table_destroy(seen_audio);
  g_list_free_full(factories, gst_object_unref);

  codec_list = json_object_new();
  json_object_set_array_member(codec_list, "video", video_codecs_array);
  json_object_set_array_member(codec_list, "audio", audio_codecs_array);
  return codec_list;
}
