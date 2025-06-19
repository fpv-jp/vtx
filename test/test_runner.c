#include "../src/headers/common.h"
#include "../src/headers/signaling.h"
#include "../src/headers/utils.h"
#include "unity.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

extern void test_vtx_media_device_inspection (void);
extern void test_vtx_codecs_supported_inspection (void);
extern void test_vtx_msp_get_board_info (void);
extern void test_vtx_msp_get_status (void);
extern void test_vtx_msp_get_status_ex (void);

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
  RUN_TEST (test_vtx_media_device_inspection);
  RUN_TEST (test_vtx_codecs_supported_inspection);
  RUN_TEST (test_vtx_msp_get_board_info);
  RUN_TEST (test_vtx_msp_get_status);
  RUN_TEST (test_vtx_msp_get_status_ex);
  return UNITY_END ();
}
