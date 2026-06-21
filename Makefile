NIX ?= nix
CXX ?= c++
CPPFLAGS ?= -I.
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2 -g

AA_SOURCE := refdata/aa/aa.ni
AA_STORY := refdata/aa.ulx
I7_FLAKE := ./tools/inform7-nix

BUILD_DIR := build
CORE_OBJS := $(BUILD_DIR)/core/story.o
GLUPSK_INFO := $(BUILD_DIR)/glupsk-info
TEST_STORY := $(BUILD_DIR)/test_story

.PHONY: all aa clean clean-aa info test

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

$(TEST_STORY): tests/test_story.cpp $(CORE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

test: $(TEST_STORY)
	$(TEST_STORY)

info: $(GLUPSK_INFO)
	$(GLUPSK_INFO) $(AA_STORY)

clean:
	rm -rf $(BUILD_DIR)

clean-aa:
	rm -f $(AA_STORY)
