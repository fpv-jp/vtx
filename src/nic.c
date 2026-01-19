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
#include "headers/nic.h"
#include "headers/wpa.h"

// --- should_skip_interface ----------------------------------
static gboolean vtx_nic_should_skip_interface(const gchar *ifname)
{
  if (!ifname || ifname[0] == '\0') return TRUE;

  static const gchar *exact_skip[] = {"lo", "docker0", NULL};
  for (const gchar **entry = exact_skip; *entry != NULL; entry++)
  {
    if (g_strcmp0(ifname, *entry) == 0) return TRUE;
  }

  static const gchar *prefix_skip[] = {
      "veth",     //
      "br-",      //
      "docker",   //
      "virbr",    //
      "tun",      //
      "tap",      //
      "utun",     //
      "bridge",   //
      "awdl",     //
      "llw",      //
      "vboxnet",  //
      "vmnet"     //
  };
  for (const gchar **prefix = prefix_skip; *prefix != NULL; prefix++)
  {
    if (g_str_has_prefix(ifname, *prefix)) return TRUE;
  }

  return FALSE;
}

// Check if an interface is a WiFi interface
// --- is_wifi_interface ----------------------------------
static gboolean vtx_nic_is_wifi_interface(const gchar *ifname)
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
// --- is_ethernet_interface ----------------------------------
static gboolean vtx_nic_is_ethernet_interface(const gchar *ifname)
{
  if (vtx_nic_should_skip_interface(ifname)) return FALSE;

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
  if (vtx_nic_is_wifi_interface(ifname)) return FALSE;

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
  JsonObject *status;
  JsonObject *signal;
} WifiInfo;

// --- vtx_nic_run_wpa_cli_command ----------------------------------
static gchar *vtx_nic_run_wpa_cli_command(const gchar *interface, const gchar *command)
{
  if (!interface || !command) return NULL;

  gchar *cmd = g_strdup_printf("wpa_cli -i %s %s 2>/dev/null", interface, command);
  if (!cmd) return NULL;

  FILE *fp = popen(cmd, "r");
  g_free(cmd);
  if (!fp) return NULL;

  GString *buffer = g_string_new(NULL);
  if (!buffer)
  {
    pclose(fp);
    return NULL;
  }

  char line[256];
  while (fgets(line, sizeof(line), fp))
  {
    g_string_append(buffer, line);
  }

  pclose(fp);

  if (buffer->len == 0)
  {
    g_string_free(buffer, TRUE);
    return NULL;
  }

  return g_string_free(buffer, FALSE);
}

// Get WiFi connection information using wpa_cli
// --- vtx_nic_get_wpa_info ----------------------------------
static WifiInfo *vtx_nic_get_wpa_info(const gchar *interface)
{
  gchar *status_output = vtx_nic_run_wpa_cli_command(interface, "status");
  JsonObject *status = status_output ? vtx_nic_parse_wpa_for_cli(status_output) : NULL;
  g_free(status_output);

  gchar *signal_output = vtx_nic_run_wpa_cli_command(interface, "signal_poll");
  JsonObject *signal = signal_output ? vtx_nic_parse_wpa_for_cli(signal_output) : NULL;
  g_free(signal_output);

  if (!status && !signal) return NULL;

  WifiInfo *info = g_new0(WifiInfo, 1);
  info->status = status;
  info->signal = signal;
  return info;
}

// --- Ethernet helper structures and functions ----------------------------------
// --- vtx_nic_get_ethtool_info ----------------------------------
static JsonArray *vtx_nic_get_ethtool_info(const gchar *interface)
{
  gchar *cmd = g_strdup_printf("ethtool %s 2>/dev/null", interface);
  FILE *fp = popen(cmd, "r");
  g_free(cmd);

  if (!fp) return NULL;

  JsonArray *entries = vtx_nic_parse_ethtool_info(fp);
  pclose(fp);
  return entries;
}

// --- iw dev parsing helpers ----------------------------------
typedef struct
{
  GHashTable *named_interfaces;
  GPtrArray *unnamed_interfaces;
} IwDevInfo;

// --- vtx_nic_free_wifi_info ----------------------------------
static void vtx_nic_free_wifi_info(IwDevInfo *info)
{
  if (!info) return;

  if (info->named_interfaces) g_hash_table_destroy(info->named_interfaces);
  if (info->unnamed_interfaces) g_ptr_array_free(info->unnamed_interfaces, TRUE);

  g_free(info);
}

