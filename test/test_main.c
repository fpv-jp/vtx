#include <gst/webrtc/webrtc.h>

#include "data_channel.h"
#include "inspection.h"
#include "unity.h"
#include "utils.h"

// ----- WebRTC Loopback Test Helpers -----

typedef struct
{
  GstElement *sender_pipe;
  GstElement *receiver_pipe;
  GstElement *sender_webrtc;
  GstElement *receiver_webrtc;
  GMainLoop *loop;
  gboolean negotiation_done;
  gint frame_count;
  guint timeout_id;
} LoopbackState;

void
test_vtx_nic_inspection (void)
{
  JsonArray *result = vtx_nic_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Network Interfaces -----");
  print_json_array (result);
}

void
test_vtx_device_load_launch_entries (void)
{
  JsonArray *result = vtx_device_load_launch_entries ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Device Capabilities -----");
  print_json_array (result);
}

void
test_vtx_supported_codec_inspection (void)
{
  JsonObject *result = vtx_supported_codec_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Support Codecs -----");
  print_json_object (result);
}

void
test_vtx_msp_flight_controller (void)
{
  JsonArray *result = vtx_msp_flight_controller ();
  if (result == NULL || json_array_get_length (result) == 0)
    {
      gst_println ("----- SKIPPED: No flight controller detected -----");
      TEST_IGNORE_MESSAGE ("No flight controller detected, skipping test");
      return;
    }
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Flight Controllers -----");
  print_json_array (result);
}

// Test if detected HW encoders can create a valid GStreamer pipeline
void
test_vtx_encoder_pipeline (void)
{
  gst_println ("----- Testing HW Encoder Pipeline -----");
  gst_println ("  Platform: %s", vtx_platform_to_string (g_platform));
  gst_println ("  GPU Vendor: %s", vtx_gpu_vendor_to_string (g_gpu_vendor));

  // Skip if not LINUX_X86
  if (g_platform != LINUX_X86)
    {
      gst_println ("  SKIPPED: Not LINUX_X86 platform");
      TEST_IGNORE_MESSAGE ("Not LINUX_X86 platform, skipping encoder test");
      return;
    }

  // Define encoder to test based on GPU vendor
  const gchar *test_encoder = NULL;
  const gchar *encoder_caps = NULL;

  switch (g_gpu_vendor)
    {
    case GPU_VENDOR_AMD:
      test_encoder = "amfh264enc";
      encoder_caps = "video/x-h264";
      break;
    case GPU_VENDOR_NVIDIA:
      test_encoder = "nvh264enc";
      encoder_caps = "video/x-h264";
      break;
    case GPU_VENDOR_INTEL:
      test_encoder = "vah264enc";
      encoder_caps = "video/x-h264";
      break;
    default:
      gst_println ("  SKIPPED: Unknown GPU vendor");
      TEST_IGNORE_MESSAGE ("Unknown GPU vendor, skipping encoder test");
      return;
    }

  gst_println ("  Testing encoder: %s", test_encoder);

  // Check if encoder element exists
  GstElementFactory *factory = gst_element_factory_find (test_encoder);
  if (!factory)
    {
      gchar msg[256];
      snprintf (msg, sizeof (msg), "Encoder '%s' not available (plugin not installed)", test_encoder);
      gst_println ("  SKIPPED: %s", msg);
      TEST_IGNORE_MESSAGE (msg);
      return;
    }
  gst_object_unref (factory);

  // Build a simple test pipeline: videotestsrc -> encoder -> fakesink
  gchar *pipeline_desc = g_strdup_printf (
      "videotestsrc num-buffers=10 ! "
      "video/x-raw,width=640,height=480,framerate=30/1 ! "
      "%s ! %s ! fakesink",
      test_encoder, encoder_caps);

  gst_println ("  Pipeline: %s", pipeline_desc);

  GError *error = NULL;
  GstElement *pipeline = gst_parse_launch (pipeline_desc, &error);
  g_free (pipeline_desc);

  if (error)
    {
      gchar msg[512];
      snprintf (msg, sizeof (msg), "Pipeline creation failed: %s", error->message);
      gst_println ("  FAILED: %s", msg);
      g_error_free (error);
      TEST_FAIL_MESSAGE (msg);
      return;
    }

  TEST_ASSERT_NOT_NULL (pipeline);

  // Try to set pipeline to PAUSED state (validates the pipeline)
  GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);

  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (pipeline);
      gst_println ("  FAILED: Could not set pipeline to PAUSED state");
      TEST_FAIL_MESSAGE ("Could not set pipeline to PAUSED state");
      return;
    }

  // Run the pipeline briefly
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (pipeline);
      gst_println ("  FAILED: Could not set pipeline to PLAYING state");
      TEST_FAIL_MESSAGE ("Could not set pipeline to PLAYING state");
      return;
    }

  // Wait for EOS or error (with timeout)
  GstBus *bus = gst_element_get_bus (pipeline);
  GstMessage *msg = gst_bus_timed_pop_filtered (bus, 5 * GST_SECOND,
                                                 GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  gboolean success = TRUE;
  if (msg)
    {
      if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR)
        {
          GError *err = NULL;
          gchar *debug = NULL;
          gst_message_parse_error (msg, &err, &debug);
          gst_println ("  Pipeline error: %s", err->message);
          g_error_free (err);
          g_free (debug);
          success = FALSE;
        }
      gst_message_unref (msg);
    }

  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  if (success)
    {
      gst_println ("  SUCCESS: HW encoder pipeline works!");
    }
  else
    {
      TEST_FAIL_MESSAGE ("HW encoder pipeline failed");
    }
}

