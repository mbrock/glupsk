#include "core/story.hpp"

#include <print>

namespace {

void print_field(const char* name, glupsk::u32 value) {
    std::println("{:<16}{} (0x{:08x})", name, value, value);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::println(stderr, "usage: glupsk-info STORY.ulx");
        return 64;
    }

    try {
        const auto story = glupsk::Story::load(argv[1]);
        const auto& header = story.header();

        std::println("file: {}", argv[1]);
        std::println("size: {} bytes", story.bytes().size());
        std::println("magic: Glul (0x{:08x})", header.magic);
        std::println("version: {} (0x{:08x})", story.version_string(),
                     header.version);

        print_field("ramstart:", header.ramstart);
        print_field("extstart:", header.extstart);
        print_field("endmem:", header.endmem);
        print_field("stack size:", header.stack_size);
        print_field("start func:", header.start_func);
        print_field("decode table:", header.decoding_table);

        std::println("checksum: {} (stored 0x{:08x}, computed 0x{:08x})",
                     story.checksum_ok() ? "ok" : "bad", header.checksum,
                     story.computed_checksum());
    } catch (const std::exception& ex) {
        std::println(stderr, "glupsk-info: {}", ex.what());
        return 1;
    }

    return 0;
}
