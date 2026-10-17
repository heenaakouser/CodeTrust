CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
ROOT     := $(abspath ..)
INCLUDES := -Iinclude -I$(ROOT)/third_party
BUILD    := $(ROOT)/build
LDFLAGS  := -pthread

LIB_SRCS := src/engine.cpp src/sandbox.cpp src/report.cpp src/testcase.cpp src/hints.cpp src/generator.cpp src/language.cpp src/llm.cpp
LIB_OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(LIB_SRCS))

.PHONY: all cli server clean

all: cli server

cli: $(BUILD)/codetrust-cli

server: $(BUILD)/codetrust-server

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/codetrust-cli: $(LIB_OBJS) $(BUILD)/main_cli.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/codetrust-server: $(LIB_OBJS) $(BUILD)/main_server.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/main_cli.o: src/main_cli.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/main_server.o: src/main_server.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(BUILD)/*.o $(BUILD)/codetrust-cli $(BUILD)/codetrust-server
