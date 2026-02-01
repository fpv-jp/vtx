// MSP_ATTITUDE (108): Roll / Pitch / Heading (compass)
// roll(i16/10=deg) + pitch(i16/10=deg) + heading(i16=deg)  = 6 bytes

#include "common.h"

#define MSP_ATTITUDE 108

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_ATTITUDE  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_ATTITUDE);

  uint8_t buf[16];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 6) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  int16_t roll    = LE_I16(buf, 0);
  int16_t pitch   = LE_I16(buf, 2);
  int16_t heading = LE_I16(buf, 4);

  printf("  roll   = %+.1f deg\n", roll    / 10.0);
  printf("  pitch  = %+.1f deg\n", pitch   / 10.0);
  printf("  heading= %d deg  (compass)\n", heading);

  return 0;
}
