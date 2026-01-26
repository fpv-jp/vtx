#include "headers/utils.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "headers/data_channel.h"
#include "headers/wpa.h"

// Global platform variable (detected at runtime)
PlatformType g_platform = UNKNOWN;

// Global GPU vendor variable (detected at runtime, LINUX_X86 only)
GpuVendor g_gpu_vendor = GPU_VENDOR_UNKNOWN;

// --- vtx_detect_gpu_vendor ----------------------------------
// Detect GPU vendor by reading /sys/class/drm/card*/device/vendor
// Vendor IDs: Intel=0x8086, AMD=0x1002, NVIDIA=0x10de
void vtx_detect_gpu_vendor(void)
{
  if (g_platform != LINUX_X86)
  {
    return;
  }

  gst_println("----- Detecting GPU vendor -----");

  // Check DRM cards for GPU vendor
  for (int card = 0; card < 16; card++)
  {
    char vendor_path[256];
    snprintf(vendor_path, sizeof(vendor_path), "/sys/class/drm/card%d/device/vendor", card);

    FILE *f = fopen(vendor_path, "r");
    if (!f) continue;

    char vendor_id[32] = {0};
    if (fgets(vendor_id, sizeof(vendor_id), f))
    {
      fclose(f);

      // Parse vendor ID (format: 0x1234)
      unsigned int vid = 0;
      if (sscanf(vendor_id, "0x%x", &vid) == 1 || sscanf(vendor_id, "%x", &vid) == 1)
      {
        switch (vid)
        {
          case 0x8086:  // Intel
            g_gpu_vendor = GPU_VENDOR_INTEL;
            gst_println("  Detected GPU vendor: Intel (card%d)", card);
            return;
          case 0x1002:  // AMD
            g_gpu_vendor = GPU_VENDOR_AMD;
            gst_println("  Detected GPU vendor: AMD (card%d)", card);
            return;
          case 0x10de:  // NVIDIA
            g_gpu_vendor = GPU_VENDOR_NVIDIA;
            gst_println("  Detected GPU vendor: NVIDIA (card%d)", card);
            return;
          default:
            break;
        }
      }
    }
    else
    {
      fclose(f);
    }
  }

  gst_printerrln("  WARNING: Could not detect GPU vendor, using UNKNOWN");
}

// --- vtx_gpu_vendor_to_string ----------------------------------
gchar *vtx_gpu_vendor_to_string(GpuVendor vendor)
{
  switch (vendor)
  {
    case GPU_VENDOR_INTEL:
      return "INTEL";
    case GPU_VENDOR_AMD:
      return "AMD";
    case GPU_VENDOR_NVIDIA:
      return "NVIDIA";
    default:
      return "UNKNOWN";
  }
}

// --- vtx_detect_platform ----------------------------------
void vtx_detect_platform(void)
{
  gst_println("----- Detecting platform -----");

  // Try /proc/device-tree/model first (Linux ARM devices)
  FILE *f = fopen("/proc/device-tree/model", "r");
  if (f)
  {
    char model[256] = {0};
    if (fgets(model, sizeof(model), f))
    {
      fclose(f);
      gst_println("  Device model: %s", model);

      // Raspberry Pi detection
      if (strstr(model, "Raspberry Pi 5"))
      {
        g_platform = RASPBERRY_PI_5;
      }
      else if (strstr(model, "Raspberry Pi Compute Module 4"))
      {
        g_platform = RASPBERRY_PI_4CM;
      }
      else if (strstr(model, "Raspberry Pi 4"))
      {
        g_platform = RASPBERRY_PI_4B;
      }
      // Radxa ROCK 5 series detection
      else if (strstr(model, "ROCK 5"))
      {
        g_platform = RADXA_ROCK_5B;  // Use 5B for all ROCK 5 series
      }
      // Jetson detection (placeholder - adjust based on actual model strings)
      else if (strstr(model, "Jetson Orin"))
      {
        g_platform = JETSON_ORIN_NANO_SUPER;
      }
      else if (strstr(model, "Jetson Nano"))
      {
        g_platform = JETSON_NANO_2GB;
      }

      if (g_platform != UNKNOWN)
      {
        gst_println("  Detected platform: %s", vtx_platform_to_string(g_platform));
        return;
      }
    }
    else
    {
      fclose(f);
    }
  }

  // Fallback: Check for macOS or Linux x86
#ifdef __APPLE__
  g_platform = APPLE_MAC;
  gst_println("  Detected platform: %s (macOS)", vtx_platform_to_string(g_platform));
  return;
#endif

#if defined(__linux__) && defined(__x86_64__)
  g_platform = LINUX_X86;
  gst_println("  Detected platform: %s (x86_64)", vtx_platform_to_string(g_platform));
  return;
#endif

  gst_printerrln("  WARNING: Could not detect platform, using UNKNOWN");
}

