// MSP_ANALOG (110): Vbat / mAh / RSSI / Amperage
// vbat(u8*0.1V) + mAhDrawn(u16) + rssi(u16/1023) + amperage(i16*0.01A) + voltage(u16 mV) = 9 bytes

#include "common.h"

#define MSP_ANALOG 110

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_ANALOG  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_ANALOG);

  uint8_t buf[16];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 9) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  uint8_t  vbat      = buf[0];
  uint16_t mah_drawn = LE_U16(buf, 1);
  uint16_t rssi      = LE_U16(buf, 3);
  int16_t  amperage  = LE_I16(buf, 5);
  uint16_t voltage   = LE_U16(buf, 7);

  printf("  voltage    = %.1fV  (%dmV)\n", vbat * 0.1, voltage);
  printf("  amperage   = %.2fA\n", amperage / 100.0);
  printf("  mAh drawn  = %dmAh\n", mah_drawn);
  printf("  RSSI       = %d / 1023  (%.0f%%)\n", rssi, rssi / 1023.0 * 100);

  return 0;
}
