#pragma once

#include "core/glk_registry.hpp"
#include "core/types.hpp"

namespace glupsk {

class GlkArgs {
  public:
    explicit GlkArgs(span<const u32> values) : values_(values) {}

    bool empty() const { return values_.empty(); }
    std::size_t size() const { return values_.size(); }

    u32 get(std::size_t index, u32 fallback = 0) const {
        return index < values_.size() ? values_[index] : fallback;
    }

    GlkWindowHandle window(std::size_t index) const {
        return {.id = get(index)};
    }

    GlkStreamHandle stream(std::size_t index) const {
        return {.id = get(index)};
    }

    GlkGestaltQuery gestalt_query() const {
        return {
            .selector = static_cast<GlkGestaltSelector>(get(0)),
            .value = get(1),
        };
    }

    GlkWindowSpec window_spec() const {
        return {
            .split = {.id = get(0)},
            .method = get(1),
            .size = get(2),
            .type = get(3),
            .rock = get(4),
        };
    }

    GlkMemoryStream memory_stream() const {
        return {
            .address = get(0),
            .len = get(1),
            .mode = get(2),
        };
    }

    GlkUnicodeMemoryStream unicode_memory_stream() const {
        return {
            .address = get(0),
            .len = get(1),
            .mode = get(2),
        };
    }

  private:
    span<const u32> values_;
};

}  // namespace glupsk
