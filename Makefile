CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
CFLAGS ?= -O2 -Wall -Wextra
PYTHON ?= python3
FLEX ?= flex
BISON ?= bison
BUILD := build
GATES ?= 100000
LATCHES ?= 2048
BLIF := blif/dtp_synth.blif
COMMON := $(BUILD)/blif.tab.o $(BUILD)/lex.yy.o $(BUILD)/bench_main.o
.PHONY: all blif bench test reference clean
all: $(BUILD)/blif_bench_strdup $(BUILD)/blif_bench_intern
$(BUILD):
	mkdir -p $@
$(BUILD)/blif.tab.c: src/blif.y | $(BUILD)
	$(BISON) -d -o $@ $<
$(BUILD)/blif.tab.h: $(BUILD)/blif.tab.c
	@test -f $@
$(BUILD)/lex.yy.c: src/blif.l $(BUILD)/blif.tab.h
	$(FLEX) -o $@ $<
$(BUILD)/blif.tab.o: $(BUILD)/blif.tab.c src/strstore.h
	$(CC) $(CFLAGS) -Isrc -c $< -o $@
$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c src/strstore.h
	$(CC) $(CFLAGS) -Isrc -I$(BUILD) -c $< -o $@
$(BUILD)/%.o: src/%.cpp src/strstore.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/blif_bench_strdup: $(COMMON) $(BUILD)/strstore_strdup.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(BUILD)/blif_bench_intern: $(COMMON) $(BUILD)/strstore_intern.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(BUILD)/handwritten_bench: src/handwritten/main.cpp src/handwritten/blif_parse.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<
reference: $(BUILD)/handwritten_bench blif
	./$(BUILD)/handwritten_bench $(BLIF) 7
blif:
	$(PYTHON) scripts/gen_blif.py --gates $(GATES) --latches $(LATCHES) --out $(BLIF)
bench: all blif
	./$(BUILD)/blif_bench_strdup $(BLIF) 7
	./$(BUILD)/blif_bench_intern $(BLIF) 7
$(BUILD)/strstore_test: tests/strstore_test.cpp src/strstore_intern.cpp src/strstore.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ tests/strstore_test.cpp src/strstore_intern.cpp
test: all $(BUILD)/handwritten_bench $(BUILD)/strstore_test
	./$(BUILD)/strstore_test
	$(PYTHON) scripts/verify.py
clean:
	rm -rf $(BUILD)
