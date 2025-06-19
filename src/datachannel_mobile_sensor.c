#include "headers/data_channel.h"

// Global mobile sensor data channel references
GObject *dc_imu = NULL;
GObject *dc_gnss = NULL;
GObject *dc_bat = NULL;

// Timeout source IDs
guint timeout_id_imu = 0;
guint timeout_id_gnss = 0;
guint timeout_id_bat = 0;

// --- vtx_send_dummy_quaternion ----------------------------------
gboolean vtx_send_dummy_quaternion(gpointer user_data)
{
  if (!dc_imu) return G_SOURCE_REMOVE;

  float epsilon = 0.1f;
  float q[4] = {
      epsilon * ((float) rand() / RAND_MAX - 0.5f),
      epsilon * ((float) rand() / RAND_MAX - 0.5f),
      epsilon * ((float) rand() / RAND_MAX - 0.5f),
      1.0f + epsilon * ((float) rand() / RAND_MAX - 0.5f),
  };

  GBytes *bytes = g_bytes_new(q, sizeof(q));
  g_signal_emit_by_name(dc_imu, "send-data", bytes, NULL);
  g_bytes_unref(bytes);

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_dummy_gnss ----------------------------------
gboolean vtx_send_dummy_gnss(gpointer user_data)
{
  if (!dc_gnss || !G_IS_OBJECT(dc_gnss)) return G_SOURCE_REMOVE;

  double latitude = 35.0 + ((double) rand() / RAND_MAX - 0.5) * 0.01;
  double longitude = 139.0 + ((double) rand() / RAND_MAX - 0.5) * 0.01;
  double altitude = 10.0 + ((double) rand() / RAND_MAX) * 100.0;
  double speed = ((double) rand() / RAND_MAX) * 5.0;
  double heading = ((double) rand() / RAND_MAX) * 360.0;

  gchar *message = g_strdup_printf(
      "{\"latitude\": %.6f, \"longitude\": %.6f, \"altitude\": "
      "%.2f, \"speed\": %.2f, \"heading\": %.2f}",
      latitude, longitude, altitude, speed, heading);

  g_signal_emit_by_name(dc_gnss, "send-string", message);
  g_free(message);

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_dummy_battery ----------------------------------
gboolean vtx_send_dummy_battery(gpointer user_data)
{
  if (!dc_bat || !G_IS_OBJECT(dc_bat)) return G_SOURCE_REMOVE;

  // Simulate battery data similar to JavaScript Battery API
  // charging: random boolean (simulate plugged/unplugged)
  // level: 0.0 to 1.0 (percentage as decimal)
  // chargingTime: seconds until fully charged (Infinity if not charging)
  // dischargingTime: seconds until empty (Infinity if charging)

  static double battery_level = 0.75;  // Start at 75%
  static gboolean charging = FALSE;

  // Randomly change charging state (10% chance per update)
  if ((rand() % 10) == 0)
  {
    charging = !charging;
  }
  // Simulate battery level changes
  if (charging)
  {
    battery_level += 0.01;  // Charge slowly
    if (battery_level > 1.0) battery_level = 1.0;
  }
  else
  {
    battery_level -= 0.005;  // Discharge slowly
    if (battery_level < 0.0) battery_level = 0.0;
  }

  // Calculate times (simplified simulation)
  double chargingTime = charging ? (1.0 - battery_level) * 3600.0 : -1.0;  // -1 = Infinity
  double dischargingTime = !charging ? battery_level * 7200.0 : -1.0;      // -1 = Infinity

  gchar *message = g_strdup_printf(
      "{\"charging\": %s, \"level\": %.3f, \"chargingTime\": %.0f, "
      "\"dischargingTime\": %.0f}",
      charging ? "true" : "false", battery_level, chargingTime, dischargingTime);

  g_signal_emit_by_name(dc_bat, "send-string", message);
  g_free(message);

  return G_SOURCE_CONTINUE;
}
