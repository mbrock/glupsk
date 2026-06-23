#include "core/deck.hpp"
#include "core/machine.hpp"
#include "core/types.hpp"

#include "tests/test.hpp"

#include <array>

using namespace glupsk::test;

namespace {

template <typename Ring>
std::vector<glupsk::u32> drain(Ring& ring) {
    auto out = std::vector<glupsk::u32>{};
    while (!ring.empty()) {
        const auto run = ring.contiguous_prefix();
        out.insert(out.end(), run.begin(), run.end());
        ring.consume(run.size());
    }
    return out;
}

}  // namespace

static suite ring_tests{"Ring", [] {
    using glupsk::Ring;
    using glupsk::u32;

    "starts empty"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring{rack};
        expect(ring.capacity() == 4);
        expect(ring.size() == 0);
        expect(ring.empty());
        expect(!ring.full());
        expect(ring.unused() == 4);
        expect(ring.contiguous_prefix().empty());
    };

    "pushes single values until full"_test = [] {
        std::array<u32, 3> rack;
        auto ring = Ring{rack};
        expect(ring.push(10));
        expect(ring.push(20));
        expect(ring.push(30));
        expect(ring.full());
        expect(!ring.push(40)) << "push past capacity must fail";
        expect(ring.size() == 3);
        expect(drain(ring) == std::vector<u32>{10, 20, 30});
    };

    "pushes a span and reports the accepted count"_test = [] {
        std::array<u32, 3> rack;
        auto ring = Ring{rack};
        const auto values = std::array<u32, 5>{1, 2, 3, 4, 5};
        const auto accepted = ring.push(glupsk::span<const u32>{values});
        expect(accepted == 3) << "only three should fit";
        expect(drain(ring) == std::vector<u32>{1, 2, 3});
    };

    "wraps around after partial consume"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring{rack};
        for (auto value : {1u, 2u, 3u, 4u}) {
            ring.push(value);
        }
        ring.consume(2);  // drop 1, 2; head now at index 2
        expect(ring.push(5));  // wraps into index 0
        expect(ring.push(6));  // wraps into index 1
        expect(ring.full());
        // readable() returns only the contiguous tail first.
        const auto first = ring.contiguous_prefix();
        expect(first.size() == 2) << "front run stops at physical end";
        expect(first[0] == 3 && first[1] == 4);
        expect(drain(ring) == std::vector<glupsk::u32>{3, 4, 5, 6});
    };

    "occupancy plus free equals capacity (balance sheet)"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring{rack};
        const auto balanced = [&] {
            return ring.size() + ring.unused() == ring.capacity();
        };
        expect(balanced());
        ring.push(1);
        ring.push(2);
        expect(balanced()) << "after pushes";
        ring.consume(1);
        expect(balanced()) << "after consume";
        ring.push(3);
        ring.push(4);
        ring.push(5);  // wraps; head_/tail_ keep advancing
        expect(balanced()) << "after wrap";
    };

    "clear resets to empty"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring{rack};
        ring.push(1);
        ring.push(2);
        ring.clear();
        expect(ring.empty());
        expect(ring.size() == 0);
        expect(ring.unused() == 4);
    };

    "zero-capacity ring accepts nothing"_test = [] {
        auto ring = Ring<u32>{};
        expect(ring.capacity() == 0);
        expect(ring.full());
        expect(!ring.push(1));
        expect(ring.contiguous_prefix().empty());
        ring.consume(5);  // must be a safe no-op
        expect(ring.empty());
    };

    "rides on caller-owned borrowed storage"_test = [] {
        auto backing = std::array<u32, 4>{};
        auto ring = Ring{backing};
        expect(ring.capacity() == 4);
        expect(ring.push(7));
        expect(ring.push(8));
        expect(backing[0] == 7) << "push writes into the caller's array";
        expect(backing[1] == 8);
        expect(drain(ring) == std::vector<u32>{7, 8});
    };
}};

static suite deck_tests{"Deck", [] {
    using glupsk::Deck;

    "produces and consumes FIFO via back/front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        expect(deck.push_back(1));
        expect(deck.push_back(2));
        expect(deck.push_back(3));
        expect(deck.pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.pop_front() == std::optional<glupsk::u32>{2});
        expect(deck.pop_front() == std::optional<glupsk::u32>{3});
        expect(deck.empty());
        expect(deck.pop_front() == std::nullopt) << "empty front pop is nullopt";
    };

    "behaves as a stack via the back end"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        expect(deck.push_back(1));
        expect(deck.push_back(2));
        expect(deck.push_back(3));
        expect(deck.pop_back() == std::optional<glupsk::u32>{3}) << "LIFO";
        expect(deck.pop_back() == std::optional<glupsk::u32>{2});
        expect(deck.pop_back() == std::optional<glupsk::u32>{1});
        expect(deck.empty());
    };

    "prepends with push_front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        expect(deck.push_back(2));
        expect(deck.push_front(1));  // 1 is now at the front
        expect(deck.pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.pop_front() == std::optional<glupsk::u32>{2});
    };

    "flip swaps the ends (push to deck, pop from flip = stack)"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        expect(deck.push_back(1));
        expect(deck.push_back(2));
        // pop from the flip == pop_back == LIFO
        expect(deck.flip().pop_front() == std::optional<glupsk::u32>{2});
        expect(deck.flip().pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.empty());
    };

    "flip().push_back prepends like push_front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        expect(deck.push_back(2));
        expect(deck.flip().push_back(1));  // == push_front(1)
        expect(deck.pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.pop_front() == std::optional<glupsk::u32>{2});
    };

    "keeps the balance sheet across mixed ends"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        const auto balanced = [&] {
            return deck.size() + deck.unused() == deck.capacity();
        };
        expect(balanced());
        expect(deck.push_back(1));
        expect(deck.push_front(2));
        expect(balanced()) << "after both ends";
        (void) deck.pop_back();
        expect(balanced()) << "after pop";
    };
}};

