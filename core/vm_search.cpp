#include "core/vm_search.hpp"

#include "core/error.hpp"

namespace glupsk {
namespace {

bool is_zero_key(const Machine& machine, u32 address, u32 key_size) {
    for (u32 index = 0; index < key_size; ++index) {
        if (machine.memory.read8(address + index) != 0) {
            return false;
        }
    }
    return true;
}

u8 key_byte(const Machine& machine,
            u32 key,
            u32 key_size,
            bool key_indirect,
            u32 index) {
    if (key_indirect) {
        return machine.memory.read8(key + index);
    }
    if (key_size != 1 && key_size != 2 && key_size != 4) {
        fail("direct search key size must be 1, 2, or 4");
    }
    return static_cast<u8>((key >> ((key_size - index - 1) * 8)) & 0xff);
}

int compare_key_at(Machine& machine,
                   u32 key,
                   u32 key_size,
                   u32 address,
                   bool key_indirect) {
    for (u32 index = 0; index < key_size; ++index) {
        const auto want = key_byte(machine, key, key_size, key_indirect, index);
        const auto got = machine.memory.read8(address + index);
        if (want < got) {
            return -1;
        }
        if (want > got) {
            return 1;
        }
    }
    return 0;
}

u32 failed_search(bool return_index) {
    return return_index ? 0xffffffffu : 0;
}

u32 successful_search(u32 start, u32 struct_size, u32 index, bool return_index) {
    return return_index ? index : start + index * struct_size;
}

}  // namespace

u32 linear_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options) {
    if ((options & ~0x07u) != 0) {
        fail("linearsearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto zero_key_terminates = (options & 0x02) != 0;
    const auto return_index = (options & 0x04) != 0;

    for (u32 index = 0; num_structs == 0xffffffffu || index < num_structs;
         ++index) {
        const auto entry_key = start + index * struct_size + key_offset;
        const auto cmp = compare_key_at(machine, key, key_size, entry_key,
                                        key_indirect);
        if (cmp == 0) {
            return successful_search(start, struct_size, index, return_index);
        }
        if (zero_key_terminates && is_zero_key(machine, entry_key, key_size)) {
            return failed_search(return_index);
        }
    }
    return failed_search(return_index);
}

u32 binary_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options) {
    if ((options & ~0x05u) != 0) {
        fail("binarysearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto return_index = (options & 0x04) != 0;

    auto low = u32{0};
    auto high = num_structs;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto cmp = compare_key_at(machine, key, key_size,
                                        start + mid * struct_size + key_offset,
                                        key_indirect);
        if (cmp == 0) {
            return successful_search(start, struct_size, mid, return_index);
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return failed_search(return_index);
}

u32 linked_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 key_offset,
                  u32 next_offset,
                  u32 options) {
    if ((options & ~0x03u) != 0) {
        fail("linkedsearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto zero_key_terminates = (options & 0x02) != 0;

    auto entry = start;
    while (entry != 0) {
        const auto entry_key = entry + key_offset;
        const auto cmp = compare_key_at(machine, key, key_size, entry_key,
                                        key_indirect);
        if (cmp == 0) {
            return entry;
        }
        if (zero_key_terminates && is_zero_key(machine, entry_key, key_size)) {
            return 0;
        }
        entry = machine.memory.read32(entry + next_offset);
    }
    return 0;
}

}  // namespace glupsk
