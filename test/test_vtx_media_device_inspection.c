#include "unity.h"
#include "utils.h"
#include "inspection.h"

void
test_vtx_media_device_inspection (void)
{
  JsonArray *result = vtx_load_device_launch_entries ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Device -----");
  print_json_array (result);
}
