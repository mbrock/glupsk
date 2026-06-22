#include "core/story.hpp"

#include "core/bytes.hpp"

#include <format>
#if GLUPSK_ENABLE_FILESYSTEM
#include <fstream>
#endif
#include <stdexcept>

namespace glupsk {
namespace {

GlulxHeader parse_header(span<const u8> bytes) {
    GlulxHeader header;
    header.magic = read_u32_be(bytes, 0);
    header.version = read_u32_be(bytes, 4);
    header.ramstart = read_u32_be(bytes, 8);
    header.extstart = read_u32_be(bytes, 12);
    header.endmem = read_u32_be(bytes, 16);
    header.stack_size = read_u32_be(bytes, 20);
    header.start_func = read_u32_be(bytes, 24);
    header.decoding_table = read_u32_be(bytes, 28);
    header.checksum = read_u32_be(bytes, 32);
    return header;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

u32 compute_glulx_checksum(span<const u8> bytes) {
    require(bytes.size() % 4 == 0,
            "Glulx story length must be a multiple of four bytes");

    u32 sum = 0;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        if (offset == 32) {
            continue;
        }
        sum += read_u32_be(bytes, offset);
    }
    return sum;
}

#if GLUPSK_ENABLE_FILESYSTEM
Story Story::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("could not open story file: " + path.string());
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
    return std::format("{}.{}.{}", v.major, v.minor, v.patch);
}

}  // namespace glupsk
