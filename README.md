# vtx

Video and telemetry transmission using GStreamer.

> **Note:** Documentation is under active development.

## Prerequisites

### Development Tools

```bash
sudo apt install -y curl git bear ethtool
```

```bash
sudo apt install -y nodejs npm
mkdir -p ~/.npm-global
npm config set prefix '~/.npm-global'
echo 'export PATH=$HOME/.npm-global/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

```bash
sudo apt install -y docker.io
sudo usermod -aG docker $(whoami)
```

### Build Dependencies

```bash
sudo apt install -y v4l-utils libasound2-dev libssl-dev libsoup-3.0-dev libjson-glib-dev libnice-dev

sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev
```

### Runtime Dependencies

```bash
sudo apt install -y gstreamer1.0-tools gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-nice gstreamer1.0-vaapi gstreamer1.0-alsa
```

### GStreamer Plugin Configuration

Disable conflicting plugins to ensure proper hardware access:

```bash
GST=$(pkg-config --variable=pluginsdir gstreamer-1.0)

# Use ALSA instead of PulseAudio
[ -f "$GST/libgstpulseaudio.so" ] && sudo mv "$GST/libgstpulseaudio.so" "$GST/libgstpulseaudio.so.disabled"

# Use V4L2 instead of libcamera
# [ -f "$GST/libgstlibcamera.so" ] && sudo mv "$GST/libgstlibcamera.so" "$GST/libgstlibcamera.so.disabled"

# Disable PipeWire
[ -f "$GST/libgstpipewire.so" ] && sudo mv "$GST/libgstpipewire.so" "$GST/libgstpipewire.so.disabled"
```

### User Group Configuration

```bash
# GPU access
sudo usermod -aG render $(whoami)

# FC access
sudo usermod -aG dialout $USER

# WAP access
sudo usermod -aG netdev $USER
```

> **Note:** Log out and back in for group changes to take effect.

## Building

```bash
make all PLATFORM=LINUX_X86
```

## Install

### 1. Start the Signaling Server

```bash
docker run -itd \
  --user root \
  --name fpvjp-app \
  -p 443:443 \
  --restart unless-stopped \
  fpvjp/app:latest
```

### 2. Extract CA Certificate

```bash
docker cp fpvjp-app:/app/certificate/server-ca-cert.pem .
```

### 3. Install as System Service

```bash
cd service
sudo bash setup.sh
```

## Development

To run vtx manually for debugging:

```bash
# Stop and disable the system service
sudo systemctl stop vtx.service
sudo systemctl disable vtx.service

# Run manually
./vtx
```

## Service Management

```bash
sudo systemctl status vtx.service    # Check status
sudo systemctl start vtx.service     # Start service
sudo systemctl stop vtx.service      # Stop service
sudo systemctl restart vtx.service   # Restart service
sudo journalctl -u vtx.service -f    # View logs
```
