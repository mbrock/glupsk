#include "core/story.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::ostream& hex32(std::ostream& out, std::uint32_t value) {
    const auto flags = out.flags();
    const auto fill = out.fill();
    out << "0x" << std::hex << std::setfill('0') << std::setw(8) << value;
    out.flags(flags);
    out.fill(fill);
    return out;
}

void print_field(const char* name, std::uint32_t value) {
    std::cout << std::left << std::setw(16) << name << std::right << value
              << " (";
    hex32(std::cout, value) << ")\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: glupsk-info STORY.ulx\n";
        return 64;
    }

    try {
        const auto story = glupsk::Story::load(argv[1]);
        const auto& header = story.header();

        std::cout << "file: " << argv[1] << '\n';
        std::cout << "size: " << story.bytes().size() << " bytes\n";
        std::cout << "magic: Glul (";
        hex32(std::cout, header.magic) << ")\n";
        std::cout << "version: " << story.version_string() << " (";
        hex32(std::cout, header.version) << ")\n";

        print_field("ramstart:", header.ramstart);
        print_field("extstart:", header.extstart);
        print_field("endmem:", header.endmem);
        print_field("stack size:", header.stack_size);
        print_field("start func:", header.start_func);
        print_field("decode table:", header.decoding_table);

        std::cout << "checksum: " << (story.checksum_ok() ? "ok" : "bad")
                  << " (stored ";
        hex32(std::cout, header.checksum) << ", computed ";
        hex32(std::cout, story.computed_checksum()) << ")\n";
    } catch (const std::exception& ex) {
        std::cerr << "glupsk-info: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