// --- vtx_nic_get_wifi_info ----------------------------------
static IwDevInfo *vtx_nic_get_wifi_info(void)
{
  FILE *fp = popen("iw dev 2>/dev/null", "r");
  if (!fp) return NULL;

  IwDevInfo *info = g_new0(IwDevInfo, 1);
  info->named_interfaces = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) json_object_unref);
  info->unnamed_interfaces = g_ptr_array_new_with_free_func((GDestroyNotify) json_object_unref);

  gchar *current_phy = NULL;
  JsonObject *current_iface = NULL;

  char line[512];
  while (fgets(line, sizeof(line), fp))
  {
    g_strchomp(line);
    gchar *trimmed = g_strchug(line);
    if (*trimmed == '\0') continue;

    if (g_str_has_prefix(trimmed, "phy#"))
    {
      g_free(current_phy);
      current_phy = g_strdup(trimmed);
      current_iface = NULL;
      continue;
    }

    if (g_str_has_prefix(trimmed, "Interface "))
    {
      const gchar *name = trimmed + strlen("Interface ");
      if (*name == '\0') continue;

      current_iface = json_object_new();
      if (current_phy) json_object_set_string_member(current_iface, "phy", current_phy);
      json_object_set_string_member(current_iface, "name", name);

      g_hash_table_insert(info->named_interfaces, g_strdup(name), json_object_ref(current_iface));
      continue;
    }

    if (g_str_has_prefix(trimmed, "Unnamed/non-netdev interface"))
    {
      current_iface = json_object_new();
      if (current_phy) json_object_set_string_member(current_iface, "phy", current_phy);
      json_object_set_boolean_member(current_iface, "unnamed", TRUE);
      json_object_set_string_member(current_iface, "description", trimmed);

      g_ptr_array_add(info->unnamed_interfaces, json_object_ref(current_iface));
      continue;
    }

    if (!current_iface) continue;

    if (g_str_has_prefix(trimmed, "multicast TXQ:"))
    {
      vtx_nic_parse_wifi_info_txq_block(fp, current_iface);
    }
    else
    {
      vtx_nic_parse_wifi_info(trimmed, vtx_nic_parse_iw_iface_add_attribute, current_iface);
    }
  }

  g_free(current_phy);
  pclose(fp);

  if (g_hash_table_size(info->named_interfaces) == 0 && info->unnamed_interfaces->len == 0)
  {
    vtx_nic_free_wifi_info(info);
    return NULL;
  }

  return info;
}

// --- vtx_nic_get_ipv4_address ----------------------------------
static gboolean vtx_nic_get_ipv4_address(struct ifaddrs *ifaddr, const gchar *ifname, char *host, gsize host_len)
{
  if (!ifaddr || !ifname || !host) return FALSE;

  for (struct ifaddrs *cursor = ifaddr; cursor != NULL; cursor = cursor->ifa_next)
  {
    if (!cursor->ifa_addr) continue;
    if (cursor->ifa_addr->sa_family != AF_INET) continue;
    if (g_strcmp0(cursor->ifa_name, ifname) != 0) continue;

    if (getnameinfo(cursor->ifa_addr, sizeof(struct sockaddr_in), host, host_len, NULL, 0, NI_NUMERICHOST) == 0)
    {
      return TRUE;
    }
  }

  return FALSE;
}

// --- vtx_nic_append_remaining_wifi_info ----------------------------------
static void vtx_nic_append_remaining_wifi_info(JsonArray *interfaces, GHashTable *seen, IwDevInfo *info)
{
  if (!interfaces || !info) return;

  if (info->named_interfaces)
  {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, info->named_interfaces);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *ifname = key;
      if (seen && g_hash_table_contains(seen, ifname)) continue;

      JsonObject *iface = vtx_nic_parse_interface_new(ifname);
      vtx_nic_parse_interface_attach_iw(iface, (JsonObject *) value, TRUE);
      vtx_nic_parse_interface_add(interfaces, iface);

      if (seen) g_hash_table_insert(seen, g_strdup(ifname), GINT_TO_POINTER(1));
    }
  }

  if (info->unnamed_interfaces)
  {
    for (guint i = 0; i < info->unnamed_interfaces->len; i++)
    {
      JsonObject *iw_iface = g_ptr_array_index(info->unnamed_interfaces, i);
      JsonObject *iface = vtx_nic_parse_interface_new("unnamed");
      vtx_nic_parse_interface_attach_iw(iface, iw_iface, TRUE);
      vtx_nic_parse_interface_add(interfaces, iface);
    }
  }
}

