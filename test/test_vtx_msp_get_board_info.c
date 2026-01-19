#include "msp.h"
#include "unity.h"
#include "utils.h"

void
test_vtx_msp_get_board_info (void)
{
  const char *port = vtx_msp_detect ();
  TEST_ASSERT_NOT_NULL_MESSAGE (port, "MSP port not detected");

  gst_println ("Detected MSP port: %s", port);

  MSP msp;
  int init_result = vtx_msp_init (&msp, port, B115200);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, init_result, "Failed to initialize MSP");

  MspBoardInfoData board_info;
  int result = vtx_msp_get_board_info (&msp, &board_info);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, result, "Failed to get board info");

  gst_println ("----- MSP Board Info -----");
  gchar board_id[5];
  memcpy (board_id, board_info.board_identifier, 4);
  board_id[4] = '\0';
  gst_println ("Board Identifier: %s", board_id);
  gst_println ("Hardware Revision: %u", board_info.hardware_revision);
  gst_println ("Board Type: %u", board_info.board_type);
  gst_println ("Target Capabilities: %u", board_info.target_capabilities);
  gst_println ("Target Name: %s", board_info.target_name);
  gst_println ("Board Name: %s", board_info.board_name);
  gst_println ("Manufacturer ID: %s", board_info.manufacturer_id);
  gst_println ("MCU Type ID: %u", board_info.mcu_type_id);
  gst_println ("Configuration State: %u", board_info.configuration_state);
  gst_println ("Sample Rate Hz: %u", board_info.sample_rate_hz);
  gst_println ("Configuration Problems: %u", board_info.configuration_problems);

  vtx_msp_close (&msp);
}
