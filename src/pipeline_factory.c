#include <stdio.h>
#include <string.h>

#include "headers/pipeline.h"

// --- vtx_pipeline_print_pretty ----------------------------------
static void vtx_pipeline_print_pretty(const char *desc)
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

// --- vtx_pipeline_sanitize_description ----------------------------------
// Removes trailing caps specifications that can cause parse errors
// when using gst_parse_bin_from_description with ghost_unlinked_pads=TRUE
static gchar *vtx_pipeline_sanitize_description(const gchar *desc)
{
  if (!desc) return NULL;

  // Create a mutable copy
  gchar *sanitized = g_strdup(desc);
  gchar *p = sanitized + strlen(sanitized) - 1;

  // Trim trailing whitespace
  while (p > sanitized && g_ascii_isspace(*p))
  {
    *p = '\0';
    p--;
  }

  // Check if pipeline ends with a standalone caps specification
  // Pattern: "! application/x-something,prop=value,..."
  gchar *last_bang = g_strrstr(sanitized, "!");
  if (last_bang)
  {
    gchar *after_bang = last_bang + 1;
    // Skip whitespace
    while (*after_bang && g_ascii_isspace(*after_bang)) after_bang++;

    // Check if what follows looks like a caps specification (contains '/')
    // and doesn't contain element-like patterns (no spaces before '/')
    if (*after_bang && strchr(after_bang, '/'))
    {
      gchar *first_space = strchr(after_bang, ' ');
      gchar *first_slash = strchr(after_bang, '/');

      // If slash comes before any space, it's likely a caps specification
      if (first_slash && (!first_space || first_slash < first_space))
      {
        gst_println("Removing trailing caps specification: %s", after_bang);
        *last_bang = '\0';
      }
    }
  }

  return sanitized;
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
  p->network_interface = json_object_has_member(o, "network_interface") ? json_object_get_string_member(o, "network_interface") : NULL;
  p->video_profile = json_object_has_member(o, "video_profile") ? json_object_get_string_member(o, "video_profile") : NULL;

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
  gst_println("  video_profile: %s", p->video_profile ? p->video_profile : "NULL");
  gst_println("}\n");

  return TRUE;
}

