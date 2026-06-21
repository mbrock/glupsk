#include "core/bytes.hpp"
#include "core/story.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

glupsk::Bytes minimal_story() {
    glupsk::Bytes bytes(256, 0);
    glupsk::write_u32_be(bytes, 0, glupsk::kGlulxMagic);
    glupsk::write_u32_be(bytes, 4, 0x00030103);
    glupsk::write_u32_be(bytes, 8, 256);
    glupsk::write_u32_be(bytes, 12, static_cast<std::uint32_t>(bytes.size()));
    glupsk::write_u32_be(bytes, 16, static_cast<std::uint32_t>(bytes.size()));
    glupsk::write_u32_be(bytes, 20, 64 * 1024);
    glupsk::write_u32_be(bytes, 24, 0x40);
    glupsk::write_u32_be(bytes, 28, 0);
    glupsk::write_u32_be(bytes, 32, glupsk::compute_glulx_checksum(bytes));
    return bytes;
}

template <typename Fn>
void expect_throw(Fn fn, const std::string& label) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return;
    }
    std::cerr << "expected runtime_error: " << label << '\n';
    std::abort();
}

void test_minimal_story() {
    const auto story = glupsk::Story::from_bytes(minimal_story());
    assert(story.header().magic == glupsk::kGlulxMagic);
    assert(story.version_string() == "3.1.3");
    assert(story.header().ramstart == 256);
    assert(story.header().extstart == story.bytes().size());
    assert(story.checksum_ok());
}

void test_rejects_bad_inputs() {
    expect_throw([] { glupsk::Story::from_bytes(glupsk::Bytes(35)); },
                 "short header");

    auto bad_magic = minimal_story();
    bad_magic[0] = 0;
    expect_throw([&] { glupsk::Story::from_bytes(bad_magic); }, "bad magic");

    auto bad_length = minimal_story();
    bad_length.push_back(0);
    expect_throw([&] { glupsk::Story::from_bytes(bad_length); },
                 "unaligned length");
}

void test_aa_story() {
    const auto story = glupsk::Story::load("refdata/aa.ulx");
    assert(story.version_string() == "3.1.2");
    assert(story.bytes().size() == 874240);
    assert(story.header().ramstart == 669952);
    assert(story.header().extstart == 874240);
    assert(story.header().endmem == 874240);
    assert(story.header().stack_size == 65536);
    assert(story.header().start_func == 0x3c);
    assert(story.header().decoding_table == 0x690b1);
    assert(story.header().checksum == 0xbb6436aa);
    assert(story.computed_checksum() == 0xbb6436aa);
    assert(story.checksum_ok());
}

}  // namespace

int main() {
    test_minimal_story();
    test_rejects_bad_inputs();
    test_aa_story();
    std::cout << "test_story: ok\n";
    return 0;
}
