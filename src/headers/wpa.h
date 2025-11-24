#pragma once

#include <glib.h>
#include <gst/gst.h>
#include <stdbool.h>
#include <json-glib/json-glib.h>

// wpa_supplicant control interface structures
typedef struct WpaSupplicant WpaSupplicant;

#define WPA_RESPONSE_MAX_LEN 4096

// Raw response from wpa_cli commands
typedef struct
{
  char raw[WPA_RESPONSE_MAX_LEN];
} WpaSignalInfo;

// Combined status information
typedef struct
{
  char raw[WPA_RESPONSE_MAX_LEN];  // Raw STATUS output
  WpaSignalInfo signal;            // Raw SIGNAL_POLL output
} WpaStatus;

// Callback function types
typedef void (*WpaSignalChangeCallback)(WpaSupplicant *wpa, const WpaSignalInfo *signal, gpointer user_data);
typedef void (*WpaStatusChangeCallback)(WpaSupplicant *wpa, const WpaStatus *status, gpointer user_data);

typedef void (*VtxWpaKeyValueFunc)(const char *key, const char *value, gpointer user_data);

// Create and initialize wpa_supplicant control interface
WpaSupplicant *vtx_wpa_supplicant_new(const char *interface_name, const char *ctrl_path);

// Free wpa_supplicant control interface
void vtx_wpa_supplicant_free(WpaSupplicant *wpa);

// Start monitoring events
gboolean vtx_wpa_supplicant_start_monitor(WpaSupplicant *wpa);

// Stop monitoring events
void vtx_wpa_supplicant_stop_monitor(WpaSupplicant *wpa);

// Get current status
gboolean vtx_wpa_supplicant_get_status(WpaSupplicant *wpa, WpaStatus *status);

// Get current signal information
gboolean vtx_wpa_supplicant_get_signal_poll(WpaSupplicant *wpa, WpaSignalInfo *signal);

// Global instance for data channel integration
extern WpaSupplicant *g_wpa_supplicant;
extern GObject *dc_wpa_supplicant;
extern guint timeout_id_wpa_supplicant;

// Initialize global wpa_supplicant instance
gboolean vtx_wpa_supplicant_init(const char *interface_name);

// Cleanup global wpa_supplicant instance
void vtx_wpa_supplicant_cleanup(void);

// Send wpa_supplicant status via data channel
gboolean vtx_wpa_send_status(gpointer user_data);
