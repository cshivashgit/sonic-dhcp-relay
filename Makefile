.ONESHELL:
SHELL = /bin/bash

RM := rm -rf
BUILD_DIR := build
BUILD_TEST_DIR := build-test

DHCP4RELAY_TARGET := $(BUILD_DIR)/dhcp4relay
DHCP4RELAY_TEST_TARGET := $(BUILD_TEST_DIR)/dhcp4relay-test
DHCP6RELAY_TARGET := $(BUILD_DIR)/dhcp6relay
DHCP6RELAY_TEST_TARGET := $(BUILD_TEST_DIR)/dhcp6relay-test
CP := cp
MKDIR := mkdir
MV := mv
FIND := find
GCOVR := gcovr
CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2

#pcap++
LD_PCAPPLUSPLUS_LIB := -lPcap++ -lPacket++ -lCommon++

#pcap plus plus zip file pcappp_v24.09.zip
PCAPPPVAR := 24.09
PCAPPPZIP_FILE := dhcp4/pcappp_v${PCAPPPVAR}.zip
PCAPPLUSPLUS_DIR := dhcp4/PcapPlusPlus-${PCAPPPVAR}
INCLUDE_DIR := $(PCAPPLUSPLUS_DIR)/PcapPlusPlus/header
PCAPPP_DONE = dhcp4/pcappp.stamp

override LDLIBS += -levent -lhiredis -lswsscommon -pthread -lboost_thread -lboost_system $(LD_PCAPPLUSPLUS_LIB) -lpcap
override CPPFLAGS += -Wall -std=c++17 -fPIE -I/usr/include/swss
override CPPFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)"
CPPFLAGS_TEST := -DUNIT_TEST --coverage -fprofile-arcs -ftest-coverage -fprofile-generate -fsanitize=address
LDLIBS_TEST := --coverage -lgtest -lgmock -pthread -lstdc++fs -fsanitize=address

# Default target
all: $(DHCP4RELAY_TARGET) $(DHCP6RELAY_TARGET) $(DHCP4RELAY_TEST_TARGET) $(DHCP6RELAY_TEST_TARGET)

-include dhcp4/src/subdir.mk
-include dhcp4/test/subdir.mk
-include dhcp6/src/subdir.mk
-include dhcp6/test/subdir.mk

$(PCAPPP_DONE):
	unzip ${PCAPPPZIP_FILE} -d dhcp4/
	cd $(PCAPPLUSPLUS_DIR) && cmake -S . -B build && cmake --build build && \
		cd build && make && sudo cmake --install . && \
	        cd ../../../ && touch $@

# Print debug info
debug-paths:
	@echo "DHCP4_SRCS: $(DHCP4_SRCS)"
	@echo "DHCP4_OBJS: $(DHCP4_OBJS)"
	@echo "DHCP6_SRCS: $(DHCP6_SRCS)"
	@echo "DHCP6_OBJS: $(DHCP6_OBJS)"
	@echo "DHCP4_TEST_SRCS: $(DHCP4_TEST_SRCS)"
	@echo "DHCP6_TEST_SRCS: $(DHCP6_TEST_SRCS)"
	@echo "DHCP4_TEST_OBJS: $(DHCP4_TEST_OBJS)"
	@echo "DHCP6_TEST_OBJS: $(DHCP6_TEST_OBJS)"

