#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace glupsk {

constexpr u32 kGlulxMagic = 0x476c756c;
constexpr u32 kSupportedMinVersion = 0x00020000;
constexpr u32 kSupportedMaxVersion = 0x000301ff;
constexpr std::size_t kHeaderSize = 36;

struct GlulxHeader {
    u32 magic = 0;
    u32 version = 0;
    u32 ramstart = 0;
    u32 extstart = 0;
    u32 endmem = 0;
    u32 stack_size = 0;
    u32 start_func = 0;
    u32 decoding_table = 0;
    u32 checksum = 0;
};

struct Version {
    u16 major = 0;
    u8 minor = 0;
    u8 patch = 0;
};

class Story {
  public:
    static Story load(const std::filesystem::path& path);
    static Story from_bytes(Bytes bytes);

    span<const u8> bytes() const { return bytes_; }
    const GlulxHeader& header() const { return header_; }

    Version version() const;
    std::string version_string() const;
    u32 computed_checksum() const { return computed_checksum_; }
    bool checksum_ok() const { return computed_checksum_ == header_.checksum; }

  private:
    explicit Story(Bytes bytes);

    Bytes bytes_;
    GlulxHeader header_;
    u32 computed_checksum_ = 0;
};

u32 compute_glulx_checksum(span<const u8> bytes);

}  // namespace glupsk
