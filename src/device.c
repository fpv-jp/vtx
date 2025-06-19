#include <arpa/inet.h>
#include <ifaddrs.h>
#include <locale.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/wireless.h>
#endif

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

// --- WiFi detection helper functions ----------------------------------
// Check if an interface is a WiFi interface
static gboolean is_wifi_interface(const gchar *ifname)
{
  // Check for WiFi Direct interfaces
  if (g_str_has_prefix(ifname, "p2p-")) return TRUE;

#ifdef __linux__
  // Linux: Check if /sys/class/net/<interface>/wireless exists
  gchar *wireless_path = g_strdup_printf("/sys/class/net/%s/wireless", ifname);
  struct stat st;
  gboolean exists = (stat(wireless_path, &st) == 0);
  g_free(wireless_path);

  if (exists) return TRUE;
#endif

  // Fallback: Use ioctl to check for wireless extensions
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) return FALSE;

#ifdef __linux__
  struct iwreq wrq;
  memset(&wrq, 0, sizeof(wrq));
  g_strlcpy(wrq.ifr_name, ifname, IFNAMSIZ);

  gboolean is_wireless = (ioctl(sock, SIOCGIWNAME, &wrq) >= 0);
  close(sock);
  return is_wireless;
#else
  // For non-Linux systems, we can't reliably detect WiFi without platform-specific code
  // Fall back to name-based detection
  close(sock);
  return (g_str_has_prefix(ifname, "wlan") || g_str_has_prefix(ifname, "wl") || g_str_has_prefix(ifname, "wifi"));
#endif
}

// Check if an interface is a physical Ethernet interface
static gboolean is_ethernet_interface(const gchar *ifname)
{
  // Skip virtual/special interfaces
  if (g_str_has_prefix(ifname, "veth") ||    //
      g_str_has_prefix(ifname, "br-") ||     //
      g_str_has_prefix(ifname, "docker") ||  //
      g_str_has_prefix(ifname, "virbr") ||   //
      g_str_has_prefix(ifname, "tun") ||     //
      g_str_has_prefix(ifname, "tap") ||     //
      g_str_has_prefix(ifname, "utun") ||    //
      g_str_has_prefix(ifname, "bridge") ||  //
      g_str_has_prefix(ifname, "awdl") ||    //
      g_str_has_prefix(ifname, "llw"))
  {
    return FALSE;
  }

  // If it's WiFi, it's not Ethernet
  if (is_wifi_interface(ifname)) return FALSE;

#ifdef __linux__
  // Linux: Check if /sys/class/net/<interface>/device exists
  // Physical devices have this symlink
  gchar *device_path = g_strdup_printf("/sys/class/net/%s/device", ifname);
  struct stat st;
  gboolean is_physical = (lstat(device_path, &st) == 0 && S_ISLNK(st.st_mode));
  g_free(device_path);

  if (is_physical) return TRUE;
#endif

  // Fallback: Common Ethernet interface name patterns
  return (g_str_has_prefix(ifname, "eth") ||  //
          g_str_has_prefix(ifname, "en") ||   //
          g_str_has_prefix(ifname, "em") ||   //
          g_str_has_prefix(ifname, "eno") ||  //
          g_str_has_prefix(ifname, "ens") ||  //
          g_str_has_prefix(ifname, "enp") ||  //
          g_str_has_prefix(ifname, "enx"));
}

// --- WiFi helper structures and functions ----------------------------------
typedef struct
{
  gboolean connected;
  gchar *wpa_state;
  gchar *mode;
  gchar *ssid;
  gint freq;
  gchar *bssid;
  gboolean is_p2p;
  // Signal poll data
  gint rssi;
  gint linkspeed;
  gint noise;
  gchar *width;
  gint center_frq1;
  gint avg_rssi;
  gint avg_beacon_rssi;
  gboolean has_signal_data;
} WifiInfo;

static void free_wifi_info(WifiInfo *info)
{
  if (!info) return;
  g_free(info->wpa_state);
  g_free(info->mode);
  g_free(info->ssid);
  g_free(info->bssid);
  g_free(info->width);
  g_free(info);
}

