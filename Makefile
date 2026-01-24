.PHONY: all clean fmt fmt-gst help test test-clean

# ========= BasicConfig =========
CC            := gcc
TARGET        := vtx
TARGET_TEST   := vtx_test
SRC_DIR       := src
SRC_DIR_TEST  := test
INCLUDE_DIR   := $(SRC_DIR)/headers
UNITY_DIR     := $(SRC_DIR_TEST)/unity
PKG_CONFIG    := $(shell which pkg-config)

# ========= SourceCode =========
SRCS          := $(wildcard $(SRC_DIR)/*.c)
SRCS_NO_MAIN  := $(filter-out $(SRC_DIR)/main.c, $(SRCS))
SRCS_TEST     := $(filter-out $(UNITY_DIR)/unity.c, $(wildcard $(SRC_DIR_TEST)/*.c))
HEADERS       := $(wildcard $(INCLUDE_DIR)/*.h)

# ========= libsoup Auto-Detection =========
SOUP_PKG := $(shell $(PKG_CONFIG) --exists libsoup-3.0 && echo libsoup-3.0 || echo libsoup-2.4)

ifeq ($(shell $(PKG_CONFIG) --exists $(SOUP_PKG) && echo yes || echo no),no)
    $(error libsoup not found. Please install libsoup-3.0 or libsoup-2.4)
endif

# ========= Flag & Link =========
CFLAGS += -DGST_USE_UNSTABLE_API \
          `$(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-webrtc-1.0 $(SOUP_PKG) json-glib-1.0 nice` \
          -I$(INCLUDE_DIR) -I$(UNITY_DIR)

LIBS   += `$(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 gstreamer-rtp-1.0 gstreamer-webrtc-nice-1.0 $(SOUP_PKG) json-glib-1.0 nice`

# ========= Build =========
OBJS := $(SRCS:.c=.o)

all: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_TEST)
	find . \( -name '*~' \) -delete

# ========= TestBuild (Unity) =========
UNITY_SRC := $(UNITY_DIR)/unity.c
TEST_EXE  := $(TARGET_TEST)

test: test-clean $(TEST_EXE)
	./$(TEST_EXE)

$(TEST_EXE): $(SRCS_NO_MAIN) $(SRCS_TEST) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test-clean:
	rm -f $(TEST_EXE)

# ========= Formatter =========
# fmt:
# 	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 gst-indent-1.0

# ========= Build EAR =========
bear:
	bear -- make test

# ========= Help =========
help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Platform is auto-detected at runtime via /proc/device-tree/model"
	@echo "Supported platforms: RPI4, RPI5, ROCK 5, Jetson Nano, Jetson Orin, Intel Mac, Linux x86"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build production binary"
	@echo "  test        - Build and run unit tests with Unity"
	@echo "  clean       - Clean production build"
	@echo "  test-clean  - Clean test build"
	@echo "  bear        - Generate compile_commands.json"
	@echo "  help        - Show this help message"
