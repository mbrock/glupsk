#include "core/word.hpp"

#include "tests/test.hpp"

#include <algorithm>
#include <array>
#include <ranges>

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