// Convert WiFi frequency (MHz) to channel number
static gint freq_to_channel(gint freq)
{
  if (freq >= 2412 && freq <= 2484)
  {
    // 2.4 GHz band
    if (freq == 2484) return 14;
    return (freq - 2407) / 5;
  }
  else if (freq >= 5170 && freq <= 5825)
  {
    // 5 GHz band
    return (freq - 5000) / 5;
  }
  return 0;  // Unknown
}

// Get WiFi connection information using wpa_cli
static WifiInfo *get_wifi_info(const gchar *interface)
{
  gchar *cmd = g_strdup_printf("wpa_cli -i %s status 2>/dev/null", interface);
  FILE *fp = popen(cmd, "r");
  g_free(cmd);

  if (!fp) return NULL;

  WifiInfo *info = g_new0(WifiInfo, 1);
  char line[256];

  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);

    if (g_str_has_prefix(line, "wpa_state="))
    {
      info->wpa_state = g_strdup(line + 10);
      if (g_strcmp0(info->wpa_state, "COMPLETED") == 0) info->connected = TRUE;
    }
    else if (g_str_has_prefix(line, "mode="))
    {
      info->mode = g_strdup(line + 5);
    }
    else if (g_str_has_prefix(line, "ssid="))
    {
      info->ssid = g_strdup(line + 5);
      // Check if this is a WiFi Direct connection
      if (info->ssid && g_str_has_prefix(info->ssid, "DIRECT-")) info->is_p2p = TRUE;
    }
    else if (g_str_has_prefix(line, "freq="))
    {
      info->freq = atoi(line + 5);
    }
    else if (g_str_has_prefix(line, "bssid="))
    {
      info->bssid = g_strdup(line + 6);
    }
    else if (g_str_has_prefix(line, "p2p_device_address="))
    {
      // If p2p_device_address exists, it's definitely a P2P connection
      info->is_p2p = TRUE;
    }
  }

  pclose(fp);

  // Get signal poll data if connected
  if (info->connected)
  {
    gchar *signal_cmd = g_strdup_printf("wpa_cli -i %s signal_poll 2>/dev/null", interface);
    FILE *signal_fp = popen(signal_cmd, "r");
    g_free(signal_cmd);

    if (signal_fp)
    {
      char signal_line[256];
      while (fgets(signal_line, sizeof(signal_line), signal_fp))
      {
        g_strchomp(signal_line);

        if (g_str_has_prefix(signal_line, "RSSI="))
        {
          info->rssi = atoi(signal_line + 5);
          info->has_signal_data = TRUE;
        }
        else if (g_str_has_prefix(signal_line, "LINKSPEED="))
        {
          info->linkspeed = atoi(signal_line + 10);
        }
        else if (g_str_has_prefix(signal_line, "NOISE="))
        {
          info->noise = atoi(signal_line + 6);
        }
        else if (g_str_has_prefix(signal_line, "WIDTH="))
        {
          info->width = g_strdup(signal_line + 6);
        }
        else if (g_str_has_prefix(signal_line, "CENTER_FRQ1="))
        {
          info->center_frq1 = atoi(signal_line + 12);
        }
        else if (g_str_has_prefix(signal_line, "AVG_RSSI="))
        {
          info->avg_rssi = atoi(signal_line + 9);
        }
        else if (g_str_has_prefix(signal_line, "AVG_BEACON_RSSI="))
        {
          info->avg_beacon_rssi = atoi(signal_line + 16);
        }
      }
      pclose(signal_fp);
    }
  }

  return info;
}

// --- Ethernet helper structures and functions ----------------------------------
typedef struct
{
  gchar *speed;
  gchar *duplex;
  gchar *auto_negotiation;
  gchar *master_slave_status;
  gchar *link_detected;
} EthernetInfo;

