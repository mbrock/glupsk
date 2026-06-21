NIX ?= nix
MESON ?= meson
CXX ?= c++
CPPFLAGS ?= -I.
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wpedantic -O2 -g

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
TINY_I7_SOURCE := refdata/tiny-i7/apple.ni
TINY_I7_STORY := refdata/tiny-i7/apple.ulx
I7_FLAKE := ./tools/inform7-nix

BUILD_DIR := build
MESON_BUILD_DIR := $(BUILD_DIR)/meson
CORE_OBJS := $(BUILD_DIR)/core/decode.o $(BUILD_DIR)/core/execute.o $(BUILD_DIR)/core/machine.o $(BUILD_DIR)/core/story.o
GLUPSK_INFO := $(BUILD_DIR)/glupsk-info
GLUPSK_TRACE := $(BUILD_DIR)/glupsk-trace
TEST_STORY := $(BUILD_DIR)/test_story
TEST_STORY_SRCS := tests/test-main.cpp tests/test_decode.cpp tests/test_execute.cpp tests/test_machine.cpp tests/test_story.cpp
TEST_STORY_DEPS := tests/test.hpp

.PHONY: all aa clean clean-aa clean-tiny-i7 compile-commands info meson meson-setup meson-test test tiny-i7 trace

all: $(GLUPSK_INFO)

aa: $(AA_STORY)

tiny-i7: $(TINY_I7_STORY)

$(AA_STORY): $(AA_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(AA_SOURCE) $@

$(TINY_I7_STORY): $(TINY_I7_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(TINY_I7_SOURCE) $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(GLUPSK_INFO): tools/glupsk-info.cpp $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(GLUPSK_TRACE): tools/glupsk-trace.cpp $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(TEST_STORY): $(TEST_STORY_SRCS) $(TEST_STORY_DEPS) $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_STORY_SRCS) $(CORE_OBJS) -o $@

test: $(TEST_STORY)
	$(TEST_STORY)

info: $(GLUPSK_INFO)
	$(GLUPSK_INFO) $(AA_STORY)

trace: $(GLUPSK_TRACE)

meson-setup:
	@if [ -d "$(MESON_BUILD_DIR)" ]; then \
		$(MESON) setup --reconfigure "$(MESON_BUILD_DIR)"; \
	else \
		$(MESON) setup "$(MESON_BUILD_DIR)"; \
	fi

meson: meson-setup
	$(MESON) compile -C "$(MESON_BUILD_DIR)"

meson-test: meson
	$(MESON) test -C "$(MESON_BUILD_DIR)"

compile-commands: meson-setup
	ln -sf "$(MESON_BUILD_DIR)/compile_commands.json" compile_commands.json

clean:
	rm -rf $(BUILD_DIR) compile_commands.json

clean-aa:
	rm -f $(AA_STORY)

clean-tiny-i7:
	rm -f $(TINY_I7_STORY)
