#include <stdio.h>
#include <string.h>

#include "headers/data_channel.h"

// Global MSP instance and data channel references
MSP *g_msp = NULL;

GObject *dc_msp_raw_imu = NULL;
GObject *dc_msp_raw_gps = NULL;
GObject *dc_msp_comp_gps = NULL;
GObject *dc_msp_attitude = NULL;
GObject *dc_msp_altitude = NULL;
GObject *dc_msp_analog = NULL;
GObject *dc_msp_sonar = NULL;
GObject *dc_msp_battery_state = NULL;

// Timeout source IDs
guint timeout_id_msp_raw_imu = 0;
guint timeout_id_msp_raw_gps = 0;
guint timeout_id_msp_comp_gps = 0;
guint timeout_id_msp_attitude = 0;
guint timeout_id_msp_altitude = 0;
guint timeout_id_msp_analog = 0;
guint timeout_id_msp_sonar = 0;
guint timeout_id_msp_battery_state = 0;

// --- vtx_msp_set_global ----------------------------------
void vtx_msp_set_global(MSP *msp)
{
  g_msp = msp;
}

// --- vtx_msp_cleanup_global ----------------------------------
void vtx_msp_cleanup_global(void)
{
  if (g_msp)
  {
    vtx_msp_close(g_msp);
    g_free(g_msp);
    g_msp = NULL;
  }
}

// --- vtx_dc_cleanup ----------------------------------
void vtx_dc_cleanup(void)
{
  // Remove all timeout sources first
  if (timeout_id_msp_raw_imu > 0)
  {
    g_source_remove(timeout_id_msp_raw_imu);
    timeout_id_msp_raw_imu = 0;
  }
  if (timeout_id_msp_raw_gps > 0)
  {
    g_source_remove(timeout_id_msp_raw_gps);
    timeout_id_msp_raw_gps = 0;
  }
  if (timeout_id_msp_comp_gps > 0)
  {
    g_source_remove(timeout_id_msp_comp_gps);
    timeout_id_msp_comp_gps = 0;
  }
  if (timeout_id_msp_attitude > 0)
  {
    g_source_remove(timeout_id_msp_attitude);
    timeout_id_msp_attitude = 0;
  }
  if (timeout_id_msp_altitude > 0)
  {
    g_source_remove(timeout_id_msp_altitude);
    timeout_id_msp_altitude = 0;
  }
  if (timeout_id_msp_analog > 0)
  {
    g_source_remove(timeout_id_msp_analog);
    timeout_id_msp_analog = 0;
  }
  if (timeout_id_msp_sonar > 0)
  {
    g_source_remove(timeout_id_msp_sonar);
    timeout_id_msp_sonar = 0;
  }
  if (timeout_id_msp_battery_state > 0)
  {
    g_source_remove(timeout_id_msp_battery_state);
    timeout_id_msp_battery_state = 0;
  }

  // Close and unref data channels
  if (dc_cmd)
  {
    g_signal_emit_by_name(dc_cmd, "close");
    g_object_unref(dc_cmd);
    dc_cmd = NULL;
  }
  if (dc_msp_raw_imu)
  {
    g_signal_emit_by_name(dc_msp_raw_imu, "close");
    g_object_unref(dc_msp_raw_imu);
    dc_msp_raw_imu = NULL;
  }
  if (dc_msp_raw_gps)
  {
    g_signal_emit_by_name(dc_msp_raw_gps, "close");
    g_object_unref(dc_msp_raw_gps);
    dc_msp_raw_gps = NULL;
  }
  if (dc_msp_comp_gps)
  {
    g_signal_emit_by_name(dc_msp_comp_gps, "close");
    g_object_unref(dc_msp_comp_gps);
    dc_msp_comp_gps = NULL;
  }
  if (dc_msp_attitude)
  {
    g_signal_emit_by_name(dc_msp_attitude, "close");
    g_object_unref(dc_msp_attitude);
    dc_msp_attitude = NULL;
  }
  if (dc_msp_altitude)
  {
    g_signal_emit_by_name(dc_msp_altitude, "close");
    g_object_unref(dc_msp_altitude);
    dc_msp_altitude = NULL;
  }
  if (dc_msp_analog)
  {
    g_signal_emit_by_name(dc_msp_analog, "close");
    g_object_unref(dc_msp_analog);
    dc_msp_analog = NULL;
  }
  if (dc_msp_sonar)
  {
    g_signal_emit_by_name(dc_msp_sonar, "close");
    g_object_unref(dc_msp_sonar);
    dc_msp_sonar = NULL;
  }
  if (dc_msp_battery_state)
  {
    g_signal_emit_by_name(dc_msp_battery_state, "close");
    g_object_unref(dc_msp_battery_state);
    dc_msp_battery_state = NULL;
  }

  gst_println("Data channels cleaned up");
}