static void free_ethernet_info(EthernetInfo *info)
{
  if (!info) return;

  g_free(info->duplex);
  g_free(info->speed);
  g_free(info->auto_negotiation);
  g_free(info->master_slave_status);
  g_free(info->link_detected);
  g_free(info);
}

static gchar *ethtool_value_from_line(const gchar *line)
{
  const gchar *colon = strchr(line, ':');
  if (!colon) return NULL;

  colon++;
  while (*colon == ' ' || *colon == '\t') colon++;

  gchar *value = g_strdup(colon);
  if (!value) return NULL;

  return g_strstrip(value);
}

// Get Ethernet details using ethtool
static EthernetInfo *get_ethernet_info(const gchar *interface)
{
  gchar *cmd = g_strdup_printf("ethtool %s 2>/dev/null", interface);
  FILE *fp = popen(cmd, "r");
  g_free(cmd);

  if (!fp) return NULL;

  EthernetInfo *info = g_new0(EthernetInfo, 1);
  gboolean has_data = FALSE;

  char line[256];
  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);

    gchar *trimmed = line;
    while (*trimmed && g_ascii_isspace(*trimmed)) trimmed++;

    if (*trimmed == '\0') continue;

    if (g_ascii_strncasecmp(trimmed, "Speed:", 6) == 0)
    {
      g_free(info->speed);
      info->speed = ethtool_value_from_line(trimmed);
      if (info->speed && *info->speed != '\0') has_data = TRUE;
    }
    else if (g_ascii_strncasecmp(trimmed, "Duplex:", 7) == 0)
    {
      g_free(info->duplex);
      info->duplex = ethtool_value_from_line(trimmed);
      if (info->duplex) has_data = TRUE;
    }
    else if (g_ascii_strncasecmp(trimmed, "Auto-negotiation:", 17) == 0)
    {
      g_free(info->auto_negotiation);
      info->auto_negotiation = ethtool_value_from_line(trimmed);
      if (info->auto_negotiation) has_data = TRUE;
    }
    else if (g_ascii_strncasecmp(trimmed, "master-slave status:", 20) == 0)
    {
      g_free(info->master_slave_status);
      info->master_slave_status = ethtool_value_from_line(trimmed);
      if (info->master_slave_status) has_data = TRUE;
    }
    else if (g_ascii_strncasecmp(trimmed, "Link detected:", 14) == 0)
    {
      g_free(info->link_detected);
      info->link_detected = ethtool_value_from_line(trimmed);
      if (info->link_detected) has_data = TRUE;
    }
  }

  pclose(fp);

  if (!has_data)
  {
    free_ethernet_info(info);
    return NULL;
  }

  return info;
}

