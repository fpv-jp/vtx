# vtx
Using GStreamer to transmit video and telemetry



# Github
```bash
ssh-keygen -t ed25519 -C "raspi-4b@fpv.jp"
cat ~/.ssh/id_ed25519.pub
# https://github.com/settings/keys
ssh -T git@github.com

git config --global user.email "raspi-4b@fpv.jp"
git config --global user.name "FPV Japan"
```

# Github Actions runner
```bash
sudo cp actions-runner.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable actions-runner
```

# SSH
```bash
sudo apt install -y openssh-server
sudo systemctl enable ssh
```

# Common dependency
```bash
sudo apt-get install -y \
gstreamer1.0-tools \
gstreamer1.0-plugins-base-apps \
gstreamer1.0-plugins-bad \
gstreamer1.0-nice \
gstreamer1.0-alsa
```

# Additional Libraries　(Raspberry Pi libcamera)
```bash
sudo apt-get install -y gstreamer1.0-libcamera
```

# Additional Libraries　(Rock5)
```bash
sudo apt install -y librga-dev
```

# Common　development dependency
```bash
sudo apt install -y \
libgstreamer1.0-dev \
libgstreamer-plugins-base1.0-dev \
libgstreamer-plugins-bad1.0-dev \
libjson-glib-dev \
libnice-dev \
libssl-dev \
libsoup-3.0-dev # or libsoup-2.4-dev
```
