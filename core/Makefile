CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Iinclude -Isrc
LDFLAGS ?=

LIB_SRC := $(wildcard src/*.cpp)
LIB_OBJ := $(patsubst src/%.cpp,build/obj/%.o,$(LIB_SRC))

.PHONY: all test clean

all: build/ninegrid_cli build/ninegrid_tests

build/obj/%.o: src/%.cpp | build/obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/obj/cli_main.o: cli/main.cpp | build/obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/obj/test_main.o: tests/test_core.cpp | build/obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/ninegrid_cli: build/obj/cli_main.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

build/ninegrid_tests: build/obj/test_main.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test: build/ninegrid_tests
	./build/ninegrid_tests data/sample_dict.tsv

build/obj:
	mkdir -p $@

clean:
	rm -rf build
