# vtx

Video and telemetry transmission using GStreamer.

This program uses GStreamer's `webrtcbin` to transmit video, audio, and telemetry via WebRTC.

The transmission flow is as follows:

1. Upon request from vrx, vtx returns a list of available Wi-Fi interfaces, flight controllers, cameras, microphones, and encoders.
2. vrx selects the desired devices and pipeline configuration, and sends a request to vtx. vtx then responds with an SDP offer to initiate negotiation.
3. Once SDP answer and ICE negotiation with vrx completes, video and audio streaming begins.
4. A DataChannel is opened to transmit telemetry (sensor values read from the flight controller).

The WebRTC direction is `sendonly` (vtx → vrx).

## Architecture

```
main.c
 └─ signaling.c          WebSocket signaling (libsoup)
      ├─ webrtc.c         webrtcbin control, SDP offer generation, ICE negotiation
      │   ├─ pipeline_factory.c  GStreamer pipeline string assembly and launch
      │   └─ ice.c        Custom ICE agent (when network_interface is specified)
      ├─ datachannel.c           DataChannel (telemetry transmission)
      │   ├─ datachannel_command.c
      │   ├─ datachannel_flight_controller.c  Receives data from flight controller via MSP
      │   └─ datachannel_wpa_supplicant.c     Wi-Fi status notifications
      ├─ msp.c / msp_common.c    MSP (MultiWii Serial Protocol) implementation
      ├─ nic.c / nic_parser.c    Wi-Fi NIC detection and information gathering
      ├─ device.c                Camera and microphone device enumeration
      ├─ codec.c                 Encoder availability inspection
      ├─ rtp.c                   RTP utilities
      └─ utils.c                 Common utilities and cleanup
```

## Session Flow

```
vtx                          signaling server                    vrx
 |                                |                               |
 |--- WebSocket connect (sender) ->|                               |
 |<-- SENDER_SESSION_ID_ISSUANCE --|                               |
 |--- SENDER_PLATFORM_INFO ------->|                               |
 |                                |<-- SENDER_MEDIA_DEVICE_LIST_REQUEST --|
 |<-- SENDER_MEDIA_DEVICE_LIST_REQUEST --|                         |
 |--- RECEIVER_MEDIA_DEVICE_LIST_RESPONSE ->|-- response --------->|
 |                                |<-- SENDER_MEDIA_STREAM_START ---------|
 |  [pipeline launch, SDP offer generation]                        |
 |--- RECEIVER_SDP_OFFER ---------->|---------- offer ------------>|
 |<-- SENDER_SDP_ANSWER ------------|<--------- answer ------------|
 |<-- SENDER_ICE ------------------|<----------- ICE --------------|
 |--- RECEIVER_ICE ---------------->|------------ ICE ------------>|
 |                [video, audio, DataChannel streaming starts]
```

### Supported Platforms

- macOS
- Debian/Ubuntu (requires an Nvidia, AMD, or Intel GPU)
- Raspberry Pi 4B
- Raspberry Pi Compute Module 4
- Raspberry Pi 5B (software encoding only)
- Raspberry Pi Compute Module 5 (software encoding only)
- Jetson Nano 2GB Developer Kit
- Jetson Orin Nano Super (software encoding only)
- Radxa ROCK 5B
- Radxa ROCK 5T

## Development Setup

The following instructions assume remote development over SSH (e.g., using VS Code Remote SSH) connected to a development machine.