// --- vtx_platform_to_string ----------------------------------
gchar *vtx_platform_to_string(PlatformType platform)
{
  switch (platform)
  {
    case APPLE_MAC:
      return "APPLE_MAC";
    case LINUX_X86:
      return "LINUX_X86";
    case RASPBERRY_PI_4B:
      return "RASPBERRY_PI_4B";
    case RASPBERRY_PI_4CM:
      return "RASPBERRY_PI_4CM";
    case RASPBERRY_PI_5:
      return "RASPBERRY_PI_5";
    case JETSON_NANO_2GB:
      return "JETSON_NANO_2GB";
    case JETSON_ORIN_NANO_SUPER:
      return "JETSON_ORIN_NANO_SUPER";
    case RADXA_ROCK_5B:
      return "RADXA_ROCK_5B";
    case RADXA_ROCK_5T:
      return "RADXA_ROCK_5T";
    default:
      return "UNKNOWN";
  }
}

// --- vtx_ws_send ----------------------------------
void vtx_ws_send(SoupWebsocketConnection *conn, int type, const gchar *ws1Id, const gchar *ws2Id, JsonObject *data)
{
  JsonObject *msg = json_object_new();

  json_object_set_int_member(msg, "type", type);
  json_object_set_string_member(msg, "ws1Id", ws1Id);
  json_object_set_string_member(msg, "ws2Id", ws2Id);

  GList *members = json_object_get_members(data);
  for (GList *l = members; l != NULL; l = l->next)
  {
    const gchar *key = l->data;
    JsonNode *value = json_object_get_member(data, key);
    json_object_set_member(msg, key, json_node_copy(value));
  }

  g_list_free(members);

  JsonNode *root = json_node_new(JSON_NODE_OBJECT);
  json_node_set_object(root, msg);

  JsonGenerator *gen = json_generator_new();
  json_generator_set_pretty(gen, TRUE);
  json_generator_set_root(gen, root);

  gsize length = 0;
  gchar *json_str = json_generator_to_data(gen, &length);

  soup_websocket_connection_send_text(conn, json_str);

  g_free(json_str);
  g_object_unref(gen);
  json_node_free(root);
  json_object_unref(msg);
}

// --- vtx_cleanup_connection ----------------------------------
gboolean vtx_cleanup_connection(const gchar *msg)
{
  if (msg) gst_printerrln("%s", msg);

  // Cleanup data channels
  vtx_dc_cleanup();

  // Cleanup MSP connection
  vtx_msp_cleanup_global();

  // Cleanup WPA supplicant
  vtx_wpa_supplicant_cleanup();

  // Cleanup GStreamer pipeline and webrtc
  if (pipeline)
  {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = NULL;
  }

  if (webrtc)
  {
    g_object_unref(webrtc);
    webrtc = NULL;
  }

  // Clear ws2Id (session ID for receiver)
  if (ws2Id)
  {
    g_free(ws2Id);
    ws2Id = NULL;
  }

  // Reset application state to SERVER_REGISTERED (ready for new connection)
  app_state = SERVER_REGISTERED;

  gst_println("Connection cleaned up, ready for reconnection");

  return TRUE;
}

// --- vtx_cleanup_and_quit_loop ----------------------------------
gboolean vtx_cleanup_and_quit_loop(const gchar *msg, AppState state)
{
  if (msg) gst_printerrln("%s", msg);

  if (state > 0) app_state = state;

  // Cleanup data channels
  vtx_dc_cleanup();

  // Cleanup MSP connection
  vtx_msp_cleanup_global();

  // Cleanup WPA supplicant
  vtx_wpa_supplicant_cleanup();

  if (ws_conn)
  {
    if (soup_websocket_connection_get_state(ws_conn) == SOUP_WEBSOCKET_STATE_OPEN) soup_websocket_connection_close(ws_conn, 1000, "");
    g_clear_object(&ws_conn);
  }

  if (loop)
  {
    g_main_loop_quit(loop);
    g_clear_pointer(&loop, g_main_loop_unref);
  }

  return TRUE;
}

