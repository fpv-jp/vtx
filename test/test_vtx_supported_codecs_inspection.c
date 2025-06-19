#include "inspection.h"
#include "unity.h"
#include "utils.h"

void
test_vtx_codecs_supported_inspection (void)
{
  JsonObject *result = vtx_codecs_supported_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Codecs -----");
  print_json_object (result);
}
