#include <stdio.h>
#include <string.h>

#include "headers/pipeline.h"

// --- print_pipeline_pretty ----------------------------------
static void print_pipeline_pretty(const char *desc)
{
  const char *p = desc;
  while (*p)
  {
    putchar(*p);
    if (*p == '!' || *p == '.')
    {
      putchar('\n');
      p++;
      while (*p == ' ') p++;
      continue;
    }
    p++;
  }
}

// --- vtx_pipeline_parse_media_params ----------------------------------
gboolean vtx_pipeline_parse_media_params(JsonObject *o, MediaParams *p)
{
  p->video_pipeline = json_object_get_string_member(o, "video_pipeline");
  p->audio_pipeline = json_object_get_string_member(o, "audio_pipeline");
  p->video_priority = json_object_get_string_member(o, "video_priority");
  p->audio_priority = json_object_get_string_member(o, "audio_priority");
  p->video_payload_type = json_object_get_int_member(o, "video_payload_type");
  p->audio_payload_type = json_object_get_int_member(o, "audio_payload_type");
  p->platform = PLATFORM;
  p->network_interface = json_object_has_member(o, "network_interface")
                          ? json_object_get_string_member(o, "network_interface")
                          : NULL;

  gst_println("=== MediaParams parsed ===\n");
  gst_println("MediaParams {");
  gst_println("  video_pipeline: %s", p->video_pipeline ? p->video_pipeline : "NULL");
  gst_println("  audio_pipeline: %s", p->audio_pipeline ? p->audio_pipeline : "NULL");
  gst_println("  video_priority: %s", p->video_priority ? p->video_priority : "NULL");
  gst_println("  audio_priority: %s", p->audio_priority ? p->audio_priority : "NULL");
  gst_println("  video_payload_type: %u", p->video_payload_type);
  gst_println("  audio_payload_type: %u", p->audio_payload_type);
  gst_println("  platform: %d", p->platform);
  gst_println("  network_interface: %s", p->network_interface ? p->network_interface : "NULL");
  gst_println("}\n");

  if (p->video_pipeline)
  {
    gst_println("=== Video Pipeline description ===\n");
    print_pipeline_pretty(p->video_pipeline);
    gst_println("\n");
  }

  if (p->audio_pipeline)
  {
    gst_println("=== Audio Pipeline description ===\n");
    print_pipeline_pretty(p->audio_pipeline);
    gst_println("\n");
  }
  return TRUE;
}

// --- vtx_pipeline_build ----------------------------------
GstElement *vtx_pipeline_build(const MediaParams *p)
{
  GstElement *pipeline = NULL;
  GstElement *webrtc = NULL;
  GError *error = NULL;

  // If network interface is specified, use custom ICE agent
  if (p->network_interface)
  {
    gst_println("=== Creating WebRTC with custom ICE agent for interface: %s ===\n", p->network_interface);

    GstWebRTCICE *agent = GST_WEBRTC_ICE(customice_agent_new("ice-agent", p->network_interface));
    webrtc = gst_element_factory_make_full(
        "webrtcbin",
        "name", "webrtcbin",
        "latency", 0,
        "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
        "stun-server", STUN_SERVER,
        "ice-agent", agent,
        NULL);
    g_object_unref(agent);

    if (!webrtc)
    {
      gst_printerrln("Failed to create webrtcbin with custom ICE agent");
      return NULL;
    }

    pipeline = gst_pipeline_new("pipeline");
    gst_bin_add(GST_BIN(pipeline), webrtc);

    if (p->video_pipeline)
    {
      gst_println("=== Video Pipeline description ===\n");
      print_pipeline_pretty(p->video_pipeline);
      gst_println("\n");

      GstElement *video_bin = gst_parse_bin_from_description(p->video_pipeline, TRUE, &error);
      if (!video_bin || error)
      {
        gst_printerrln("Video pipeline parse error: %s", error ? error->message : "unknown");
        g_clear_error(&error);
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_bin_add(GST_BIN(pipeline), video_bin);
      if (!gst_element_link(video_bin, webrtc))
      {
        gst_printerrln("Failed to link video pipeline to webrtcbin");
        gst_object_unref(pipeline);
        return NULL;
      }
    }

    if (p->audio_pipeline)
    {
      gst_println("=== Audio Pipeline description ===\n");
      print_pipeline_pretty(p->audio_pipeline);
      gst_println("\n");

      GstElement *audio_bin = gst_parse_bin_from_description(p->audio_pipeline, TRUE, &error);
      if (!audio_bin || error)
      {
        gst_printerrln("Audio pipeline parse error: %s", error ? error->message : "unknown");
        g_clear_error(&error);
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_bin_add(GST_BIN(pipeline), audio_bin);
      if (!gst_element_link(audio_bin, webrtc))
      {
        gst_printerrln("Failed to link audio pipeline to webrtcbin");
        gst_object_unref(pipeline);
        return NULL;
      }
    }

    return pipeline;
  }
  else
  {
    // Use default ICE agent with gst_parse_launch
    gchar *desc = NULL;

    if (p->video_pipeline && p->audio_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle stun-server=%s "
          "%s ! webrtcbin. "
          "%s ! webrtcbin.",
          STUN_SERVER, p->video_pipeline, p->audio_pipeline);
    }
    else if (p->video_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle stun-server=%s "
          "%s ! webrtcbin.",
          STUN_SERVER, p->video_pipeline);
    }
    else if (p->audio_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle stun-server=%s "
          "%s ! webrtcbin.",
          STUN_SERVER, p->audio_pipeline);
    }

    if (!desc) return NULL;
    gst_println("=== Assembled WebRTC pipeline ===\n");
    gst_println("%s", desc);
    gst_println("\n");

    pipeline = gst_parse_launch(desc, &error);
    g_free(desc);
    if (error)
    {
      gst_printerrln("Pipeline parse error: %s", error ? error->message : "unknown");
      g_clear_error(&error);
      return NULL;
    }

    return pipeline;
  }
}
