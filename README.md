# vtx
Using GStreamer to transmit video and telemetry

# ⚠️documentation is currently being written.

```

sudo apt install -y curl git linux-headers-generic build-essential dkms ethtool nodejs golang npm docker.io

sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libsoup-3.0-dev v4l-utils libjson-glib-dev libnice-dev libssl-dev

sudo apt-get install -y gstreamer1.0-tools gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-nice gstreamer1.0-vaapi gstreamer1.0-alsa

sudo mv /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstpulseaudio.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstpulseaudio.so.disable

sudo usermod -a -G docker `whoami`
sudo usermod -a -G render `whoami`

sudo nmcli connection modify "???SSID???" connection.autoconnect yes
sudo nmcli connection modify "???SSID???" connection.autoconnect-priority 10
```
