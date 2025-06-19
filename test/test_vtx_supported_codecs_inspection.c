#include "unity.h"
#include "utils.h"
#include "inspection.h"

void
test_vtx_supported_codecs_inspection (void)
{
  JsonObject *result = vtx_supported_codecs_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Codecs -----");
  print_json_object (result);
}
