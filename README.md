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

## Build

The following instructions assume remote development over SSH (e.g., using VS Code Remote SSH) connected to the target machine.

**Prerequisite:** Complete the GStreamer installation by following the instructions at [fpv-jp/bsp - GStreamer](https://github.com/fpv-jp/bsp/tree/main/Gstreamer).

<details>
<summary>Jetson Nano 2GB (Ubuntu 18.04 "bionic"): full from-source GStreamer build</summary>

The Jetson Nano 2GB ships Ubuntu 18.04, whose apt-provided GStreamer is a stock 1.14.5 — too old for the `GstWebRTCICE` API used by `src/ice.c` (`gst/webrtc/ice.h` doesn't exist yet in 1.14.5). This board needs GStreamer built from source per the BSP repo, with a few adjustments for this specific board/OS combo. Full procedure:

**1. Install apt dependencies.** Skip `libsoup-3.0-dev` from the BSP repo's own dependency list — it doesn't exist in Ubuntu 18.04 apt, and it isn't actually needed (`meson-base.sh` sets `-Dauto_features=disabled` and never enables the `soup` plugin, so the GStreamer build itself has no libsoup dependency). Add `python3.8`/`gcc-8` (see steps 3 and 5) and `libgudev-1.0-dev`/`libudev-dev` (needed by the `v4l2codecs` feature; without it, meson setup fails trying to build a `libgudev` fallback subproject with `Dependency "libudev" not found`) up front:

```bash
sudo apt-get install -y \
  libjson-glib-dev libnice-dev \
  libgstreamer1.0-dev libgstreamer-plugins-bad1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base-apps \
  gstreamer1.0-plugins-bad gstreamer1.0-nice gstreamer1.0-alsa \
  flex bison libdrm-dev libva-dev \
  libasound2-dev libopus-dev libssl-dev \
  libsrtp2-dev libvpx-dev libx265-dev \
  libgudev-1.0-dev libudev-dev \
  python3.8 python3.8-venv python3.8-dev \
  gcc-8 g++-8
```

**2. Clone GStreamer 1.28.0:**

```bash
cd ~
git clone --branch 1.28.0 --depth 1 https://gitlab.freedesktop.org/gstreamer/gstreamer.git
cd gstreamer
```

**3. Create a `python3.8` venv for meson/ninja.** The system `python3` (3.6.9) is too old for recent `meson`:

```bash
python3.8 -m venv meson-venv
. meson-venv/bin/activate
pip install --upgrade pip && pip install meson ninja
```

**4. Fetch the platform build scripts** from the BSP repo into the `gstreamer` source directory (`meson-jetson-nano-2gb.sh` sources `meson-base.sh` from the same directory it's run from):

```bash
curl -sLO https://raw.githubusercontent.com/fpv-jp/bsp/main/Gstreamer/meson-base.sh
curl -sLO https://raw.githubusercontent.com/fpv-jp/bsp/main/Gstreamer/meson-jetson-nano-2gb.sh
chmod +x meson-jetson-nano-2gb.sh
```

**5. Run meson setup with `gcc-8`/`g++-8`.** The stock `gcc-7.5` fails compiling `subprojects/gst-plugins-good/sys/v4l2/gstv4l2object.c` with `initializer element is not constant` — that file relies on a static-aggregate-initializer extension only supported from GCC 8 onward:

```bash
CC=gcc-8 CXX=g++-8 ./meson-jetson-nano-2gb.sh
```

**6. Build.** Use `-j2`/`-j3`, not `-j$(nproc)` — the 2GB board OOMs/thrashes with full parallelism:

```bash
ninja -C build -j3
```

**7. Install and refresh the plugin cache.** `meson-base.sh` uses `--prefix=/usr`, so this overwrites the apt-installed 1.14.5 files in place:

```bash
sudo meson-venv/bin/ninja -C build install
sudo ldconfig
```

**8. Clean up stale apt-owned plugins.** Files for plugins that were merged/renamed upstream (`videoconvert` + `videoscale` → `videoconvertscale`) aren't overwritten by step 7 since the new build uses a different filename, so the old and new plugins both try to register the same GType — this aborts registration for it with `cannot register existing type 'GstVideoScale'`. Check for it and clean up:

```bash
rm -rf ~/.cache/gstreamer-1.0/
gst-inspect-1.0 2>&1 >/dev/null | grep -i critical   # should print nothing
```

If it does print `cannot register existing type` errors, delete the stale file(s) apt still owns at that path and re-run the check above:

```bash
sudo rm -f /usr/lib/aarch64-linux-gnu/gstreamer-1.0/libgstvideoscale.so
sudo rm -f /usr/lib/aarch64-linux-gnu/gstreamer-1.0/libgstvideoconvert.so
```

**9. Verify** the new build is active before moving on to building `vtx` itself:

```bash
gst-inspect-1.0 --version   # should report 1.28.0
gst-inspect-1.0 webrtcbin
```

</details>

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

> On older Ubuntu/Debian releases (e.g. Ubuntu 18.04 "bionic", as shipped on the Jetson Nano 2GB), `libsoup-3.0-dev` does not exist in apt — install `libsoup2.4-dev` instead:
> ```bash
> sudo apt-get install -y \
>   libgstreamer1.0-dev \
>   libgstreamer-plugins-base1.0-dev \
>   libgstreamer-plugins-bad1.0-dev \
>   libsoup2.4-dev \
>   libjson-glib-dev \
>   libnice-dev
> ```
> The Makefile auto-detects whichever of `libsoup-3.0` / `libsoup-2.4` is available via `pkg-config`, so either package works. Note the package name has no hyphen before the version (`libsoup2.4-dev`, not `libsoup-2.4-dev`) — `libsoup2.4-1` is only the runtime shared library and lacks the headers needed to build.
>
> `libgstreamer-plugins-bad1.0-dev` provides `gstreamer-webrtc-1.0` and `gstreamer-webrtc-nice-1.0`, and `libnice-dev` provides `nice`, both required by the Makefile's `pkg-config` checks.

**Radxa ROCK 5B / ROCK 5T:** the Rockchip build of `gstreamer-video-1.0`/`gstreamer-sdp-1.0` depends on `librga` for the RGA 2D accelerator. The Radxa apt repo (`radxa-repo`) ships the runtime library `librga2` but not the `-dev` package, so `make` fails with `Package 'librga', required by 'gstreamer-video-1.0', not found` even though GStreamer itself is installed. Install it explicitly:

```bash
sudo apt-get install -y librga-dev
```

**Runtime GStreamer plugins:** building successfully does not mean `vtx` can run. GStreamer resolves plugins at runtime through its plugin registry, which is separate from the `-dev` packages above needed only to build. On startup `vtx` checks that every plugin it needs is present and exits with `Required GStreamer plugin 'X' not found` for anything missing.

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

Then build:

```bash
make
```

## Run

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

### 3. Download the CA certificate

```sh
curl -L -o server-ca-cert.pem https://raw.githubusercontent.com/fpv-jp/app/refs/heads/main/certificate/server-ca-cert.pem
```

> Alternatively, since the signaling server above is running locally in Docker, you can extract the certificate directly from the container instead:
> ```sh
> docker cp fpvjp-app:/app/certificate/server-ca-cert.pem .
> ```

### 4. Run

```
./vtx
```

> **Note:** To use the public fpv.jp signaling server for testing:
> ```
> SIGNALING_ENDPOINT=wss://fpv.jp/signaling SERVER_CERTIFICATE_AUTHORITY= ./vtx
> ```

## Service Registration

To run `vtx` permanently as a systemd service instead of launching it manually, install it under `/opt/vtx` and register the unit files in `service/`.

**Prerequisite:** For Wi-Fi configuration, see [fpv-jp/bsp - Wifi](https://github.com/fpv-jp/bsp/tree/main/Wifi). Also complete steps 1–3 of [Run](#run) above (signaling server running, `fpv` hostname resolvable, `server-ca-cert.pem` downloaded) and have a built `vtx` binary from [Build](#build).

### 1. Install the binary and certificate

```bash
sudo mkdir -p /opt/vtx
sudo cp vtx server-ca-cert.pem /opt/vtx/
sudo chmod +x /opt/vtx/vtx
```

### 2. Install the environment file

```bash
sudo cp service/vtx.env /etc/vtx.env
```

`/etc/vtx.env` sets `SIGNALING_ENDPOINT` and points `SERVER_CERTIFICATE_AUTHORITY` at `/opt/vtx/server-ca-cert.pem` — edit it if your signaling endpoint differs from the default.

### 3. Register and start the systemd service

```bash
sudo cp service/vtx.service /etc/systemd/system/vtx.service
sudo systemctl daemon-reload
sudo systemctl enable --now vtx.service
sudo systemctl status vtx.service
```

### Service Management

```bash
sudo systemctl status vtx.service    # Check status
sudo systemctl start vtx.service     # Start service
sudo systemctl stop vtx.service      # Stop service
sudo systemctl restart vtx.service   # Restart service
sudo journalctl -u vtx.service -f    # View logs
```

To run `vtx` manually for debugging (see [Run](#run)), first stop and disable the service so they don't conflict:

```bash
sudo systemctl stop vtx.service
sudo systemctl disable vtx.service
```
