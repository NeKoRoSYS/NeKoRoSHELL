CXX ?= g++
CXXFLAGS ?= -O3 -Wall -Wextra
WAYLAND_LIBS = $(shell pkg-config --cflags --libs wayland-client 2>/dev/null)

BUILD_DIR = build
SRC_DIR = src

TARGETS = $(BUILD_DIR)/show-keybinds $(BUILD_DIR)/navbar-hover $(BUILD_DIR)/navbar-watcher $(BUILD_DIR)/hypr-nice $(BUILD_DIR)/eject-forbidden

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/show-keybinds: $(SRC_DIR)/show-keybinds.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/navbar-hover: $(SRC_DIR)/navbar-hover.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(WAYLAND_LIBS)

$(BUILD_DIR)/navbar-watcher: $(SRC_DIR)/navbar-watcher.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(WAYLAND_LIBS)

$(BUILD_DIR)/hypr-nice: $(SRC_DIR)/hypr-nice.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/eject-forbidden: $(SRC_DIR)/eject-forbidden.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
