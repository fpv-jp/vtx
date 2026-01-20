#include "../src/headers/common.h"
#include "../src/headers/signaling.h"
#include "../src/headers/utils.h"
#include "unity.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

extern void test_vtx_nic_inspection (void);
extern void test_vtx_device_load_launch_entries (void);
extern void test_vtx_supported_codec_inspection (void);
extern void test_vtx_msp_flight_controller (void);

void
setUp (void)
{
}

void
tearDown (void)
{
}

int
main (void)
{
  UNITY_BEGIN ();
  RUN_TEST (test_vtx_nic_inspection);
  RUN_TEST (test_vtx_device_load_launch_entries);
  RUN_TEST (test_vtx_supported_codec_inspection);
  RUN_TEST (test_vtx_msp_flight_controller);
  return UNITY_END ();
}
