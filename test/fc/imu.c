// MSP_RAW_IMU (102): Accelerometer / Gyroscope / Magnetometer
// acc[3] + gyro[3] + mag[3] = 9 x int16 = 18 bytes

#include "common.h"

#define MSP_RAW_IMU 102

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_RAW_IMU  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_RAW_IMU);

  uint8_t buf[32];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 18) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  int16_t ax = LE_I16(buf,  0), ay = LE_I16(buf,  2), az = LE_I16(buf,  4);
  int16_t gx = LE_I16(buf,  6), gy = LE_I16(buf,  8), gz = LE_I16(buf, 10);
  int16_t mx = LE_I16(buf, 12), my = LE_I16(buf, 14), mz = LE_I16(buf, 16);

  printf("Accelerometer (raw / 512 = g):\n");
  printf("  ax=%-6d ay=%-6d az=%-6d  (%.3fg %.3fg %.3fg)\n",
         ax, ay, az, ax/512.0, ay/512.0, az/512.0);

  printf("Gyroscope (raw / 4 = deg/s):\n");
  printf("  gx=%-6d gy=%-6d gz=%-6d  (%.1f°/s %.1f°/s %.1f°/s)\n",
         gx, gy, gz, gx/4.0, gy/4.0, gz/4.0);

  printf("Magnetometer (raw):\n");
  printf("  mx=%-6d my=%-6d mz=%-6d\n", mx, my, mz);

  return 0;
}
