#include "unity.h"
#include "../src/headers/common.h"
#include "../src/headers/signaling.h"
#include "../src/headers/utils.h"

GMainLoop *loop = NULL;

AppState app_state = APP_STATE_UNKNOWN;

extern void test_vtx_media_device_inspection (void);
extern void test_vtx_supported_codecs_inspection (void);

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
  RUN_TEST (test_vtx_supported_codecs_inspection);
  return UNITY_END ();
}
