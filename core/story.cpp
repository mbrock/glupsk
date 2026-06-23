#include "core/story.hpp"

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/word.hpp"

#if GLUPSK_ENABLE_FILESYSTEM
#include <fstream>
#endif
#include <string>

namespace glupsk {
namespace {

GlulxHeader parse_header(span<const u8> bytes) {
    auto words = word::big_endian_words<u32>(bytes);
    GlulxHeader header;
    header.magic = words[0];
    header.version = words[1];
    header.ramstart = words[2];
    header.extstart = words[3];
    header.endmem = words[4];
    header.stack_size = words[5];
    header.start_func = words[6];
    header.decoding_table = words[7];
    header.checksum = words[8];
    return header;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

}  // namespace

u32 compute_glulx_checksum(span<const u8> bytes) {
    require(bytes.size() % 4 == 0,
            "Glulx story length must be a multiple of four bytes");

    auto words = word::big_endian_words<u32>(bytes);
    u32 sum = 0;
    for (std::size_t index = 0; index < words.size(); ++index) {
        if (index == 8) {
            continue;
        }
        sum += words[index];
    }
    return sum;
}

#if GLUPSK_ENABLE_FILESYSTEM
Story Story::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail("could not open story file: " + path.string());
    }

    Bytes bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    return Story::from_bytes(std::move(bytes));
}
#endif

Story Story::from_bytes(Bytes bytes) {
    return Story(std::move(bytes));
}

Story::Story(Bytes owned_bytes) : bytes_(std::move(owned_bytes)) {
    require(bytes_.size() >= kHeaderSize,
            "Glulx story is shorter than the 36-byte header");
    require(bytes_.size() % 4 == 0,
            "Glulx story length must be a multiple of four bytes");

    header_ = parse_header(bytes());

    require(header_.magic == kGlulxMagic, "Glulx magic number is not 'Glul'");
    require(header_.version >= kSupportedMinVersion &&
                header_.version <= kSupportedMaxVersion,
            "unsupported Glulx version");
    require(header_.ramstart >= 256, "RAMSTART must be at least 256");
    require(header_.ramstart <= header_.extstart,
            "RAMSTART must not be greater than EXTSTART");
    require(header_.extstart == bytes_.size(),
            "EXTSTART must match the story file length");
    require(header_.endmem >= header_.extstart,
            "ENDMEM must not be less than EXTSTART");

    computed_checksum_ = compute_glulx_checksum(bytes());
}

Version Story::version() const {
    Version version;
    version.major = static_cast<u16>((header_.version >> 16) & 0xffff);
    version.minor = static_cast<u8>((header_.version >> 8) & 0xff);
    version.patch = static_cast<u8>(header_.version & 0xff);
    return version;
}

std::string Story::version_string() const {
    const Version v = version();
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." +
           std::to_string(v.patch);
}

}  // namespace glupsk