// --- vtx_msp_flight_controller ----------------------------------
JsonArray *vtx_msp_flight_controller(void)
{
  JsonArray *flight_controllers = json_array_new();

  char **ports = NULL;
  int port_count = vtx_msp_detect_all(&ports);

  if (port_count == 0)
  {
    gst_println("No flight controllers detected");
    return flight_controllers;
  }

  gst_println("Number of detected ports: %d", port_count);
  for (int i = 0; i < port_count; i++)
  {
    gst_println("  [%d] %s", i, ports[i]);
  }

  for (int i = 0; i < port_count; i++)
  {
    const char *port = ports[i];
    JsonObject *fc_info = json_object_new();
    JsonObject *msp_board_info = NULL;
    JsonObject *msp_status = NULL;

    json_object_set_string_member(fc_info, "port", port);

    MSP *msp = g_malloc(sizeof(MSP));
    if (vtx_msp_init(msp, port, B115200) == 1)
    {
      // Store the first successful MSP connection globally
      if (g_msp == NULL)
      {
        vtx_msp_set_global(msp);
        gst_println("MSP connection established and stored globally: %s", port);
      }

      // Get board info
      MspBoardInfoData board_info;

      if (vtx_msp_get_board_info(msp, &board_info))
      {
        msp_board_info = json_object_new();
        gchar board_id[5];
        memcpy(board_id, board_info.board_identifier, 4);
        board_id[4] = '\0';
        json_object_set_string_member(msp_board_info, "board_identifier", board_id);
        json_object_set_int_member(msp_board_info, "hardware_revision", board_info.hardware_revision);
        json_object_set_int_member(msp_board_info, "board_type", board_info.board_type);
        json_object_set_int_member(msp_board_info, "target_capabilities", board_info.target_capabilities);
        json_object_set_string_member(msp_board_info, "target_name", board_info.target_name);
        json_object_set_string_member(msp_board_info, "board_name", board_info.board_name);
        json_object_set_string_member(msp_board_info, "manufacturer_id", board_info.manufacturer_id);
        json_object_set_int_member(msp_board_info, "mcu_type_id", board_info.mcu_type_id);
        json_object_set_int_member(msp_board_info, "configuration_state", board_info.configuration_state);
        json_object_set_int_member(msp_board_info, "sample_rate_hz", board_info.sample_rate_hz);
        json_object_set_int_member(msp_board_info, "configuration_problems", board_info.configuration_problems);
      }

      // Get status - Try MSP_STATUS_EX first, fallback to MSP_STATUS
      MspStatusExData status_ex;

      if (vtx_msp_get_status_ex(msp, &status_ex))
      {
        msp_status = json_object_new();
        json_object_set_int_member(msp_status, "cycle_time", status_ex.cycle_time);
        json_object_set_int_member(msp_status, "i2c_errors", status_ex.i2c_errors);
        json_object_set_int_member(msp_status, "sensor", status_ex.sensor);
        json_object_set_int_member(msp_status, "flag", status_ex.flag);
        json_object_set_int_member(msp_status, "current_pid_profile", status_ex.current_pid_profile);
        json_object_set_int_member(msp_status, "cpu_load", status_ex.cpu_load);
        json_object_set_int_member(msp_status, "num_profiles", status_ex.num_profiles);
        json_object_set_int_member(msp_status, "rate_profile", status_ex.rate_profile);
        json_object_set_int_member(msp_status, "arming_disable_count", status_ex.arming_disable_count);
        json_object_set_int_member(msp_status, "arming_disable_flags", status_ex.arming_disable_flags);
        json_object_set_int_member(msp_status, "config_state_flag", status_ex.config_state_flag);
        json_object_set_int_member(msp_status, "cpu_temp", status_ex.cpu_temp);
        json_object_set_int_member(msp_status, "number_of_rate_profiles", status_ex.number_of_rate_profiles);
      }
      else
      {
        // Fallback to regular MSP_STATUS if MSP_STATUS_EX is not supported
        MspStatusData status;

        if (vtx_msp_get_status(msp, &status))
        {
          msp_status = json_object_new();
          json_object_set_int_member(msp_status, "cycle_time", status.cycle_time);
          json_object_set_int_member(msp_status, "i2c_errors", status.i2c_errors);
          json_object_set_int_member(msp_status, "sensor", status.sensor);
          json_object_set_int_member(msp_status, "flag", status.flag);
          json_object_set_int_member(msp_status, "current_pid_profile", status.current_pid_profile);
        }
      }

      // Close MSP connection if not stored globally
      if (msp != g_msp)
      {
        vtx_msp_close(msp);
        g_free(msp);
      }
    }
    else
    {
      g_free(msp);
    }

    if (msp_board_info)
    {
      json_object_set_object_member(fc_info, "msp_board_info", msp_board_info);
    }

    if (msp_status)
    {
      json_object_set_object_member(fc_info, "msp_status", msp_status);
    }

    json_array_add_object_element(flight_controllers, fc_info);

    // Free port string
    free(ports[i]);
  }

  // Free ports array
  free(ports);

  return flight_controllers;
}

