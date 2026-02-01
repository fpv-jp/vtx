// MSP_RAW_GPS (106): GPS fix / satellites / lat / lon / alt / speed / course
// MSP_COMP_GPS (107): distance to home / direction to home / heartbeat

#include "common.h"

#define MSP_RAW_GPS  106
#define MSP_COMP_GPS 107

static void print_raw_gps(int fd)
{
  printf("\n--- MSP_RAW_GPS ---\n");
  fc_send(fd, MSP_RAW_GPS);

  uint8_t buf[32];
  int n = fc_recv(fd, buf, sizeof(buf));
  if (n < 16) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return; }

  uint8_t  fix    = buf[0];
  uint8_t  numsat = buf[1];
  int32_t  lat    = LE_I32(buf, 2);
  int32_t  lon    = LE_I32(buf, 6);
  uint16_t alt    = LE_U16(buf, 10);
  uint16_t speed  = LE_U16(buf, 12);
  uint16_t course = LE_U16(buf, 14);

  printf("  fix=%d  satellites=%d\n", fix, numsat);
  printf("  latitude=%.7f  longitude=%.7f\n", lat / 1e7, lon / 1e7);
  printf("  altitude=%dm  speed=%dcm/s  course=%.1fdeg\n", alt, speed, course / 10.0);

  if (!fix)
    printf("  [INFO] No GPS fix (outdoors / antenna required)\n");
}

static void print_comp_gps(int fd)
{
  printf("\n--- MSP_COMP_GPS ---\n");
  fc_send(fd, MSP_COMP_GPS);

  uint8_t buf[16];
  int n = fc_recv(fd, buf, sizeof(buf));
  if (n < 5) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return; }

  uint16_t dist      = LE_U16(buf, 0);
  uint16_t direction = LE_U16(buf, 2);
  uint8_t  heartbeat = buf[4];

  printf("  distanceToHome=%dm  directionToHome=%ddeg  heartbeat=%d\n",
         dist, direction, heartbeat);
}

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== GPS  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  print_raw_gps(fd);
  print_comp_gps(fd);

  close(fd);
  return 0;
}