# Source file locations
DHCP4_SRCS := $(wildcard dhcp4/src/*.cpp)
DHCP6_SRCS := $(wildcard dhcp6/src/*.cpp)
DHCP4_TEST_SRCS := $(wildcard dhcp4/test/*.cpp)
DHCP6_TEST_SRCS := $(wildcard dhcp6/test/*.cpp)

# Object files for binaries
DHCP4_OBJS := $(DHCP4_SRCS:%.cpp=$(BUILD_DIR)/%.o)
DHCP6_OBJS := $(DHCP6_SRCS:%.cpp=$(BUILD_DIR)/%.o)

# Object files for test binaries
DHCP4_TEST_OBJS := $(DHCP4_TEST_SRCS:%.cpp=$(BUILD_TEST_DIR)/%.o)
DHCP6_TEST_OBJS := $(DHCP6_TEST_SRCS:%.cpp=$(BUILD_TEST_DIR)/%.o)

# Exclude main.o from the test build
DHCP4_SRC_TEST_OBJS := $(filter-out %/main.o, $(DHCP4_SRCS:%.cpp=$(BUILD_TEST_DIR)/%.o))
DHCP6_SRC_TEST_OBJS := $(filter-out %/main.o, $(DHCP6_SRCS:%.cpp=$(BUILD_TEST_DIR)/%.o))

# Rules for compiling object files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BUILD_TEST_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(CPPFLAGS_TEST) -c -o $@ $<

# Rules for building binaries
$(DHCP4RELAY_TARGET): $(PCAPPP_DONE) $(DHCP4_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "Building DHCPv4 binaries..."
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(DHCP6RELAY_TARGET): $(DHCP6_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "Building DHCPv6 binaries..."
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Rules for building test binaries
$(DHCP4RELAY_TEST_TARGET): $(DHCP4_TEST_OBJS) $(DHCP4_SRC_TEST_OBJS)
	@mkdir -p $(BUILD_TEST_DIR)
	@echo "Building DHCPv4 test files..."
	$(CXX)  $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_TEST)  -o $@

$(DHCP6RELAY_TEST_TARGET): $(DHCP6_TEST_OBJS) $(DHCP6_SRC_TEST_OBJS)
	@mkdir -p $(BUILD_TEST_DIR)
	@echo "Building DHCPv6 test files..."
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) $(LDLIBS_TEST) -o $@

# Test execution
test: $(DHCP4RELAY_TEST_TARGET) $(DHCP6RELAY_TEST_TARGET)
	# Run dhcp4 tests
	@echo "Running DHCP4 tests..."
	sudo ASAN_OPTIONS=detect_leaks=0 ./$(DHCP4RELAY_TEST_TARGET) --gtest_output=xml:$(DHCP4RELAY_TEST_TARGET)-test-result.xml || true
	@echo "Generating DHCP4 code coverage..."
	gcovr -r ./ --html --html-details -o $(DHCP4RELAY_TEST_TARGET)-code-coverage.html
	gcovr -r ./ --xml-pretty -o $(DHCP4RELAY_TEST_TARGET)-code-coverage.xml

	# Run dhcp6 tests
	@echo "Running DHCP6 tests..."
	sudo ASAN_OPTIONS=detect_leaks=0 ./$(DHCP6RELAY_TEST_TARGET) --gtest_output=xml:$(DHCP6RELAY_TEST_TARGET)-test-result.xml || true
	@echo "Generating DHCP6 code coverage..."
	gcovr -r ./ --html --html-details -o $(DHCP6RELAY_TEST_TARGET)-code-coverage.html
	gcovr -r ./ --xml-pretty -o $(DHCP6RELAY_TEST_TARGET)-code-coverage.xml


install: $(DHCP4RELAY_TARGET) $(DHCP6RELAY_TARGET)
	install -D $(DHCP4RELAY_TARGET) $(DESTDIR)/usr/sbin/$(notdir $(DHCP4RELAY_TARGET))
	install -D $(DHCP6RELAY_TARGET) $(DESTDIR)/usr/sbin/$(notdir $(DHCP6RELAY_TARGET))

uninstall:
	$(RM) $(DESTDIR)/usr/sbin/$(notdir $(DHCP4RELAY_TARGET))
	$(RM) $(DESTDIR)/usr/sbin/$(notdir $(DHCP6RELAY_TARGET))

# Clean up build artifacts
clean:
	@echo "Cleaning up build directories..."
	$(RM) $(PCAPPLUSPLUS_DIR) $(PCAPPP_DONE) $(BUILD_DIR) $(BUILD_TEST_DIR) *.gcda *.gcno *.gcov *.html *.xml

.PHONY: all clean test install uninstall