// --- vtx_pipeline_build ----------------------------------
GstElement *vtx_pipeline_build(const MediaParams *p)
{
  GstElement *pipeline = NULL;
  GstElement *webrtc = NULL;
  GError *error = NULL;

  // Set camera H264 profile based on browser support
  if (p->video_profile)
  {
    gint profile_value = -1;
    if (g_strcmp0(p->video_profile, "high") == 0)
      profile_value = 4;
    else if (g_strcmp0(p->video_profile, "main") == 0)
      profile_value = 2;
    else if (g_strcmp0(p->video_profile, "constrained-baseline") == 0)
      profile_value = 1;
    else if (g_strcmp0(p->video_profile, "baseline") == 0)
      profile_value = 0;

    if (profile_value >= 0)
    {
      gchar *cmd = g_strdup_printf("v4l2-ctl -d /dev/video0 --set-ctrl h264_profile=%d", profile_value);
      gst_println("Setting camera H264 profile: %s (value=%d)", p->video_profile, profile_value);
      gst_println("Executing: %s", cmd);

      gint exit_status;
      GError *exec_error = NULL;
      if (!g_spawn_command_line_sync(cmd, NULL, NULL, &exit_status, &exec_error))
      {
        gst_printerrln("Failed to execute v4l2-ctl: %s", exec_error ? exec_error->message : "unknown");
        g_clear_error(&exec_error);
      }
      else if (exit_status != 0)
      {
        gst_printerrln("v4l2-ctl returned non-zero exit status: %d", exit_status);
      }
      else
      {
        gst_println("Successfully set camera H264 profile to %s", p->video_profile);
      }

      g_free(cmd);
    }
  }

  // If network interface is specified, use custom ICE agent
  if (p->network_interface)
  {
    gst_println("=== Creating WebRTC with custom ICE agent for interface: %s ===\n", p->network_interface);

    GstWebRTCICE *agent = GST_WEBRTC_ICE(customice_agent_new("ice-agent", p->network_interface));

    if (!agent || !GST_IS_WEBRTC_ICE(agent))
    {
      gst_printerrln("Failed to create valid CustomICEAgent as GstWebRTCICE");
      return NULL;
    }

    gst_println("CustomICEAgent cast to GstWebRTCICE succeeded\n");

    // webrtc = gst_element_factory_make_full("webrtcbin", "name", "webrtcbin", "latency", 0, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, "stun-server", STUN_SERVER, "ice-agent", agent, NULL);
    webrtc = gst_element_factory_make_full("webrtcbin", "name", "webrtcbin", "latency", 0, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, "ice-agent", agent, NULL);

    if (webrtc)
    {
      // Keep CustomICEAgent alive for the lifetime of webrtcbin even if the
      // construct-only property does not hold its own reference.
      g_object_set_data_full(G_OBJECT(webrtc), "custom-ice-agent", g_object_ref(agent), (GDestroyNotify) g_object_unref);
    }
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
      vtx_pipeline_print_pretty(p->video_pipeline);
      gst_println("\n");

      // Sanitize the pipeline description to remove trailing caps
      gchar *sanitized_video = vtx_pipeline_sanitize_description(p->video_pipeline);

      GstElement *video_bin = gst_parse_bin_from_description(sanitized_video, TRUE, &error);
      g_free(sanitized_video);

      if (!video_bin || error)
      {
        gst_printerrln("Video pipeline parse error: %s", error ? error->message : "unknown");
        g_clear_error(&error);
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_bin_add(GST_BIN(pipeline), video_bin);
      // webrtcbin has on-request sink pads (sink_%u), so we must use gst_element_link_pads
      if (!gst_element_link_pads(video_bin, NULL, webrtc, "sink_%u"))
      {
        gst_printerrln("Failed to link video pipeline to webrtcbin");
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_println("Successfully linked video pipeline to webrtcbin\n");
    }

    if (p->audio_pipeline)
    {
      gst_println("=== Audio Pipeline description ===\n");
      vtx_pipeline_print_pretty(p->audio_pipeline);
      gst_println("\n");

      // Sanitize the pipeline description to remove trailing caps
      gchar *sanitized_audio = vtx_pipeline_sanitize_description(p->audio_pipeline);

      GstElement *audio_bin = gst_parse_bin_from_description(sanitized_audio, TRUE, &error);
      g_free(sanitized_audio);

      if (!audio_bin || error)
      {
        gst_printerrln("Audio pipeline parse error: %s", error ? error->message : "unknown");
        g_clear_error(&error);
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_bin_add(GST_BIN(pipeline), audio_bin);
      // webrtcbin has on-request sink pads (sink_%u), so we must use gst_element_link_pads
      if (!gst_element_link_pads(audio_bin, NULL, webrtc, "sink_%u"))
      {
        gst_printerrln("Failed to link audio pipeline to webrtcbin");
        gst_object_unref(pipeline);
        return NULL;
      }
      gst_println("Successfully linked audio pipeline to webrtcbin\n");
    }

    return pipeline;
  }
  else
  {
    // Use default ICE agent with gst_parse_launch
    gchar *desc = NULL;

    // "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle stun-server=stun://stun.l.google.com:19302 "

    if (p->video_pipeline && p->audio_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
          "%s ! webrtcbin. "
          "%s ! webrtcbin.",
          p->video_pipeline, p->audio_pipeline);
    }
    else if (p->video_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
          "%s ! webrtcbin.",
          p->video_pipeline);
    }
    else if (p->audio_pipeline)
    {
      desc = g_strdup_printf(
          "webrtcbin name=webrtcbin latency=0 bundle-policy=max-bundle "
          "%s ! webrtcbin.",
          p->audio_pipeline);
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
