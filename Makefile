NIX ?= nix
MESON ?= meson
CXX ?= c++
CPPFLAGS ?= -I.
CXXFLAGS ?= -std=c++23 -Wall -Wextra -Wpedantic -O2 -g

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
I7_FLAKE := ./tools/inform7-nix

BUILD_DIR := build
MESON_BUILD_DIR := $(BUILD_DIR)/meson
CORE_OBJS := $(BUILD_DIR)/core/story.o
GLUPSK_INFO := $(BUILD_DIR)/glupsk-info
TEST_STORY := $(BUILD_DIR)/test_story
TEST_STORY_SRCS := tests/test-main.cpp tests/test_story.cpp
TEST_STORY_DEPS := tests/test.hpp

.PHONY: all aa clean clean-aa compile-commands info meson meson-setup meson-test test

all: $(GLUPSK_INFO)

aa: $(AA_STORY)

$(AA_STORY): $(AA_SOURCE) tools/inform7-nix/flake.nix tools/inform7-nix/flake.lock
	$(NIX) run $(I7_FLAKE)#i7-build -- --glulx $(AA_SOURCE) $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(GLUPSK_INFO): tools/glupsk-info.cpp $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(TEST_STORY): $(TEST_STORY_SRCS) $(TEST_STORY_DEPS) $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_STORY_SRCS) $(CORE_OBJS) -o $@

test: $(TEST_STORY)
	$(TEST_STORY)

info: $(GLUPSK_INFO)
	$(GLUPSK_INFO) $(AA_STORY)

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
