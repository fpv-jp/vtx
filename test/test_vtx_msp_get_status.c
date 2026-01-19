#include "msp.h"
#include "unity.h"
#include "utils.h"

void
test_vtx_msp_get_status (void)
{
  const char *port = vtx_msp_detect ();
  TEST_ASSERT_NOT_NULL_MESSAGE (port, "MSP port not detected");

  gst_println ("Detected MSP port: %s", port);

  MSP msp;
  int init_result = vtx_msp_init (&msp, port, B115200);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, init_result, "Failed to initialize MSP");

  MspStatusData status;
  int result = vtx_msp_get_status (&msp, &status);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, result, "Failed to get status");

  gst_println ("----- MSP Status -----");
  gst_println ("Cycle Time: %u", status.cycle_time);
  gst_println ("I2C Errors: %u", status.i2c_errors);
  gst_println ("Sensor: %u", status.sensor);
  gst_println ("Flag: %u", status.flag);
  gst_println ("Current PID Profile: %u", status.current_pid_profile);

  vtx_msp_close (&msp);
}
