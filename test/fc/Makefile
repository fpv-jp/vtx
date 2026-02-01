.PHONY: all imu gps attitude altitude analog battery sonar build clean help

PORT     ?= /dev/ttyACM0
CC       := gcc
CFLAGS   := -Wall -I.
BUILD    := build

TARGETS  := imu gps attitude altitude analog battery sonar

## ===== Help =====

help:  ## Show available Make targets
	@echo "==== FC Data Test ===="
	@echo "  PORT=$(PORT)  (override: PORT=/dev/ttyUSB0 make gps)"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?##"}; {printf "  \033[36m%-12s\033[0m %s\n", $$1, $$2}'

## ===== Build & Run (individual) =====

imu: $(BUILD)/imu  ## Accelerometer / Gyroscope / Magnetometer  (MSP 102)
	@./$(BUILD)/imu $(PORT)

gps: $(BUILD)/gps  ## GPS fix / position / compass home  (MSP 106, 107)
	@./$(BUILD)/gps $(PORT)

attitude: $(BUILD)/attitude  ## Roll / Pitch / Heading  (MSP 108)
	@./$(BUILD)/attitude $(PORT)

altitude: $(BUILD)/altitude  ## Barometer altitude / variometer  (MSP 109)
	@./$(BUILD)/altitude $(PORT)

analog: $(BUILD)/analog  ## Voltage / Amperage / RSSI / mAh  (MSP 110)
	@./$(BUILD)/analog $(PORT)

battery: $(BUILD)/battery  ## Battery state / cells / capacity  (MSP 130)
	@./$(BUILD)/battery $(PORT)

sonar: $(BUILD)/sonar  ## Sonar altitude  (MSP 58)
	@./$(BUILD)/sonar $(PORT)

## ===== Run all / Build =====

all: build  ## Build and run all tests
	@for t in $(TARGETS); do \
		echo ""; \
		./$(BUILD)/$$t $(PORT); \
	done

build: $(addprefix $(BUILD)/, $(TARGETS))  ## Build all test binaries only

$(BUILD)/%: %.c common.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

## ===== Clean =====

clean:  ## Remove build artifacts
	rm -rf $(BUILD)
