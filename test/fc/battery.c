// MSP_BATTERY_STATE (130): Cell count / capacity / voltage / current / state
// cellCount(u8) + capacity(u16 mAh) + voltage(u8*0.1V) + mAhDrawn(u16) +
// amperage(u16*0.01A) + batteryState(u8) + milliVoltage(u16 mV) = 10 bytes

#include "common.h"

#define MSP_BATTERY_STATE 130

static const char *battery_state_str(uint8_t state)
{
  switch (state) {
    case 0: return "OK";
    case 1: return "WARNING";
    case 2: return "CRITICAL";
    case 3: return "NOT_PRESENT";
    default: return "UNKNOWN";
  }
}

int main(int argc, char *argv[])
{
  const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;
  printf("=== MSP_BATTERY_STATE  port=%s ===\n", port);

  int fd = fc_open(port);
  if (fd < 0) return 1;

  fc_send(fd, MSP_BATTERY_STATE);

  uint8_t buf[16];
  int n = fc_recv(fd, buf, sizeof(buf));
  close(fd);

  if (n < 10) { fprintf(stderr, "Insufficient data (%d bytes)\n", n); return 1; }

  uint8_t  cell_count   = buf[0];
  uint16_t capacity     = LE_U16(buf, 1);
  uint8_t  voltage_raw  = buf[3];
  uint16_t mah_drawn    = LE_U16(buf, 4);
  uint16_t amperage     = LE_U16(buf, 6);
  uint8_t  state        = buf[8];
  uint16_t milli_volt   = LE_U16(buf, 9);

  printf("  state       = %s (%d)\n", battery_state_str(state), state);
  printf("  cells       = %d\n",   cell_count);
  printf("  voltage     = %.1fV  (%dmV)\n", voltage_raw * 0.1, milli_volt);
  printf("  capacity    = %dmAh\n", capacity);
  printf("  mAh drawn   = %dmAh\n", mah_drawn);
  printf("  amperage    = %.2fA\n", amperage / 100.0);

  return 0;
}