// --- vtx_network_interfaces_inspection ----------------------------------
JsonArray *vtx_network_interfaces_inspection(void)
{
  struct ifaddrs *ifaddr, *ifa;
  JsonArray *interfaces = json_array_new();

  if (getifaddrs(&ifaddr) == -1)
  {
    gst_printerrln("Failed to get network interfaces");
    return interfaces;
  }

  // Track which interfaces we've already added to avoid duplicates
  GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr == NULL) continue;

    // Skip if we've already processed this interface
    if (g_hash_table_contains(seen, ifa->ifa_name)) continue;

    // Skip loopback interface
    if (strcmp(ifa->ifa_name, "lo") == 0 || strcmp(ifa->ifa_name, "docker0") == 0) continue;

    int family = ifa->ifa_addr->sa_family;

    // Only process IPv4 addresses for simplicity
    if (family == AF_INET)
    {
      JsonObject *iface = json_object_new();
      json_object_set_string_member(iface, "name", ifa->ifa_name);

      // Get IP address
      char host[NI_MAXHOST];
      if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
      {
        json_object_set_string_member(iface, "address", host);
      }

      // Check interface status
      gboolean is_up = (ifa->ifa_flags & IFF_UP) != 0;
      gboolean is_running = (ifa->ifa_flags & IFF_RUNNING) != 0;
      json_object_set_boolean_member(iface, "up", is_up);
      json_object_set_boolean_member(iface, "running", is_running);

      // Determine interface type and get additional information
      gboolean is_wifi = is_wifi_interface(ifa->ifa_name);
      gboolean is_ethernet = is_ethernet_interface(ifa->ifa_name);

      if (is_wifi)
      {
        json_object_set_string_member(iface, "type", "wifi");

        // Get WiFi connection information
        WifiInfo *wifi_info = get_wifi_info(ifa->ifa_name);
        if (wifi_info)
        {
          json_object_set_boolean_member(iface, "connected", wifi_info->connected);

          if (wifi_info->wpa_state) json_object_set_string_member(iface, "wpa_state", wifi_info->wpa_state);

          // Determine WiFi mode (managed AP or WiFi Direct)
          if (wifi_info->is_p2p || g_str_has_prefix(ifa->ifa_name, "p2p-"))
          {
            json_object_set_string_member(iface, "wifi_mode", "p2p");
          }
          else
          {
            json_object_set_string_member(iface, "wifi_mode", "managed");
          }

          if (wifi_info->ssid) json_object_set_string_member(iface, "ssid", wifi_info->ssid);

          if (wifi_info->freq > 0)
          {
            json_object_set_int_member(iface, "frequency", wifi_info->freq);

            gint channel = freq_to_channel(wifi_info->freq);
            if (channel > 0) json_object_set_int_member(iface, "channel", channel);
          }

          if (wifi_info->bssid) json_object_set_string_member(iface, "bssid", wifi_info->bssid);

          // Add signal poll data if available
          if (wifi_info->has_signal_data)
          {
            if (wifi_info->rssi != 0) json_object_set_int_member(iface, "rssi", wifi_info->rssi);

            if (wifi_info->linkspeed > 0) json_object_set_int_member(iface, "linkspeed", wifi_info->linkspeed);

            if (wifi_info->noise != 0 && wifi_info->noise != 9999) json_object_set_int_member(iface, "noise", wifi_info->noise);

            if (wifi_info->width) json_object_set_string_member(iface, "channel_width", wifi_info->width);

            if (wifi_info->center_frq1 > 0) json_object_set_int_member(iface, "center_frequency", wifi_info->center_frq1);

            if (wifi_info->avg_rssi != 0) json_object_set_int_member(iface, "avg_rssi", wifi_info->avg_rssi);

            if (wifi_info->avg_beacon_rssi != 0) json_object_set_int_member(iface, "avg_beacon_rssi", wifi_info->avg_beacon_rssi);
          }

          free_wifi_info(wifi_info);
        }
        else
        {
          // WiFi interface but couldn't get wpa_cli info
          json_object_set_boolean_member(iface, "connected", FALSE);
        }
      }
      else if (is_ethernet)
      {
        json_object_set_string_member(iface, "type", "ethernet");

        // Get Ethernet link status
        EthernetInfo *eth_info = get_ethernet_info(ifa->ifa_name);
        if (eth_info)
        {
          if (eth_info->speed) json_object_set_string_member(iface, "speed", eth_info->speed);

          if (eth_info->duplex) json_object_set_string_member(iface, "duplex", eth_info->duplex);

          if (eth_info->auto_negotiation) json_object_set_string_member(iface, "auto_negotiation", eth_info->auto_negotiation);

          if (eth_info->master_slave_status) json_object_set_string_member(iface, "master_slave_status", eth_info->master_slave_status);

          if (eth_info->link_detected) json_object_set_string_member(iface, "link_detected", eth_info->link_detected);

          free_ethernet_info(eth_info);
        }
      }
      else
      {
        // Other interface types (e.g., docker, bridge)
        json_object_set_string_member(iface, "type", "other");
      }

      json_array_add_object_element(interfaces, iface);

      // Mark this interface as seen
      g_hash_table_insert(seen, g_strdup(ifa->ifa_name), GINT_TO_POINTER(1));
    }
  }

  g_hash_table_destroy(seen);
  freeifaddrs(ifaddr);

  return interfaces;
}
