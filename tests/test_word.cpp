#include "core/word.hpp"

#include "tests/test.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <ranges>
#include <vector>

using namespace glupsk::test;

static suite word_tests{"word", [] {
    using glupsk::u16;
    using glupsk::u32;
    using glupsk::u8;

    "loads and stores unsigned big-endian words"_test = [] {
        auto bytes = std::array<u8, 8>{};

        glupsk::word::store_be<u32>(bytes, 1, 0x12345678u);
        expect(bytes[1] == 0x12);
        expect(bytes[2] == 0x34);
        expect(bytes[3] == 0x56);
        expect(bytes[4] == 0x78);

        expect(glupsk::word::load_be<u32>(bytes, 1) == 0x12345678u);
        expect(glupsk::word::load_be<u16>(bytes, 2) == 0x3456u);
    };

    "loads and stores unsigned little-endian words"_test = [] {
        auto bytes = std::array<u8, 8>{};

        glupsk::word::store_le<u32>(bytes, 1, 0x12345678u);
        expect(bytes[1] == 0x78);
        expect(bytes[2] == 0x56);
        expect(bytes[3] == 0x34);
        expect(bytes[4] == 0x12);

        expect(glupsk::word::load_le<u32>(bytes, 1) == 0x12345678u);
        expect(glupsk::word::load_le<u16>(bytes, 2) == 0x3456u);
    };

    "views bytes as mutable big-endian words"_test = [] {
        auto bytes = std::array<u8, 8>{};
        auto words = glupsk::word::big_endian_words<u32>(bytes);

        expect(words.size() == 2);
        words[0] = 0x11223344u;
        words[1] = 0xaabbccddu;

        expect(bytes == std::array<u8, 8>{
                            0x11, 0x22, 0x33, 0x44,
                            0xaa, 0xbb, 0xcc, 0xdd,
                        });
        expect(static_cast<u32>(words[0]) == 0x11223344u);
        expect(static_cast<u32>(words[1]) == 0xaabbccddu);
    };

    "views bytes as mutable little-endian words"_test = [] {
        auto bytes = std::array<u8, 8>{};
        auto words = glupsk::word::little_endian_words<u32>(bytes);

        words[0] = 0x11223344u;
        words[1] = 0xaabbccddu;

        expect(bytes == std::array<u8, 8>{
                            0x44, 0x33, 0x22, 0x11,
                            0xdd, 0xcc, 0xbb, 0xaa,
                        });
        expect(static_cast<u32>(words[0]) == 0x11223344u);
        expect(static_cast<u32>(words[1]) == 0xaabbccddu);
    };

    "uses std endian order for generic word views"_test = [] {
        auto bytes = std::array<u8, 4>{};
        auto words =
            glupsk::word::endian_words<u32, std::endian::little>(bytes);

        words[0] = 0x01020304u;
        expect(bytes == std::array<u8, 4>{4, 3, 2, 1});
        expect(static_cast<u32>(words[0]) == 0x01020304u);
    };

    "supports unaligned word views"_test = [] {
        auto bytes = std::array<u8, 10>{};
        auto words = glupsk::word::big_endian_words<u32>(bytes, 1, 2);

        words[0] = 0x01020304u;
        words[1] = 0x05060708u;

        expect(bytes[0] == 0);
        expect(bytes[1] == 1);
        expect(bytes[4] == 4);
        expect(bytes[5] == 5);
        expect(bytes[8] == 8);
        expect(bytes[9] == 0);
    };

    "borrows vector and span byte storage"_test = [] {
        auto bytes = std::vector<u8>(8);
        auto vector_words = glupsk::word::big_endian_words<u32>(bytes);

        vector_words[0] = 0x10203040u;
        expect(bytes[0] == 0x10);
        expect(bytes[3] == 0x40);

        auto byte_span = std::span<u8>{bytes};
        auto span_words = glupsk::word::big_endian_words<u16>(byte_span, 4, 2);
        span_words[0] = 0x5566u;
        span_words[1] = 0x7788u;

        expect(bytes[4] == 0x55);
        expect(bytes[5] == 0x66);
        expect(bytes[6] == 0x77);
        expect(bytes[7] == 0x88);
    };

    "has random-access proxy iterators"_test = [] {
        using Words = glupsk::word::BigEndianWords<u32>;
        static_assert(std::ranges::random_access_range<Words>);
        static_assert(std::random_access_iterator<std::ranges::iterator_t<Words>>);

        auto bytes = std::array<u8, 12>{};
        auto words = glupsk::word::big_endian_words<u32>(bytes);

        auto it = words.begin();
        *it = 1;
        *(it + 1) = 2;
        it[2] = 3;

        auto values = std::array<u32, 3>{};
        std::ranges::copy(words, values.begin());
        expect(values == std::array<u32, 3>{1, 2, 3});
    };
}};