// --- vtx_nic_process_interface ----------------------------------
static JsonObject *vtx_nic_process_interface(struct ifaddrs *ifaddr, struct ifaddrs *ifa, IwDevInfo *iw_info)
{
  JsonObject *iface = vtx_nic_parse_interface_new(ifa->ifa_name);

  if (iw_info && iw_info->named_interfaces)
  {
    JsonObject *iw_iface = g_hash_table_lookup(iw_info->named_interfaces, ifa->ifa_name);
    if (iw_iface)
    {
      vtx_nic_parse_interface_attach_iw(iface, iw_iface, FALSE);
      g_hash_table_remove(iw_info->named_interfaces, ifa->ifa_name);
    }
  }

  // Get IPv4 address if available
  char host[NI_MAXHOST];
  if (vtx_nic_get_ipv4_address(ifaddr, ifa->ifa_name, host, sizeof(host)))
  {
    vtx_nic_parse_interface_set_address(iface, host);
  }

  // Check interface status
  gboolean is_up = (ifa->ifa_flags & IFF_UP) != 0;
  gboolean is_running = (ifa->ifa_flags & IFF_RUNNING) != 0;
  vtx_nic_parse_interface_set_flags(iface, is_up, is_running);

  // Determine interface type and get additional information
  gboolean is_wifi = vtx_nic_is_wifi_interface(ifa->ifa_name);
  gboolean is_ethernet = vtx_nic_is_ethernet_interface(ifa->ifa_name);

  if (is_wifi)
  {
    // Get WiFi connection information
    WifiInfo *wifi_info = vtx_nic_get_wpa_info(ifa->ifa_name);
    if (wifi_info)
    {
      vtx_nic_parse_interface_attach_wifi(iface, wifi_info->status, wifi_info->signal);
      if (wifi_info->status) json_object_unref(wifi_info->status);
      if (wifi_info->signal) json_object_unref(wifi_info->signal);
      g_free(wifi_info);
    }
  }
  else if (is_ethernet)
  {
    // Get Ethernet link status
    JsonArray *eth_info = vtx_nic_get_ethtool_info(ifa->ifa_name);
    if (eth_info)
    {
      vtx_nic_parse_interface_attach_ethtool(iface, eth_info);
      json_array_unref(eth_info);
    }
  }

  return iface;
}

// --- vtx_nic_inspection ----------------------------------
JsonArray *vtx_nic_inspection(void)
{
  struct ifaddrs *ifaddr, *ifa;
  JsonArray *interfaces = json_array_new();
  IwDevInfo *iw_info = vtx_nic_get_wifi_info();

  if (getifaddrs(&ifaddr) == -1)
  {
    gst_printerrln("Failed to get network interfaces");
    vtx_nic_append_remaining_wifi_info(interfaces, NULL, iw_info);
    vtx_nic_free_wifi_info(iw_info);
    return interfaces;
  }

  // Track which interfaces we've already added to avoid duplicates
  GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  // First pass: Add WiFi interfaces
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (!ifa->ifa_name) continue;
    if (g_hash_table_contains(seen, ifa->ifa_name)) continue;
    if (vtx_nic_should_skip_interface(ifa->ifa_name)) continue;
    if (!vtx_nic_is_wifi_interface(ifa->ifa_name)) continue;

    JsonObject *iface = vtx_nic_process_interface(ifaddr, ifa, iw_info);
    vtx_nic_parse_interface_add(interfaces, iface);
    g_hash_table_insert(seen, g_strdup(ifa->ifa_name), GINT_TO_POINTER(1));
  }

  // Second pass: Add Ethernet and other interfaces
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (!ifa->ifa_name) continue;
    if (g_hash_table_contains(seen, ifa->ifa_name)) continue;
    if (vtx_nic_should_skip_interface(ifa->ifa_name)) continue;

    JsonObject *iface = vtx_nic_process_interface(ifaddr, ifa, iw_info);
    vtx_nic_parse_interface_add(interfaces, iface);
    g_hash_table_insert(seen, g_strdup(ifa->ifa_name), GINT_TO_POINTER(1));
  }

  vtx_nic_append_remaining_wifi_info(interfaces, seen, iw_info);

  g_hash_table_destroy(seen);
  freeifaddrs(ifaddr);
  vtx_nic_free_wifi_info(iw_info);

  return interfaces;
}