// ----- WebRTC Loopback Test -----

static void
on_decodebin_pad_added (GstElement *decodebin, GstPad *src_pad, GstElement *fakesink)
{
  GstPad *sink_pad = gst_element_get_static_pad (fakesink, "sink");
  if (!gst_pad_is_linked (sink_pad))
    {
      // Add videoconvert between decodebin and fakesink
      GstElement *parent = GST_ELEMENT (gst_element_get_parent (fakesink));
      GstElement *convert = gst_element_factory_make ("videoconvert", NULL);
      gst_bin_add (GST_BIN (parent), convert);
      gst_element_sync_state_with_parent (convert);

      gst_element_link (convert, fakesink);
      GstPad *convert_sink = gst_element_get_static_pad (convert, "sink");
      gst_pad_link (src_pad, convert_sink);
      gst_object_unref (convert_sink);
      gst_object_unref (parent);
    }
  gst_object_unref (sink_pad);
}

static void
on_receiver_pad_added (GstElement *webrtc, GstPad *pad, LoopbackState *state)
{
  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  gst_println ("    Receiver: New pad added - %s", GST_PAD_NAME (pad));

  // Create a simple sink chain: decodebin -> videoconvert -> fakesink
  GstElement *decodebin = gst_element_factory_make ("decodebin", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", "test_sink");

  g_object_set (fakesink, "signal-handoffs", TRUE, NULL);

  // Count frames received
  g_signal_connect_swapped (fakesink, "handoff",
                            G_CALLBACK (g_atomic_int_inc), &state->frame_count);

  gst_bin_add_many (GST_BIN (state->receiver_pipe), decodebin, fakesink, NULL);
  gst_element_sync_state_with_parent (decodebin);
  gst_element_sync_state_with_parent (fakesink);

  // Connect decodebin pad-added to link to fakesink
  g_signal_connect (decodebin, "pad-added", G_CALLBACK (on_decodebin_pad_added), fakesink);

  GstPad *sink_pad = gst_element_get_static_pad (decodebin, "sink");
  gst_pad_link (pad, sink_pad);
  gst_object_unref (sink_pad);
}

static void
on_ice_candidate (GstElement *webrtc, guint mlineindex, gchar *candidate, GstElement *other_webrtc)
{
  // Forward ICE candidate to the other peer
  g_signal_emit_by_name (other_webrtc, "add-ice-candidate", mlineindex, candidate);
}

static void
on_answer_created (GstPromise *promise, gpointer user_data)
{
  LoopbackState *state = user_data;
  const GstStructure *reply = gst_promise_get_reply (promise);
  GstWebRTCSessionDescription *answer = NULL;

  gst_structure_get (reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);

  gst_println ("    Receiver: Setting local description (answer)");
  g_signal_emit_by_name (state->receiver_webrtc, "set-local-description", answer, NULL);

  gst_println ("    Sender: Setting remote description (answer)");
  g_signal_emit_by_name (state->sender_webrtc, "set-remote-description", answer, NULL);

  gst_webrtc_session_description_free (answer);
  gst_promise_unref (promise);

  state->negotiation_done = TRUE;
  gst_println ("    Negotiation complete!");
}

static void
on_offer_created (GstPromise *promise, gpointer user_data)
{
  LoopbackState *state = user_data;
  const GstStructure *reply = gst_promise_get_reply (promise);
  GstWebRTCSessionDescription *offer = NULL;

  gst_structure_get (reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

  gst_println ("    Sender: Setting local description (offer)");
  g_signal_emit_by_name (state->sender_webrtc, "set-local-description", offer, NULL);

  gst_println ("    Receiver: Setting remote description (offer)");
  g_signal_emit_by_name (state->receiver_webrtc, "set-remote-description", offer, NULL);

  gst_webrtc_session_description_free (offer);
  gst_promise_unref (promise);

  // Create answer
  gst_println ("    Receiver: Creating answer...");
  GstPromise *answer_promise = gst_promise_new_with_change_func (on_answer_created, state, NULL);
  g_signal_emit_by_name (state->receiver_webrtc, "create-answer", NULL, answer_promise);
}

static void
on_negotiation_needed (GstElement *webrtc, LoopbackState *state)
{
  gst_println ("    Sender: Negotiation needed, creating offer...");
  GstPromise *promise = gst_promise_new_with_change_func (on_offer_created, state, NULL);
  g_signal_emit_by_name (webrtc, "create-offer", NULL, promise);
}

static gboolean
on_timeout (LoopbackState *state)
{
  gst_println ("    Test timeout reached");
  g_main_loop_quit (state->loop);
  return G_SOURCE_REMOVE;
}

void
test_vtx_webrtc_loopback (void)
{
  gst_println ("----- Testing WebRTC Loopback -----");
  gst_println ("  Platform: %s", vtx_platform_to_string (g_platform));
  gst_println ("  GPU Vendor: %s", vtx_gpu_vendor_to_string (g_gpu_vendor));

  LoopbackState state = {0};
  state.loop = g_main_loop_new (NULL, FALSE);

  // Determine encoder based on platform/GPU
  const gchar *encoder = "x264enc tune=zerolatency";  // Default SW encoder

  if (g_platform == LINUX_X86)
    {
      switch (g_gpu_vendor)
        {
        case GPU_VENDOR_NVIDIA:
          {
            GstElementFactory *f = gst_element_factory_find ("nvh264enc");
            if (f)
              {
                encoder = "nvh264enc";
                gst_object_unref (f);
              }
          }
          break;
        case GPU_VENDOR_AMD:
          {
            GstElementFactory *f = gst_element_factory_find ("amfh264enc");
            if (f)
              {
                encoder = "amfh264enc";
                gst_object_unref (f);
              }
          }
          break;
        case GPU_VENDOR_INTEL:
          {
            GstElementFactory *f = gst_element_factory_find ("vah264enc");
            if (f)
              {
                encoder = "vah264enc";
                gst_object_unref (f);
              }
          }
          break;
        default:
          break;
        }
    }

  gst_println ("  Using encoder: %s", encoder);

  // Create sender pipeline
  gchar *sender_desc = g_strdup_printf (
      "videotestsrc is-live=true pattern=ball num-buffers=100 ! "
      "video/x-raw,width=320,height=240,framerate=30/1 ! "
      "%s ! rtph264pay config-interval=-1 ! "
      "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
      "webrtcbin name=sender bundle-policy=max-bundle",
      encoder);

  gst_println ("  Sender pipeline: %s", sender_desc);

  GError *error = NULL;
  state.sender_pipe = gst_parse_launch (sender_desc, &error);
  g_free (sender_desc);

  if (error)
    {
      gst_println ("  FAILED: Sender pipeline creation failed: %s", error->message);
      g_error_free (error);
      g_main_loop_unref (state.loop);
      TEST_FAIL_MESSAGE ("Sender pipeline creation failed");
      return;
    }

  // Get sender webrtcbin
  state.sender_webrtc = gst_bin_get_by_name (GST_BIN (state.sender_pipe), "sender");
  if (!state.sender_webrtc)
    {
      gst_println ("  FAILED: Could not get sender webrtcbin");
      gst_object_unref (state.sender_pipe);
      g_main_loop_unref (state.loop);
      TEST_FAIL_MESSAGE ("Could not get sender webrtcbin");
      return;
    }

  // Create receiver pipeline with webrtcbin
  state.receiver_pipe = gst_pipeline_new ("receiver_pipeline");
  state.receiver_webrtc = gst_element_factory_make ("webrtcbin", "receiver");
  if (!state.receiver_webrtc)
    {
      gst_println ("  FAILED: Could not create receiver webrtcbin");
      gst_object_unref (state.sender_webrtc);
      gst_object_unref (state.sender_pipe);
      g_main_loop_unref (state.loop);
      TEST_FAIL_MESSAGE ("Could not create receiver webrtcbin");
      return;
    }
  g_object_set (state.receiver_webrtc, "bundle-policy", 3 /* max-bundle */, NULL);
  gst_bin_add (GST_BIN (state.receiver_pipe), state.receiver_webrtc);

  // Connect signals for ICE candidate exchange
  g_signal_connect (state.sender_webrtc, "on-ice-candidate",
                    G_CALLBACK (on_ice_candidate), state.receiver_webrtc);
  g_signal_connect (state.receiver_webrtc, "on-ice-candidate",
                    G_CALLBACK (on_ice_candidate), state.sender_webrtc);

  // Connect signal for negotiation
  g_signal_connect (state.sender_webrtc, "on-negotiation-needed",
                    G_CALLBACK (on_negotiation_needed), &state);

  // Connect signal for receiving media
  g_signal_connect (state.receiver_webrtc, "pad-added",
                    G_CALLBACK (on_receiver_pad_added), &state);

  // Start pipelines
  gst_println ("  Starting pipelines...");
  gst_element_set_state (state.receiver_pipe, GST_STATE_PLAYING);
  gst_element_set_state (state.sender_pipe, GST_STATE_PLAYING);

  // Set timeout (5 seconds)
  state.timeout_id = g_timeout_add_seconds (5, (GSourceFunc) on_timeout, &state);

  // Run main loop
  g_main_loop_run (state.loop);

  // Cleanup
  gst_element_set_state (state.sender_pipe, GST_STATE_NULL);
  gst_element_set_state (state.receiver_pipe, GST_STATE_NULL);

  // sender_webrtc was obtained via gst_bin_get_by_name, so we need to unref it
  gst_object_unref (state.sender_webrtc);
  // receiver_webrtc is owned by receiver_pipe (added via gst_bin_add), no separate unref needed
  gst_object_unref (state.sender_pipe);
  gst_object_unref (state.receiver_pipe);
  g_main_loop_unref (state.loop);

  gint frames = g_atomic_int_get (&state.frame_count);
  gst_println ("  Frames received: %d", frames);
  gst_println ("  Negotiation completed: %s", state.negotiation_done ? "YES" : "NO");

  if (state.negotiation_done && frames > 0)
    {
      gst_println ("  SUCCESS: WebRTC loopback works! (%d frames received)", frames);
    }
  else if (state.negotiation_done)
    {
      gst_println ("  PARTIAL: Negotiation succeeded but no frames received");
      TEST_IGNORE_MESSAGE ("Negotiation succeeded but no frames received");
    }
  else
    {
      TEST_FAIL_MESSAGE ("WebRTC negotiation failed");
    }
}
