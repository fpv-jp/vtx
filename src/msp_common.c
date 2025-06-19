#include <dirent.h>
#include <fcntl.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "headers/msp.h"

// Check if the device is a flight controller by reading USB vendor/product info
static int is_flight_controller(const char *port_name)
{
  char sysfs_path[512];
  char vid_path[600];
  char pid_path[600];
  FILE *fp;
  char vid[8] = {0};
  char pid[8] = {0};

#ifdef __APPLE__
  // macOS doesn't have sysfs, so we can't check vendor/product ID easily
  return 1;
#else
  // Extract tty device name (e.g., ttyACM0 from /dev/ttyACM0)
  const char *dev_name = strrchr(port_name, '/');
  if (dev_name == NULL)
  {
    return 0;
  }
  dev_name++;  // Skip the '/'

  // Build path to sysfs
  snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/tty/%s/device", dev_name);

  // Read real path (follows symlinks)
  char real_path[512];
  if (realpath(sysfs_path, real_path) == NULL)
  {
    return 0;
  }
  // Navigate up to USB device directory and read idVendor/idProduct
  // Check that we have enough space for the path construction
  if (strlen(real_path) > sizeof(vid_path) - 20)  // 20 bytes for "/../idVendor" + safety margin
  {
    return 0;  // Path too long
  }

  snprintf(vid_path, sizeof(vid_path), "%s/../idVendor", real_path);
  snprintf(pid_path, sizeof(pid_path), "%s/../idProduct", real_path);

  // Read Vendor ID
  fp = fopen(vid_path, "r");
  if (fp != NULL)
  {
    if (fgets(vid, sizeof(vid), fp) != NULL)
    {
      // Remove newline
      vid[strcspn(vid, "\n")] = 0;
    }
    fclose(fp);
  }
  // Read Product ID
  fp = fopen(pid_path, "r");
  if (fp != NULL)
  {
    if (fgets(pid, sizeof(pid), fp) != NULL)
    {
      // Remove newline
      pid[strcspn(pid, "\n")] = 0;
    }
    fclose(fp);
  }
  // Common flight controller vendor IDs
  // 0483 = STMicroelectronics (common for F4/F7/H7 flight controllers)
  // 1209 = pid.codes (used by some open-source flight controllers)
  // 2341 = Arduino (some flight controllers use Arduino bootloader)
  if (strcmp(vid, "0483") == 0 || strcmp(vid, "1209") == 0 || strcmp(vid, "2341") == 0)
  {
    printf("Flight controller detected: VID=%s PID=%s on %s\n", vid, pid, port_name);
    return 1;
  }
  // Exclude Raspberry Pi's built-in UART (usually ttyAMA0 or ttyS0)
  if (strstr(dev_name, "ttyAMA") != NULL || strstr(dev_name, "ttyS") == NULL)  // Fixed: was == 0, should be == NULL
  {
    return 0;
  }

  return 0;
#endif
}

// Serial port auto-detection with flight controller filtering
const char *vtx_msp_detect()
{
  static char detected_port[512];
  DIR *dir;
  struct dirent *entry;

#ifdef __APPLE__
  // macOS: Scan /dev/cu.* devices
  const char *dev_dir = "/dev";
  const char *prefixes[] = {"cu.usbmodem", "cu.usbserial", "cu.SLAB_USBtoUART", NULL};
#else
  // Linux: Scan /dev/tty* devices
  const char *dev_dir = "/dev";
  const char *prefixes[] = {"ttyACM", "ttyUSB", "ttyAMA", "ttyS", NULL};
#endif

  dir = opendir(dev_dir);
  if (dir == NULL)
  {
    gst_printerrln("Failed to open device directory: %s", dev_dir);
    return NULL;
  }
  // Scan device directory
  while ((entry = readdir(dir)) != NULL)
  {
    // Check if it matches any prefix
    for (int i = 0; prefixes[i] != NULL; i++)
    {
      if (strncmp(entry->d_name, prefixes[i], strlen(prefixes[i])) == 0)
      {
        snprintf(detected_port, sizeof(detected_port), "%s/%s", dev_dir, entry->d_name);

        // Check if the device can actually be opened
        int fd = open(detected_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0)
        {
          close(fd);

          // Check if it's a flight controller
          if (is_flight_controller(detected_port))
          {
            closedir(dir);
            gst_println("[MSP] Flight controller serial port detected: %s", detected_port);
            return detected_port;
          }
        }
      }
    }
  }

  closedir(dir);
  gst_printerrln("[MSP] No flight controller serial port found");
  return NULL;
}

