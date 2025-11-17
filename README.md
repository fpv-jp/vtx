# vtx
Using GStreamer to transmit video and telemetry

# ⚠️documentation is currently being written.

## Viewer requirements

Most device profiles (for example `PLATFORM=RPI4_V4L2`) advertise pipelines that produce H.264 video and Opus audio. The WebRTC peer that connects to `vtx` must therefore support decoding those codecs. Browsers on macOS ship with H.264, but many Ubuntu builds (Snap Chromium, Firefox without OpenH264, etc.) reject the video m-line, which prevents the pipeline from starting.

To view the stream from Ubuntu, install a browser that has H.264 built in (Google Chrome, Microsoft Edge) or install the extra codec packages, e.g.

```bash
sudo apt install chromium-codecs-ffmpeg-extra
```

Without H.264/Opus support the peer will reject the SDP offer and the stream will be torn down by `vtx`.