// static suite memory_slab_tests{"MemorySlab", [] {
//     using glupsk::Deck;
//     using glupsk::MemorySlab;
//     using glupsk::u32;
//     using Vault = MemorySlab<u32>;
//     using MemDeck = Deck<u32, Vault>;

//     const auto make_memory = [](std::size_t size) {
//         auto memory = glupsk::Memory{};
//         memory.bytes = glupsk::Bytes(size, 0);
//         memory.ramstart = 0;
//         memory.endmem = static_cast<u32>(size);
//         return memory;
//     };

//     "writes big-endian codepoints into VM memory"_test = [&] {
//         auto memory = make_memory(64);
//         auto deck = MemDeck{Vault{memory, 0, 4}};
//         deck.push_back(0x11223344u);
//         expect(memory.read8(0) == 0x11) << "stored big-endian";
//         expect(memory.read8(1) == 0x22);
//         expect(memory.read8(2) == 0x33);
//         expect(memory.read8(3) == 0x44);
//     };

//     "round-trips FIFO and LIFO through VM memory"_test = [&] {
//         auto memory = make_memory(64);
//         auto deck = MemDeck{Vault{memory, 8, 4}};
//         deck.push_back(10);
//         deck.push_back(20);
//         expect(deck.pop_front() == std::optional<u32>{10});
//         expect(deck.pop_back() == std::optional<u32>{20});
//         expect(deck.empty());
//     };

//     "survives a memory reallocation"_test = [&] {
//         auto memory = make_memory(64);
//         auto deck = MemDeck{Vault{memory, 0, 4}};
//         deck.push_back(0xdeadbeefu);
//         // Grow memory: this reallocates bytes, which would dangle a cached
//         // span. The proxy re-resolves through Memory, so the value survives.
//         memory.bytes.resize(1 << 16);
//         memory.endmem = static_cast<u32>(memory.bytes.size());
//         expect(deck.pop_front() == std::optional<u32>{0xdeadbeefu});
//     };

//     "a memory slab is deck storage but not contiguous"_test = [] {
//         static_assert(glupsk::deck_storage<Vault, u32>,
//                       "a memory slab can back a deck");
//         static_assert(!glupsk::mutable_slice_of<Vault, u32>,
//                       "but it is not a contiguous slice, so no contiguous_prefix");
//         static_assert(glupsk::mutable_slice_of<std::span<u32>, u32>,
//                       "a span vault keeps contiguous_prefix");
//         expect(true);
//     };
// }};

static suite mark_tests{"RootedIterator", [] {
    using glupsk::range::RootedIterator;
    using glupsk::range::rooted_subrange;

    "is a random access iterator"_test = [] {
        static_assert(
            std::random_access_iterator<RootedIterator<std::vector<int>>>);
        expect(true);
    };

    "rooted_subrange views a slice of the range"_test = [] {
        auto values = std::vector<int>{0, 1, 2, 3, 4, 5};
        auto view = rooted_subrange(values, 2, 3);
        expect(view.size() == 3);
        const auto got = std::vector<int>(view.begin(), view.end());
        expect(got == std::vector<int>{2, 3, 4});
    };

    "writes through the subrange mutate the range"_test = [] {
        auto values = std::vector<int>{0, 1, 2, 3};
        auto view = rooted_subrange(values, 1, 2);
        auto it = view.begin();
        *it = 77;
        *(it + 1) = 88;
        expect(values[1] == 77);
        expect(values[2] == 88);
    };

    "a rooted iterator is stable across reallocation"_test = [] {
        auto values = std::vector<int>{10, 20, 30};
        values.shrink_to_fit();
        const auto* before = values.data();
        auto it = RootedIterator<std::vector<int>>{values, 1};  // -> 20
        expect(*it == 20);
        values.reserve(values.capacity() + 4096);  // force a reallocation
        expect(values.data() != before) << "expected a reallocation";
        expect(*it == 20)
            << "rooted iterator re-resolves begin(), so it survives";
    };

    "rooted_subrange is borrowed, random-access, not contiguous"_test = [] {
        using View = decltype(rooted_subrange(
            std::declval<std::vector<int>&>(), 0, 0));
        static_assert(std::ranges::random_access_range<View>);
        static_assert(std::ranges::borrowed_range<View>);
        static_assert(!std::ranges::contiguous_range<View>);
        expect(true);
    };
}};
