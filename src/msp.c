#include "headers/msp.h"

// --- 共通処理関数 ----------------------------------

// MSPコマンドを送信してレスポンスを受信
static int msp_request(MSP *msp, uint16_t cmd, uint8_t *response, size_t response_size, int min_expected_size)
{
  if (!msp_send_command(msp, cmd, NULL, 0))
  {
    return 0;
  }

  int size = msp_receive_response(msp, response, response_size, 1000);
  return (size >= min_expected_size) ? size : 0;
}

// --- データパース関数 ----------------------------------

// ボード情報をパース
// JavaScript: getText(data) - reads length-prefixed string
static int read_text(const uint8_t *buf, int offset, int max_offset, char *dest, int dest_size)
{
  if (offset >= max_offset)
  {
    dest[0] = '\0';
    return offset;
  }

  uint8_t length = buf[offset++];
  if (length == 0 || offset + length > max_offset)
  {
    dest[0] = '\0';
    return offset;
  }

  int copy_len = (length < dest_size - 1) ? length : dest_size - 1;
  memcpy(dest, buf + offset, copy_len);
  dest[copy_len] = '\0';

  return offset + length;
}

static void parse_board_info(const uint8_t *buf, int size, MspBoardInfoData *data)
{
  int offset = 0;

  // boardIdentifier (4 bytes)
  memcpy(data->board_identifier, buf + offset, 4);
  offset += 4;

  // boardVersion (2 bytes)
  data->hardware_revision = READ_UINT16(buf, offset);
  offset += 2;

  // boardType (1 byte)
  data->board_type = buf[offset++];

  // targetCapabilities (1 byte)
  data->target_capabilities = buf[offset++];

  // targetName (length-prefixed string)
  offset = read_text(buf, offset, size, data->target_name, sizeof(data->target_name));

  // boardName (length-prefixed string)
  offset = read_text(buf, offset, size, data->board_name, sizeof(data->board_name));

  // manufacturerId (length-prefixed string)
  offset = read_text(buf, offset, size, data->manufacturer_id, sizeof(data->manufacturer_id));

  // signature (32 bytes)
  int sig_len = (offset + 32 <= size) ? 32 : (size - offset);
  if (sig_len > 0)
  {
    memcpy(data->signature, buf + offset, sig_len);
    offset += sig_len;
  }
  // mcuTypeId (1 byte)
  if (offset < size)
  {
    data->mcu_type_id = buf[offset++];
  }
  // configurationState (1 byte) - API 1.42+
  if (offset < size)
  {
    data->configuration_state = buf[offset++];
  }
  // sampleRateHz (2 bytes) - API 1.43+
  if (offset + 1 < size)
  {
    data->sample_rate_hz = READ_UINT16(buf, offset);
    offset += 2;
  }
  // configurationProblems (4 bytes) - API 1.43+
  if (offset + 3 < size)
  {
    data->configuration_problems = READ_UINT32(buf, offset);
  }
}

// ステータスデータをパース
static void parse_status(const uint8_t *buf, MspStatusData *data)
{
  data->cycle_time = READ_UINT16(buf, 0);
  data->i2c_errors = READ_UINT16(buf, 2);
  data->sensor = READ_UINT16(buf, 4);
  data->flag = READ_INT32(buf, 6);
  data->current_pid_profile = buf[10];
}

// 拡張ステータスデータをパース (MSP_STATUS_EX)
static void parse_status_ex(const uint8_t *buf, int size, MspStatusExData *data)
{
  int offset = 0;

  // cycleTime (2 bytes)
  data->cycle_time = READ_UINT16(buf, offset);
  offset += 2;

  // i2cError (2 bytes)
  data->i2c_errors = READ_UINT16(buf, offset);
  offset += 2;

  // activeSensors (2 bytes)
  data->sensor = READ_UINT16(buf, offset);
  offset += 2;

  // mode (4 bytes)
  data->flag = READ_UINT32(buf, offset);
  offset += 4;

  // profile (1 byte)
  data->current_pid_profile = buf[offset++];

  // cpuload (2 bytes)
  if (offset + 1 < size)
  {
    data->cpu_load = READ_UINT16(buf, offset);
    offset += 2;
  }
  // numProfiles (1 byte)
  if (offset < size)
  {
    data->num_profiles = buf[offset++];
  }
  // rateProfile (1 byte)
  if (offset < size)
  {
    data->rate_profile = buf[offset++];
  }
  // Flight mode flags byte count (skip these)
  if (offset < size)
  {
    uint8_t byte_count = buf[offset++];
    offset += byte_count;  // Skip flight mode flags
  }
  // armingDisableCount (1 byte)
  if (offset < size)
  {
    data->arming_disable_count = buf[offset++];
  }
  // armingDisableFlags (4 bytes)
  if (offset + 3 < size)
  {
    data->arming_disable_flags = READ_UINT32(buf, offset);
    offset += 4;
  }
  // configStateFlag (1 byte)
  if (offset < size)
  {
    data->config_state_flag = buf[offset++];
  }
  // cpuTemp (2 bytes) - API 1.46+
  if (offset + 1 < size)
  {
    data->cpu_temp = READ_UINT16(buf, offset);
    offset += 2;
  }
  // numberOfRateProfiles (1 byte) - API 1.47+
  if (offset < size)
  {
    data->number_of_rate_profiles = buf[offset++];
  }
}

// --- 公開API関数 ----------------------------------

// ボード情報取得
int vtx_msp_get_board_info(MSP *msp, MspBoardInfoData *data)
{
  uint8_t response[256];
  // Minimum size: 4 (board_id) + 2 (hw_rev) + 1 (board_type) + 1 (target_cap) = 8 bytes
  int size = msp_request(msp, MSP_BOARD_INFO, response, sizeof(response), 8);

  if (size)
  {
    memset(data, 0, sizeof(MspBoardInfoData));
    parse_board_info(response, size, data);
    return 1;
  }
  return 0;
}

// ステータス情報取得
int vtx_msp_get_status(MSP *msp, MspStatusData *data)
{
  uint8_t response[64];
  int size = msp_request(msp, MSP_STATUS, response, sizeof(response), 11);

  if (size)
  {
    parse_status(response, data);
    return 1;
  }
  return 0;
}

// 拡張ステータス情報取得 (MSP_STATUS_EX)
int vtx_msp_get_status_ex(MSP *msp, MspStatusExData *data)
{
  uint8_t response[128];
  // Minimum size: 2+2+2+4+1 = 11 bytes (same as MSP_STATUS)
  int size = msp_request(msp, MSP_STATUS_EX, response, sizeof(response), 11);

  if (size)
  {
    memset(data, 0, sizeof(MspStatusExData));
    parse_status_ex(response, size, data);
    return 1;
  }
  return 0;
}
