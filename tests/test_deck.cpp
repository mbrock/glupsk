#include "core/deck.hpp"
#include "core/types.hpp"

#include "tests/test.hpp"

#include <array>

using namespace glupsk::test;

namespace {

template <typename Ring>
std::vector<glupsk::u32> drain(Ring& ring) {
    auto out = std::vector<glupsk::u32>{};
    while (!ring.empty()) {
        const auto run = ring.readable();
        out.insert(out.end(), run.begin(), run.end());
        ring.consume(run.size());
    }
    return out;
}

}  // namespace

static suite ring_tests{"Ring", [] {
    using glupsk::Ring;
    using glupsk::BorrowedStorage;
    using glupsk::u32;

    "starts empty"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring<u32>(rack);
        expect(ring.capacity() == 4);
        expect(ring.size() == 0);
        expect(ring.empty());
        expect(!ring.full());
        expect(ring.unused() == 4);
        expect(ring.readable().empty());
    };

    "pushes single values until full"_test = [] {
        std::array<u32, 3> rack;
        auto ring = Ring<u32>{rack};
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
        auto ring = Ring<u32>{rack};
        const auto values = std::array<u32, 5>{1, 2, 3, 4, 5};
        const auto accepted = ring.push(glupsk::span<const u32>{values});
        expect(accepted == 3) << "only three should fit";
        expect(drain(ring) == std::vector<u32>{1, 2, 3});
    };

    "wraps around after partial consume"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring<u32>{rack};
        for (auto value : {1u, 2u, 3u, 4u}) {
            ring.push(value);
        }
        ring.consume(2);  // drop 1, 2; head now at index 2
        expect(ring.push(5));  // wraps into index 0
        expect(ring.push(6));  // wraps into index 1
        expect(ring.full());
        // readable() returns only the contiguous tail first.
        const auto first = ring.readable();
        expect(first.size() == 2) << "front run stops at physical end";
        expect(first[0] == 3 && first[1] == 4);
        expect(drain(ring) == std::vector<glupsk::u32>{3, 4, 5, 6});
    };

    "occupancy plus free equals capacity (balance sheet)"_test = [] {
        std::array<u32, 4> rack;
        auto ring = Ring<u32>{rack};
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
        auto ring = Ring<u32>{rack};
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
        expect(ring.readable().empty());
        ring.consume(5);  // must be a safe no-op
        expect(ring.empty());
    };

    "rides on caller-owned borrowed storage"_test = [] {
        auto backing = std::array<u32, 4>{};
        using Storage = glupsk::BorrowedStorage<u32>;
        auto ring =
            Ring<u32, Storage>{Storage{backing.data(), backing.size()}};
        expect(ring.capacity() == 4);
        expect(ring.push(7));
        expect(ring.push(8));
        expect(backing[0] == 7) << "push writes into the caller's array";
        expect(backing[1] == 8);
        expect(drain(ring) == std::vector<u32>{7, 8});
    };
}};

static suite deck_tests{"Deck", [] {
    using Deck = glupsk::Deck<glupsk::u32>;

    "produces and consumes FIFO via back/front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        deck.push_back(1);
        deck.push_back(2);
        deck.push_back(3);
        expect(deck.pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.pop_front() == std::optional<glupsk::u32>{2});
        expect(deck.pop_front() == std::optional<glupsk::u32>{3});
        expect(deck.empty());
        expect(deck.pop_front() == std::nullopt) << "empty front pop is nullopt";
    };

    "behaves as a stack via the back end"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        deck.push_back(1);
        deck.push_back(2);
        deck.push_back(3);
        expect(deck.pop_back() == std::optional<glupsk::u32>{3}) << "LIFO";
        expect(deck.pop_back() == std::optional<glupsk::u32>{2});
        expect(deck.pop_back() == std::optional<glupsk::u32>{1});
        expect(deck.empty());
    };

    "prepends with push_front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        deck.push_back(2);
        deck.push_front(1);  // 1 is now at the front
        expect(deck.pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.pop_front() == std::optional<glupsk::u32>{2});
    };

    "flip swaps the ends (push to deck, pop from flip = stack)"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        deck.push_back(1);
        deck.push_back(2);
        // pop from the flip == pop_back == LIFO
        expect(deck.flip().pop_front() == std::optional<glupsk::u32>{2});
        expect(deck.flip().pop_front() == std::optional<glupsk::u32>{1});
        expect(deck.empty());
    };

    "flip().push_back prepends like push_front"_test = [] {
        std::array<glupsk::u32, 4> rack;
        auto deck = Deck{rack};
        deck.push_back(2);
        deck.flip().push_back(1);  // == push_front(1)
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
        deck.push_back(1);
        deck.push_front(2);
        expect(balanced()) << "after both ends";
        (void) deck.pop_back();
        expect(balanced()) << "after pop";
    };
}};