// --- vtx_check_gst_plugins ----------------------------------
gboolean vtx_check_gst_plugins(void)
{
  gst_println("----- Check GStreamer plugins -----");

  GstRegistry *registry = gst_registry_get();
  gboolean ret = TRUE;
  GstPlugin *plugin;

  // Check for gst-device-monitor-1.0 executable
  gchar *device_monitor_path = g_find_program_in_path("gst-device-monitor-1.0");
  if (!device_monitor_path)
  {
    gst_printerrln("Required GStreamer tool 'gst-device-monitor-1.0' not found in PATH");
    gst_printerrln("  Install with: apt install gstreamer1.0-plugins-base-apps (Debian/Ubuntu)");
    ret = FALSE;
  }
  else
  {
    gst_println("  -> OK gst-device-monitor-1.0");
    g_free(device_monitor_path);
  }

  GPtrArray *needed = g_ptr_array_new_with_free_func(g_free);

  const gchar *common_plugins[] = {
      "nice",             //
      "webrtc",           //
      "rtp",              //
      "rtpmanager",       //
      "dtls",             //
      "srtp",             //
      "videoparsersbad",  //
      "opus",             //
      "opusparse",        //
      NULL                //
  };
  for (int i = 0; common_plugins[i]; i++)
  {
    g_ptr_array_add(needed, g_strdup(common_plugins[i]));
  }

  switch (g_platform)
  {
    case APPLE_MAC:
      g_ptr_array_add(needed, g_strdup("applemedia"));
      g_ptr_array_add(needed, g_strdup("vpx"));
      g_ptr_array_add(needed, g_strdup("svtav1"));
      g_ptr_array_add(needed, g_strdup("osxvideo"));
      g_ptr_array_add(needed, g_strdup("osxaudio"));
      break;

    case LINUX_X86:
      g_ptr_array_add(needed, g_strdup("video4linux2"));// for v4l2src
      switch (g_gpu_vendor)
      {
        case GPU_VENDOR_INTEL:
          g_ptr_array_add(needed, g_strdup("va"));  // Intel VA-API HW encoders
          break;
        case GPU_VENDOR_AMD:
          g_ptr_array_add(needed, g_strdup("amfcodec"));  // AMD AMF HW encoders
          break;
        case GPU_VENDOR_NVIDIA:
          g_ptr_array_add(needed, g_strdup("nvcodec"));  // NVIDIA NVCODEC HW encoders
          break;
        default:
          gst_printerrln("  WARNING: Unknown GPU vendor, skipping HW encoder plugin check");
          break;
      }
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    case RASPBERRY_PI_4B:
    case RASPBERRY_PI_4CM:
      g_ptr_array_add(needed, g_strdup("libcamera"));
      g_ptr_array_add(needed, g_strdup("video4linux2"));  // for v4l2h264enc
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    case RASPBERRY_PI_5:
      g_ptr_array_add(needed, g_strdup("libcamera"));
      g_ptr_array_add(needed, g_strdup("openh264"));
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    case JETSON_NANO_2GB:
      g_ptr_array_add(needed, g_strdup("nvarguscamerasrc")); // MIPI CSI
      g_ptr_array_add(needed, g_strdup("nvv4l2camerasrc")); // USB
      g_ptr_array_add(needed, g_strdup("nveglstreamsrc")); // Overlay
      g_ptr_array_add(needed, g_strdup("nvvideo4linux2"));
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    case JETSON_ORIN_NANO_SUPER:
      g_ptr_array_add(needed, g_strdup("nvarguscamerasrc")); // MIPI CSI
      g_ptr_array_add(needed, g_strdup("nvv4l2camerasrc")); // USB
      g_ptr_array_add(needed, g_strdup("nveglstreamsrc")); // Overlay
      g_ptr_array_add(needed, g_strdup("openh264"));
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    case RADXA_ROCK_5B:
    case RADXA_ROCK_5T:
      g_ptr_array_add(needed, g_strdup("video4linux2"));// for v4l2src
      g_ptr_array_add(needed, g_strdup("rockchipmpp"));
      g_ptr_array_add(needed, g_strdup("alsa"));
      break;

    default:
      break;
  }

  for (guint i = 0; i < needed->len; i++)
  {
    const gchar *plugin_name = g_ptr_array_index(needed, i);
    plugin = gst_registry_find_plugin(registry, plugin_name);

    if (!plugin)
    {
      gst_printerrln("Required GStreamer plugin '%s' not found", plugin_name);
      ret = FALSE;
    }
    else
    {
      gst_println("  -> OK %s", plugin_name);
      gst_object_unref(plugin);
    }
  }

  g_ptr_array_free(needed, TRUE);
  return ret;
}

// --- print_json_object ----------------------------------
void print_json_object(JsonObject *obj)
{
  JsonNode *node = json_node_new(JSON_NODE_OBJECT);
  json_node_set_object(node, obj);
  gchar *str = json_to_string(node, TRUE);

  setlocale(LC_ALL, "en_US.UTF-8");
  gst_println("%s", str);

  g_free(str);
  json_node_free(node);
}

// --- print_json_array ----------------------------------
void print_json_array(JsonArray *array)
{
  JsonNode *node = json_node_new(JSON_NODE_ARRAY);
  json_node_set_array(node, array);
  gchar *str = json_to_string(node, TRUE);

  setlocale(LC_ALL, "en_US.UTF-8");
  gst_println("%s", str);

  g_free(str);
  json_node_free(node);
}