**Prerequisite:** Complete the GStreamer installation by following the instructions at [fpv-jp/bsp - GStreamer](https://github.com/fpv-jp/bsp/tree/main/Gstreamer).

**Build dependencies:** On Debian/Ubuntu, install the following packages before running `make`:

```bash
sudo apt-get install -y \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libgstreamer-plugins-bad1.0-dev \
  libsoup-3.0-dev \
  libjson-glib-dev \
  libnice-dev
```

> `libgstreamer-plugins-bad1.0-dev` provides `gstreamer-webrtc-1.0` and `gstreamer-webrtc-nice-1.0`, and `libnice-dev` provides `nice`, both required by the Makefile's `pkg-config` checks. If your distribution ships only `libsoup-2.4` (e.g. older Ubuntu/Debian releases), install `libsoup2.4-dev` instead — the Makefile auto-detects whichever is available.

**Radxa ROCK 5B / ROCK 5T:** the Rockchip build of `gstreamer-video-1.0`/`gstreamer-sdp-1.0` depends on `librga` for the RGA 2D accelerator. The Radxa apt repo (`radxa-repo`) ships the runtime library `librga2` but not the `-dev` package, so `make` fails with `Package 'librga', required by 'gstreamer-video-1.0', not found` even though GStreamer itself is installed. Install it explicitly:

```bash
sudo apt-get install -y librga-dev
```

### 1. Start the signaling server

Run the signaling server from [app](https://github.com/fpv-jp/app) locally via Docker:

```bash
docker run -itd \
  --user root \
  --name fpvjp-app \
  -p 443:443 \
  --restart unless-stopped \
  fpvjp/app:latest
```

### 2. Point `fpv` at the local signaling server

The default `SIGNALING_ENDPOINT` is `wss://fpv/signaling`, so add a loopback entry for `fpv` to `/etc/hosts`:

```sh
echo "127.0.0.1 localhost fpv" | sudo tee -a /etc/hosts
```

### 3. Build

```
make
```

### 4. Install runtime GStreamer plugins

Building successfully does not mean `vtx` can run. GStreamer resolves plugins at runtime through its plugin registry, which is separate from the `-dev` packages installed above needed only to build. On startup `vtx` checks that every plugin it needs is present and exits with `Required GStreamer plugin 'X' not found` for anything missing.

Pick the command for your platform and run it as-is:

**Raspberry Pi (4B / CM4 / 5 / CM5):**

```bash
sudo apt-get install -y \
  gstreamer1.0-plugins-base-apps \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-alsa \
  gstreamer1.0-nice \
  gstreamer1.0-libcamera
```

**Debian/Ubuntu x86 (Intel/AMD/Nvidia GPU):**

```bash
sudo apt-get install -y \
  gstreamer1.0-plugins-base-apps \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-alsa \
  gstreamer1.0-nice
```

> The HW encoder plugins (`va`, `amfcodec`, `nvcodec`) ship inside `gstreamer1.0-plugins-bad` above — just make sure the vendor driver is installed (e.g. `intel-media-va-driver` for Intel).

**Jetson / Radxa ROCK:**

```bash
sudo apt-get install -y \
  gstreamer1.0-plugins-base-apps \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-alsa \
  gstreamer1.0-nice
```

> The camera and HW-encoder plugins (`nvarguscamerasrc`, `rockchipmpp`, etc.) come from the vendor BSP rather than standard apt packages — see the platform's BSP setup docs.

<details>
<summary>What each package provides</summary>

- `gstreamer1.0-plugins-base-apps` — the `gst-device-monitor-1.0` tool used for device enumeration, plus `opus` (pulled in as a dependency of the packages below)
- `gstreamer1.0-plugins-bad` — `webrtc`, `dtls`, `srtp`, `videoparsersbad`, `opusparse`
- `gstreamer1.0-plugins-good` — `rtp`, `rtpmanager`, `video4linux2`
- `gstreamer1.0-alsa` — `alsa` (a separate package on Debian/Raspberry Pi OS)
- `gstreamer1.0-nice` — `nice` (ICE)
- `gstreamer1.0-libcamera` — `libcamera` (Raspberry Pi camera source)

</details>

### 5. Download the CA certificate

```sh
curl -L -o server-ca-cert.pem https://raw.githubusercontent.com/fpv-jp/app/refs/heads/main/certificate/server-ca-cert.pem
```

### 6. Run

```
./vtx
```

> **Note:** To use the public fpv.jp signaling server for testing:
> ```
> SIGNALING_ENDPOINT=wss://fpv.jp/signaling SERVER_CERTIFICATE_AUTHORITY= ./vtx
> ```

## Production Deployment

**Prerequisite:** For Wi-Fi configuration, see [fpv-jp/bsp - Wifi](https://github.com/fpv-jp/bsp/tree/main/Wifi).

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
```

## Service Management

```bash
sudo systemctl status vtx.service    # Check status
sudo systemctl start vtx.service     # Start service
sudo systemctl stop vtx.service      # Stop service
sudo systemctl restart vtx.service   # Restart service
sudo journalctl -u vtx.service -f    # View logs
```
