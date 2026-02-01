// MSP_ALTITUDE (109): Estimated altitude / variometer
// estimatedAltitude(i32 cm) [+ variometer(i16 cm/s)]

#include "common.h"

#define MSP_ALTITUDE 109

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_ALTITUDE  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_ALTITUDE);

  uint8_t buf[16];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 4) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  int32_t alt = LE_I32(buf, 0);
  printf("  altitude = %dcm  (%.2fm)\n", alt, alt / 100.0);

  if (n >= 6) {
    int16_t vario = LE_I16(buf, 4);
    printf("  variometer = %dcm/s  (%.2fm/s)\n", vario, vario / 100.0);
  }

  return 0;
}
