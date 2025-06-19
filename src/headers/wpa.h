#pragma once

#include <glib.h>
#include <gst/gst.h>
#include <stdbool.h>

// wpa_supplicant control interface structures
typedef struct WpaSupplicant WpaSupplicant;

// Network mode types
typedef enum
{
  WPA_MODE_UNKNOWN = 0,
  WPA_MODE_P2P_CLIENT = 1,  // P2P Client mode (192.168.49.0/24)
  WPA_MODE_STA = 2          // Station/SoftAP Client mode (192.168.50.0/24)
} WpaNetworkMode;

// Signal quality information
typedef struct
{
  int rssi;              // Signal strength in dBm
  int link_speed;        // Link speed in Mbps
  int noise;             // Noise level in dBm
  int frequency;         // Frequency in MHz
  char bssid[18];        // AP/GO BSSID (MAC address)
} WpaSignalInfo;

// Connection status information
typedef struct
{
  WpaNetworkMode mode;
  char state[32];        // wpa_state (e.g., "COMPLETED")
  char ip_address[16];   // Current IP address
  char ssid[256];        // Connected SSID/P2P group
  bool connected;        // Connection status
  WpaSignalInfo signal;  // Signal information
} WpaStatus;

// Callback function types
typedef void (*WpaSignalChangeCallback)(WpaSupplicant *wpa, const WpaSignalInfo *signal, gpointer user_data);
typedef void (*WpaStatusChangeCallback)(WpaSupplicant *wpa, const WpaStatus *status, gpointer user_data);

// Create and initialize wpa_supplicant control interface
WpaSupplicant *wpa_supplicant_new(const char *interface_name, const char *ctrl_path);

// Free wpa_supplicant control interface
void wpa_supplicant_free(WpaSupplicant *wpa);

// Start monitoring events
gboolean wpa_supplicant_start_monitor(WpaSupplicant *wpa);

// Stop monitoring events
void wpa_supplicant_stop_monitor(WpaSupplicant *wpa);

// Get current status
gboolean wpa_supplicant_get_status(WpaSupplicant *wpa, WpaStatus *status);

// Get current signal information
gboolean wpa_supplicant_get_signal_poll(WpaSupplicant *wpa, WpaSignalInfo *signal);

// Set signal change callback
void wpa_supplicant_set_signal_callback(WpaSupplicant *wpa, WpaSignalChangeCallback callback, gpointer user_data);

// Set status change callback
void wpa_supplicant_set_status_callback(WpaSupplicant *wpa, WpaStatusChangeCallback callback, gpointer user_data);

// Get network mode (P2P or SoftAP)
WpaNetworkMode wpa_supplicant_get_mode(WpaSupplicant *wpa);

// Check if connected
gboolean wpa_supplicant_is_connected(WpaSupplicant *wpa);

// Global instance for data channel integration
extern WpaSupplicant *g_wpa_supplicant;
extern GObject *dc_wpa_supplicant;
extern guint timeout_id_wpa_supplicant;

// Initialize global wpa_supplicant instance
gboolean vtx_wpa_supplicant_init(const char *interface_name);

// Cleanup global wpa_supplicant instance
void vtx_wpa_supplicant_cleanup(void);

// Send wpa_supplicant status via data channel
gboolean vtx_send_wpa_status(gpointer user_data);
