#pragma once

// Common serial helpers for FC MSP tests

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULT_PORT "/dev/ttyACM0"
#define DEFAULT_BAUD B115200

static inline int fc_open(const char *port)
{
  int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) { perror("open"); return -1; }

  struct termios tty;
  memset(&tty, 0, sizeof(tty));
  tcgetattr(fd, &tty);
  cfsetospeed(&tty, DEFAULT_BAUD);
  cfsetispeed(&tty, DEFAULT_BAUD);
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_iflag &= ~IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 10;
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
  tcsetattr(fd, TCSANOW, &tty);
  sleep(2);
  tcflush(fd, TCIOFLUSH);
  return fd;
}

static inline int fc_send(int fd, uint8_t cmd)
{
  uint8_t pkt[] = {'$', 'M', '<', 0, cmd, cmd};
  return write(fd, pkt, sizeof(pkt)) == (ssize_t) sizeof(pkt);
}

// Returns payload size on success, -1 on timeout/error
static inline int fc_recv(int fd, uint8_t *buf, int max)
{
  uint8_t b;
  int state = 0;
  for (int i = 0; i < 300; i++) {
    if (read(fd, &b, 1) != 1) continue;
    if      (state == 0 && b == '$') state = 1;
    else if (state == 1 && b == 'M') state = 2;
    else if (state == 2 && b == '>') { state = 3; break; }
    else state = 0;
  }
  if (state != 3) { fprintf(stderr, "  [ERROR] Header not received (timeout)\n"); return -1; }

  uint8_t size, cmd;
  if (read(fd, &size, 1) != 1 || read(fd, &cmd, 1) != 1) return -1;

  int n = (size < max) ? size : max;
  for (int i = 0; i < n; i++) {
    if (read(fd, &buf[i], 1) != 1) return -1;
  }
  uint8_t cs; read(fd, &cs, 1);
  return size;
}

// Little-endian read helpers
#define LE_I16(b, o) ((int16_t)  ((b)[o] | (b)[(o)+1] << 8))
#define LE_U16(b, o) ((uint16_t) ((b)[o] | (b)[(o)+1] << 8))
#define LE_I32(b, o) ((int32_t)  ((b)[o] | (b)[(o)+1]<<8 | (b)[(o)+2]<<16 | (b)[(o)+3]<<24))
#define LE_U32(b, o) ((uint32_t) ((b)[o] | (b)[(o)+1]<<8 | (b)[(o)+2]<<16 | (b)[(o)+3]<<24))
