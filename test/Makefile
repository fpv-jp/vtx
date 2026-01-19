.PHONY: help camera_info mic_info video_src audio_src gst-encoders wifi_dev wifi_scan net_info full

## ===== Help =====

help:  ## Show available Make targets
	@echo "==== Available Make Targets ===="
	@grep -E '^[a-zA-Z_-]+:.*?##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?##"}; {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}'

## ===== Media Device Info =====

camera_info:  ## Show camera device info
	@echo "=========== Camera Device Info ==========="
	@which v4l2-ctl >/dev/null 2>&1 && v4l2-ctl --list-devices || echo "v4l2-ctl not found"
	@echo ""
	@which v4l2-ctl >/dev/null 2>&1 && v4l2-ctl --list-formats-ext || echo "v4l2-ctl not found"
	@echo ""
	@which libcamera-vid >/dev/null 2>&1 && libcamera-vid --list-cameras || echo "libcamera-vid not found"
	@echo ""

mic_info:  ## Show microphone device info
	@echo "=========== Microphone Devices Info ==========="
	@which arecord >/dev/null 2>&1 && arecord -l || echo "arecord not available"
	@echo ""

## ===== Media Info =====

video_src:  ## Show GStreamer video sources
	@echo "=========== Video Sources ==========="
	@which gst-device-monitor-1.0 >/dev/null 2>&1 && gst-device-monitor-1.0 Video/Source || echo "gst-device-monitor-1.0 not found"
	@echo ""

audio_src:  ## Show GStreamer audio sources
	@echo "=========== Audio Sources ==========="
	@which gst-device-monitor-1.0 >/dev/null 2>&1 && gst-device-monitor-1.0 Audio/Source || echo "gst-device-monitor-1.0 not found"
	@echo ""

gst-encoders:  ## Show available GStreamer encoders
	@echo "=========== GStreamer Encoders ==========="
	@gst-inspect-1.0 | grep -Ei 'enc' | grep -Ei '264|265|av1|vp8|vp9|opus|mulaw' | grep -Eiv 'json|timestamper|alpha'
	@echo ""

## ===== Network Info =====

net_info:  ## Show IP and network info
	@echo "=========== Network Info ==========="
	@ip addr show
	@echo ""

wifi_dev:  ## Show detailed wireless capability info
	@echo "=========== Wireless Device Info ==========="
# 	@which /usr/sbin/iw >/dev/null 2>&1 && /usr/sbin/iw list || echo "iw not found"
	@which /usr/sbin/iw >/dev/null 2>&1 && /usr/sbin/iw dev || echo "iw not found"
	@echo ""

wifi_scan:  ## Scan for available WiFi networks
	@echo "=========== WiFi Network Scan ==========="
	@which nmcli >/dev/null 2>&1 && nmcli dev wifi list || echo "nmcli not found"
	@echo ""

## ===== Full Report =====

full:  ## Run all diagnostics together
	@$(MAKE) --no-print-directory camera_info
	@$(MAKE) --no-print-directory mic_info
	@$(MAKE) --no-print-directory video_src
	@$(MAKE) --no-print-directory audio_src
	@$(MAKE) --no-print-directory gst-encoders
	@$(MAKE) --no-print-directory net_info
	@$(MAKE) --no-print-directory wifi_dev
	@$(MAKE) --no-print-directory wifi_scan
