#include "core/bytes.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <stdexcept>


namespace {

glupsk::Bytes minimal_story() {
    glupsk::Bytes bytes(256, 0);
    glupsk::write_u32_be(bytes, 0, glupsk::kGlulxMagic);
    glupsk::write_u32_be(bytes, 4, 0x00030103);
    glupsk::write_u32_be(bytes, 8, 256);
    glupsk::write_u32_be(bytes, 12, static_cast<glupsk::u32>(bytes.size()));
    glupsk::write_u32_be(bytes, 16, static_cast<glupsk::u32>(bytes.size()));
    glupsk::write_u32_be(bytes, 20, 64 * 1024);
    glupsk::write_u32_be(bytes, 24, 0x40);
    glupsk::write_u32_be(bytes, 28, 0);
    glupsk::write_u32_be(bytes, 32, glupsk::compute_glulx_checksum(bytes));
    return bytes;
}

template <typename Fn>
bool throws_runtime_error(Fn fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

using namespace glupsk::test;

static suite story_tests{"Story", [] {
    "parses a minimal synthetic story"_test = [] {
        const auto story = glupsk::Story::from_bytes(minimal_story());
        expect(story.header().magic == glupsk::kGlulxMagic);
        expect(story.version_string() == "3.1.3");
        expect(story.header().ramstart == 256);
        expect(story.header().extstart == story.bytes().size());
        expect(story.checksum_ok());
    };

    "rejects malformed story bytes"_test = [] {
        expect(throws_runtime_error(
            [] { glupsk::Story::from_bytes(glupsk::Bytes(35)); }))
            << "short headers should be rejected";

        auto bad_magic = minimal_story();
        bad_magic[0] = 0;
        expect(throws_runtime_error(
            [&] { glupsk::Story::from_bytes(bad_magic); }))
            << "bad magic should be rejected";

        auto bad_length = minimal_story();
        bad_length.push_back(0);
        expect(throws_runtime_error(
            [&] { glupsk::Story::from_bytes(bad_length); }))
            << "unaligned lengths should be rejected";
    };

    "parses the committed AA story"_test = [] {
        const auto story = glupsk::Story::load("refdata/aa.ulx");
        expect(story.version_string() == "3.1.2");
        expect(story.bytes().size() == 895744);
        expect(story.header().ramstart == 689408);
        expect(story.header().extstart == 895744);
        expect(story.header().endmem == 895744);
        expect(story.header().stack_size == 65536);
        expect(story.header().start_func == 0x3c);
        expect(story.header().decoding_table == 0x6a4ec);
        expect(story.header().checksum == 0x2fa1eb34);
        expect(story.computed_checksum() == 0x2fa1eb34);
        expect(story.checksum_ok());
    };
}};
