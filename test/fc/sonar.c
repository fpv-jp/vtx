// MSP_SONAR_ALTITUDE (58): Sonar altitude
// altitude(i32 cm) = 4 bytes

#include "common.h"

#define MSP_SONAR_ALTITUDE 58

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_SONAR_ALTITUDE  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_SONAR_ALTITUDE);

  uint8_t buf[8];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 4) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  int32_t alt = LE_I32(buf, 0);
  printf("  sonar altitude = %dcm  (%.2fm)\n", alt, alt / 100.0);

  if (alt <= 0)
    printf("  [INFO] No sonar reading (sensor may not be connected)\n");

  return 0;
}
