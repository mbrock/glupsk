#include "core/ring.hpp"
#include "core/types.hpp"

#include "tests/test.hpp"

#include <array>

using namespace glupsk::test;

namespace {

std::vector<glupsk::u32> drain(glupsk::Ring<glupsk::u32>& ring) {
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
    "starts empty"_test = [] {
        auto ring = glupsk::Ring<glupsk::u32>{4};
        expect(ring.capacity() == 4);
        expect(ring.size() == 0);
        expect(ring.empty());
        expect(!ring.full());
        expect(ring.unused() == 4);
        expect(ring.readable().empty());
    };

    "pushes single values until full"_test = [] {
        auto ring = glupsk::Ring<glupsk::u32>{3};
        expect(ring.push(10));
        expect(ring.push(20));
        expect(ring.push(30));
        expect(ring.full());
        expect(!ring.push(40)) << "push past capacity must fail";
        expect(ring.size() == 3);
        expect(drain(ring) == std::vector<glupsk::u32>{10, 20, 30});
    };

    "pushes a span and reports the accepted count"_test = [] {
        auto ring = glupsk::Ring<glupsk::u32>{3};
        const auto values = std::array<glupsk::u32, 5>{1, 2, 3, 4, 5};
        const auto accepted = ring.push(glupsk::span<const glupsk::u32>{values});
        expect(accepted == 3) << "only three should fit";
        expect(drain(ring) == std::vector<glupsk::u32>{1, 2, 3});
    };

    "wraps around after partial consume"_test = [] {
        auto ring = glupsk::Ring<glupsk::u32>{4};
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
        auto ring = glupsk::Ring<glupsk::u32>{4};
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
        auto ring = glupsk::Ring<glupsk::u32>{4};
        ring.push(1);
        ring.push(2);
        ring.clear();
        expect(ring.empty());
        expect(ring.size() == 0);
        expect(ring.unused() == 4);
    };

    "zero-capacity ring accepts nothing"_test = [] {
        auto ring = glupsk::Ring<glupsk::u32>{0};
        expect(ring.capacity() == 0);
        expect(ring.full());
        expect(!ring.push(1));
        expect(ring.readable().empty());
        ring.consume(5);  // must be a safe no-op
        expect(ring.empty());
    };
}};