// --- vtx_send_msp_raw_imu ----------------------------------
gboolean vtx_send_msp_raw_imu(gpointer user_data)
{
  if (!dc_msp_raw_imu) return G_SOURCE_REMOVE;

  // acc[3] + gyro[3] + mag[3] = 9 x int16 = 18 bytes
  uint8_t response[32];  // 18 bytes + margin
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_RAW_IMU, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 18);
    size = 18;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_raw_imu, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_raw_gps ----------------------------------
gboolean vtx_send_msp_raw_gps(gpointer user_data)
{
  if (!dc_msp_raw_gps) return G_SOURCE_REMOVE;

  // u8 + u8 + i32 + i32 + u16 + u16 + u16 = 16 bytes
  uint8_t response[16];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_RAW_GPS, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 16);
    size = 16;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_raw_gps, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_comp_gps ----------------------------------
gboolean vtx_send_msp_comp_gps(gpointer user_data)
{
  if (!dc_msp_comp_gps) return G_SOURCE_REMOVE;

  // u16 + u16 + u8 = 5 bytes
  uint8_t response[8];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_COMP_GPS, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 5);
    size = 5;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_comp_gps, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_attitude ----------------------------------
gboolean vtx_send_msp_attitude(gpointer user_data)
{
  if (!dc_msp_attitude) return G_SOURCE_REMOVE;

  // i16 + i16 + i16 = 6 bytes
  uint8_t response[8];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_ATTITUDE, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 6);
    size = 6;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_attitude, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_altitude ----------------------------------
gboolean vtx_send_msp_altitude(gpointer user_data)
{
  if (!dc_msp_altitude) return G_SOURCE_REMOVE;

  // i32 = 4 bytes
  uint8_t response[8];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_ALTITUDE, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 4);
    size = 4;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_altitude, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_analog ----------------------------------
gboolean vtx_send_msp_analog(gpointer user_data)
{
  if (!dc_msp_analog) return G_SOURCE_REMOVE;

  // u8 + u16 + u16 + i16 + u16 = 9 bytes
  uint8_t response[16];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_ANALOG, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 9);
    size = 9;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_analog, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_sonar ----------------------------------
gboolean vtx_send_msp_sonar(gpointer user_data)
{
  if (!dc_msp_sonar) return G_SOURCE_REMOVE;

  // i32 = 4 bytes
  uint8_t response[8];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_SONAR_ALTITUDE, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 4);
    size = 4;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_sonar, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}

// --- vtx_send_msp_battery_state ----------------------------------
gboolean vtx_send_msp_battery_state(gpointer user_data)
{
  if (!dc_msp_battery_state) return G_SOURCE_REMOVE;

  // u8 + u16 + u8 + u16 + u16 + u8 + u16 = 10 bytes
  uint8_t response[16];
  int size = 0;

  if (g_msp)
  {
    size = msp_request_raw(g_msp, MSP_BATTERY_STATE, response, sizeof(response));
  }
  else
  {
    memset(response, 0, 10);
    size = 10;
  }

  if (size > 0)
  {
    GBytes *bytes = g_bytes_new(response, size);
    g_signal_emit_by_name(dc_msp_battery_state, "send-data", bytes, NULL);
    g_bytes_unref(bytes);
  }

  return G_SOURCE_CONTINUE;
}
