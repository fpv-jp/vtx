#include "data_channel.h"
#include "inspection.h"
#include "unity.h"
#include "utils.h"

void
test_vtx_nic_inspection (void)
{
  JsonArray *result = vtx_nic_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Network Interfaces -----");
  print_json_array (result);
}

void
test_vtx_device_load_launch_entries (void)
{
  JsonArray *result = vtx_device_load_launch_entries ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Device Capabilities -----");
  print_json_array (result);
}

void
test_vtx_supported_codec_inspection (void)
{
  JsonObject *result = vtx_supported_codec_inspection ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Support Codecs -----");
  print_json_object (result);
}

void
test_vtx_msp_flight_controller (void)
{
  JsonArray *result = vtx_msp_flight_controller ();
  TEST_ASSERT_NOT_NULL (result);
  gst_println ("----- Result Flight Controllers -----");
  print_json_array (result);
}
