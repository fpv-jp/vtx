#pragma once

// Multiwii Serial Protocol (MSP)

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "msp_protocol.h"
#include "msp_protocol_v2_betaflight.h"
#include "msp_protocol_v2_common.h"

typedef struct
{
  int serial_fd;
} MSP;

// --- データ構造体 ----------------------------------

typedef struct
{
  uint8_t board_identifier[4];
  uint16_t hardware_revision;
  uint8_t board_type;
  uint8_t target_capabilities;
  char target_name[32];
  char board_name[32];
  char manufacturer_id[32];
  uint8_t signature[32];
  uint8_t mcu_type_id;
  uint8_t configuration_state;
  uint16_t sample_rate_hz;
  uint32_t configuration_problems;
} MspBoardInfoData;

typedef struct
{
  uint16_t cycle_time;
  uint16_t i2c_errors;
  uint16_t sensor;
  uint32_t flag;
  uint8_t current_pid_profile;
} MspStatusData;

typedef struct
{
  uint16_t cycle_time;
  uint16_t i2c_errors;
  uint16_t sensor;
  uint32_t flag;
  uint8_t current_pid_profile;
  uint16_t cpu_load;
  uint8_t num_profiles;
  uint8_t rate_profile;
  uint8_t arming_disable_count;
  uint32_t arming_disable_flags;
  uint8_t config_state_flag;
  uint16_t cpu_temp;
  uint8_t number_of_rate_profiles;
} MspStatusExData;

// --- ヘルパーマクロ ----------------------------------

// リトルエンディアンで16ビット値を読み取る
#define READ_INT16(buf, offset) ((int16_t) ((buf)[offset] | ((buf)[(offset) + 1] << 8)))

// リトルエンディアンで16ビット符号なし値を読み取る
#define READ_UINT16(buf, offset) ((uint16_t) ((buf)[offset] | ((buf)[(offset) + 1] << 8)))

// リトルエンディアンで32ビット値を読み取る
#define READ_INT32(buf, offset) ((int32_t) ((buf)[offset] | ((buf)[(offset) + 1] << 8) | ((buf)[(offset) + 2] << 16) | ((buf)[(offset) + 3] << 24)))

// リトルエンディアンで32ビット符号なし値を読み取る
#define READ_UINT32(buf, offset) ((uint32_t) ((buf)[offset] | ((buf)[(offset) + 1] << 8) | ((buf)[(offset) + 2] << 16) | ((buf)[(offset) + 3] << 24)))

// ---------------------

const char *vtx_msp_detect();

int vtx_msp_detect_all(char ***ports_out);

// void vtx_msp_sleep_ms(int milliseconds);

int vtx_msp_init(MSP *msp, const char *port, speed_t baudrate);

void vtx_msp_close(MSP *msp);

int msp_send_command(MSP *msp, uint8_t cmd, const uint8_t *data, uint8_t data_size);

int msp_receive_response(MSP *msp, uint8_t *response, int max_size, int timeout_ms);

int msp_request_raw(MSP *msp, uint16_t cmd, uint8_t *response, size_t response_size);

// ---------------------

int vtx_msp_get_board_info(MSP *msp, MspBoardInfoData *data);

int vtx_msp_get_status(MSP *msp, MspStatusData *data);

int vtx_msp_get_status_ex(MSP *msp, MspStatusExData *data);
