#include "msp.h"
#include "unity.h"
#include "utils.h"

void
test_vtx_msp_get_status_ex (void)
{
  const char *port = vtx_msp_detect ();
  TEST_ASSERT_NOT_NULL_MESSAGE (port, "MSP port not detected");

  gst_println ("Detected MSP port: %s", port);

  MSP msp;
  int init_result = vtx_msp_init (&msp, port, B115200);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, init_result, "Failed to initialize MSP");

  MspStatusExData status_ex;
  int result = vtx_msp_get_status_ex (&msp, &status_ex);
  TEST_ASSERT_EQUAL_INT_MESSAGE (1, result, "Failed to get status ex");

  gst_println ("----- MSP Status EX -----");
  gst_println ("Cycle Time: %u", status_ex.cycle_time);
  gst_println ("I2C Errors: %u", status_ex.i2c_errors);
  gst_println ("Sensor: %u", status_ex.sensor);
  gst_println ("Flag: %u", status_ex.flag);
  gst_println ("Current PID Profile: %u", status_ex.current_pid_profile);
  gst_println ("CPU Load: %u", status_ex.cpu_load);
  gst_println ("Num Profiles: %u", status_ex.num_profiles);
  gst_println ("Rate Profile: %u", status_ex.rate_profile);
  gst_println ("Arming Disable Count: %u", status_ex.arming_disable_count);
  gst_println ("Arming Disable Flags: %u", status_ex.arming_disable_flags);
  gst_println ("Config State Flag: %u", status_ex.config_state_flag);
  gst_println ("CPU Temp: %u", status_ex.cpu_temp);
  gst_println ("Number of Rate Profiles: %u",
      status_ex.number_of_rate_profiles);

  vtx_msp_close (&msp);
}
