#pragma once

#include <gst/gst.h>

#include "msp.h"
#include "utils.h"

typedef struct
{
  const char *channel_type;
  GObject **dc_ref;
  gboolean ordered;
  gboolean use_lifetime;
  guint value;  // max-packet-lifetime または max-retransmits
} ChannelConfig;

typedef enum
{
  CMD_HANG_UP = 0,
  CMD_PING = 1,
  CMD_PONG = 2,
  // CMD_SEND_KEYFRAME_REQUEST = 3,
  // CMD_SPS_PPS = 4,
  CMD_ERROR = 9
} CommandType;

void vtx_webrtc_on_data_channel(GstElement *webrtc, GObject *data_channel, gpointer user_data);

void vtx_dc_create_offer(GstElement *webrtc);

// ----------------------------------

#define CHANNEL_TYPE_CMD "CMD"

extern GObject *dc_cmd;

void vtx_dc_on_message_command(GObject *dc, gchar *str, gpointer user_data);

// ----------------------------------

#define CHANNEL_TYPE_IMU "IMU"
#define CHANNEL_TYPE_GNSS "GNSS"
#define CHANNEL_TYPE_BAT "BAT"

extern GObject *dc_imu;
extern GObject *dc_gnss;
extern GObject *dc_bat;

extern guint timeout_id_imu;
extern guint timeout_id_gnss;
extern guint timeout_id_bat;

gboolean vtx_send_dummy_quaternion(gpointer user_data);

gboolean vtx_send_dummy_gnss(gpointer user_data);

gboolean vtx_send_dummy_battery(gpointer user_data);

// ----------------------------------

#define CHANNEL_TYPE_MSP_RAW_IMU "MSP_RAW_IMU"
#define CHANNEL_TYPE_MSP_RAW_GPS "MSP_RAW_GPS"
#define CHANNEL_TYPE_MSP_COMP_GPS "MSP_COMP_GPS"
#define CHANNEL_TYPE_MSP_ATTITUDE "MSP_ATTITUDE"
#define CHANNEL_TYPE_MSP_ALTITUDE "MSP_ALTITUDE"
#define CHANNEL_TYPE_MSP_ANALOG "MSP_ANALOG"
#define CHANNEL_TYPE_MSP_SONAR "MSP_SONAR"
#define CHANNEL_TYPE_MSP_BATTERY_STATE "MSP_BATTERY_STATE"

#define CHANNEL_TYPE_WPA_SUPPLICANT "WPA_SUPPLICANT"

extern MSP *g_msp;

extern GObject *dc_msp_raw_imu;
extern GObject *dc_msp_raw_gps;
extern GObject *dc_msp_comp_gps;
extern GObject *dc_msp_attitude;
extern GObject *dc_msp_altitude;
extern GObject *dc_msp_analog;
extern GObject *dc_msp_sonar;
extern GObject *dc_msp_battery_state;

extern GObject *dc_wpa_supplicant;

// Timeout source IDs for data channel periodic sends
extern guint timeout_id_msp_raw_imu;
extern guint timeout_id_msp_raw_gps;
extern guint timeout_id_msp_comp_gps;
extern guint timeout_id_msp_attitude;
extern guint timeout_id_msp_altitude;
extern guint timeout_id_msp_analog;
extern guint timeout_id_msp_sonar;
extern guint timeout_id_msp_battery_state;

extern guint timeout_id_wpa_supplicant;

JsonArray *vtx_msp_flight_controller(void);

gboolean vtx_send_msp_raw_imu(gpointer user_data);

gboolean vtx_send_msp_raw_gps(gpointer user_data);

gboolean vtx_send_msp_comp_gps(gpointer user_data);

gboolean vtx_send_msp_attitude(gpointer user_data);

gboolean vtx_send_msp_altitude(gpointer user_data);

gboolean vtx_send_msp_analog(gpointer user_data);

gboolean vtx_send_msp_sonar(gpointer user_data);

gboolean vtx_send_msp_battery_state(gpointer user_data);

void vtx_msp_set_global(MSP *msp);

void vtx_msp_cleanup_global(void);

void vtx_dc_cleanup(void);
