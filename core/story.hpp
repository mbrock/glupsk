#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace glupsk {

constexpr std::uint32_t kGlulxMagic = 0x476c756c;
constexpr std::uint32_t kSupportedMinVersion = 0x00020000;
constexpr std::uint32_t kSupportedMaxVersion = 0x000301ff;
constexpr std::size_t kHeaderSize = 36;

struct GlulxHeader {
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t ramstart = 0;
    std::uint32_t extstart = 0;
    std::uint32_t endmem = 0;
    std::uint32_t stack_size = 0;
    std::uint32_t start_func = 0;
    std::uint32_t decoding_table = 0;
    std::uint32_t checksum = 0;
};

struct Version {
    std::uint16_t major = 0;
    std::uint8_t minor = 0;
    std::uint8_t patch = 0;
};

class Story {
  public:
    static Story load(const std::filesystem::path& path);
    static Story from_bytes(std::vector<std::uint8_t> bytes);

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    const GlulxHeader& header() const { return header_; }

    Version version() const;
    std::string version_string() const;
    std::uint32_t computed_checksum() const { return computed_checksum_; }
    bool checksum_ok() const { return computed_checksum_ == header_.checksum; }

  private:
    explicit Story(std::vector<std::uint8_t> bytes);

    std::vector<std::uint8_t> bytes_;
    GlulxHeader header_;
    std::uint32_t computed_checksum_ = 0;
};

std::uint32_t compute_glulx_checksum(const std::vector<std::uint8_t>& bytes);

}  // namespace glupsk