// More detailed detection (multiple device support)
int vtx_msp_detect_all(char ***ports_out)
{
  gst_println("----- Check Serial port -----");
  char **ports = NULL;
  int count = 0;
  int capacity = 10;
  DIR *dir;
  struct dirent *entry;

  ports = malloc(capacity * sizeof(char *));
  if (ports == NULL)
  {
    return 0;
  }
#ifdef __APPLE__
  const char *dev_dir = "/dev";
  const char *prefixes[] = {"cu.usbmodem", "cu.usbserial", "cu.SLAB_USBtoUART", NULL};
#else
  const char *dev_dir = "/dev";
  const char *prefixes[] = {"ttyACM", "ttyUSB", "ttyAMA", "ttyS", NULL};
#endif

  dir = opendir(dev_dir);
  if (dir == NULL)
  {
    free(ports);
    return 0;
  }

  while ((entry = readdir(dir)) != NULL)
  {
    for (int i = 0; prefixes[i] != NULL; i++)
    {
      if (strncmp(entry->d_name, prefixes[i], strlen(prefixes[i])) == 0)
      {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dev_dir, entry->d_name);

        // Check if the device can actually be opened
        int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0)
        {
          close(fd);

          // Check if it's a flight controller
          if (!is_flight_controller(path))
          {
            continue;
          }
          // Expand capacity if exceeded
          if (count >= capacity)
          {
            capacity *= 2;
            char **new_ports = realloc(ports, capacity * sizeof(char *));
            if (new_ports == NULL)
            {
              // Free existing memory on memory allocation failure
              for (int j = 0; j < count; j++)
              {
                free(ports[j]);
              }
              free(ports);
              closedir(dir);
              return 0;
            }
            ports = new_ports;
          }

          ports[count] = strdup(path);
          if (ports[count] == NULL)
          {
            // Memory allocation failure
            for (int j = 0; j < count; j++)
            {
              free(ports[j]);
            }
            free(ports);
            closedir(dir);
            return 0;
          }
          count++;
          printf("[MSP] Flight controller serial port detected: %s\n", path);
        }
      }
    }
  }

  closedir(dir);
  *ports_out = ports;
  return count;
}

// ミリ秒単位でスリープ
// void vtx_msp_sleep_ms(int milliseconds)
// {
//   struct timespec ts;
//   ts.tv_sec = milliseconds / 1000;
//   ts.tv_nsec = (milliseconds % 1000) * 1000000;
//   nanosleep(&ts, NULL);
// }

// MSP初期化
int vtx_msp_init(MSP *msp, const char *port, speed_t baudrate)
{
  msp->serial_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
  if (msp->serial_fd < 0)
  {
    gst_printerrln("[MSP] unable to open serial port: %s", port);
    return 0;
  }

  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(msp->serial_fd, &tty) != 0)
  {
    gst_printerrln("[MSP] tcgetattr error");
    return 0;
  }

  cfsetospeed(&tty, baudrate);
  cfsetispeed(&tty, baudrate);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8ビット
  tty.c_iflag &= ~IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 5;  // 0.5秒タイムアウト

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | PARODD);
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(msp->serial_fd, TCSANOW, &tty) != 0)
  {
    gst_printerrln("[MSP] tcgetattr error");
    return 0;
  }

  sleep(2);

  tcflush(msp->serial_fd, TCIOFLUSH);

  gst_println("[MSP] connection successful: %s", port);
  return 1;
}

// MSP終了処理
void vtx_msp_close(MSP *msp)
{
  if (msp->serial_fd >= 0)
  {
    close(msp->serial_fd);
  }
}

// MSPコマンド送信
int msp_send_command(MSP *msp, uint8_t cmd, const uint8_t *data, uint8_t data_size)
{
  uint8_t checksum = data_size ^ cmd;

  // パケット構築: $M< + size + cmd + data + checksum
  uint8_t packet[256];
  int idx = 0;
  packet[idx++] = '$';
  packet[idx++] = 'M';
  packet[idx++] = '<';
  packet[idx++] = data_size;
  packet[idx++] = cmd;

  for (int i = 0; i < data_size; i++)
  {
    packet[idx++] = data[i];
    checksum ^= data[i];
  }
  packet[idx++] = checksum;

  // 送信
  ssize_t written = write(msp->serial_fd, packet, idx);
  return written == idx;
}

// MSP応答受信
int msp_receive_response(MSP *msp, uint8_t *response, int max_size, int timeout_ms)
{
  uint8_t buffer;
  struct timespec start, now;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // ヘッダー待機: $M>
  while (1)
  {
    if (read(msp->serial_fd, &buffer, 1) == 1)
    {
      if (buffer == '$')
      {
        if (read(msp->serial_fd, &buffer, 1) == 1 && buffer == 'M')
        {
          if (read(msp->serial_fd, &buffer, 1) == 1 && buffer == '>')
          {
            break;  // ヘッダー発見
          }
        }
      }
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
    if (elapsed > timeout_ms)
    {
      gst_printerrln("[MSP] Timeout: Header not found");
      return 0;
    }
  }

  // サイズ読み取り
  uint8_t size;
  if (read(msp->serial_fd, &size, 1) != 1)
  {
    return 0;
  }
  // コマンド読み取り
  uint8_t cmd;
  if (read(msp->serial_fd, &cmd, 1) != 1)
  {
    return 0;
  }
  // データ読み取り
  uint8_t checksum = size ^ cmd;
  for (int i = 0; i < size && i < max_size; i++)
  {
    if (read(msp->serial_fd, &buffer, 1) == 1)
    {
      response[i] = buffer;
      checksum ^= buffer;
    }
    else
    {
      return 0;
    }
  }

  // チェックサム検証
  uint8_t received_checksum;
  if (read(msp->serial_fd, &received_checksum, 1) != 1 || checksum != received_checksum)
  {
    gst_printerrln("checksum error");
    return 0;
  }

  return size;
}

// MSPコマンドを送信して生のレスポンスバイトを取得
int msp_request_raw(MSP *msp, uint16_t cmd, uint8_t *response, size_t response_size)
{
  if (!msp_send_command(msp, cmd, NULL, 0))
  {
    return 0;
  }

  return msp_receive_response(msp, response, response_size, 1000);
}
