#include "core/glk_names.hpp"

namespace glupsk {

std::string_view glulx_gestalt_name(u32 selector) {
    switch (selector) {
        case 0: return "GlulxVersion";
        case 1: return "TerpVersion";
        case 2: return "ResizeMem";
        case 3: return "Undo";
        case 4: return "IOSystem";
        case 5: return "Unicode";
        case 6: return "MemCopy";
        case 7: return "MAlloc";
        case 8: return "MAllocHeap";
        case 9: return "Acceleration";
        case 10: return "AccelFunc";
        case 11: return "Float";
        case 12: return "ExtUndo";
        case 13: return "Double";
        default: return "unknown";
    }
}

std::string_view glk_selector_name(u32 selector) {
    switch (selector) {
        case 0x0001: return "exit";
        case 0x0004: return "gestalt";
        case 0x0020: return "window_iterate";
        case 0x0021: return "window_get_rock";
        case 0x0022: return "window_get_root";
        case 0x0023: return "window_open";
        case 0x0025: return "window_get_size";
        case 0x002a: return "window_clear";
        case 0x002b: return "window_move_cursor";
        case 0x002c: return "window_get_stream";
        case 0x002f: return "set_window";
        case 0x0040: return "stream_iterate";
        case 0x0041: return "stream_get_rock";
        case 0x0043: return "stream_open_memory";
        case 0x0044: return "stream_close";
        case 0x0047: return "stream_set_current";
        case 0x0048: return "stream_get_current";
        case 0x0064: return "fileref_iterate";
        case 0x0065: return "fileref_get_rock";
        case 0x0080: return "put_char";
        case 0x0081: return "put_char_stream";
        case 0x0082: return "put_string";
        case 0x0083: return "put_string_stream";
        case 0x0084: return "put_buffer";
        case 0x0085: return "put_buffer_stream";
        case 0x0086: return "set_style";
        case 0x00a0: return "char_to_lower";
        case 0x00a1: return "char_to_upper";
        case 0x00b0: return "stylehint_set";
        case 0x00c0: return "select";
        case 0x00d0: return "request_line_event";
        case 0x0120: return "buffer_to_lower_case_uni";
        case 0x0121: return "buffer_to_upper_case_uni";
        case 0x0122: return "buffer_to_title_case_uni";
        case 0x0123: return "buffer_canon_decompose_uni";
        case 0x0124: return "buffer_canon_normalize_uni";
        case 0x0128: return "put_char_uni";
        case 0x0129: return "put_string_uni";
        case 0x012a: return "put_buffer_uni";
        case 0x012b: return "put_char_stream_uni";
        case 0x012c: return "put_string_stream_uni";
        case 0x012d: return "put_buffer_stream_uni";
        case 0x0139: return "stream_open_memory_uni";
        case 0x0141: return "request_line_event_uni";
        default: return "unknown";
    }
}

std::string_view glk_gestalt_name(u32 selector) {
    switch (selector) {
        case 0: return "Version";
        case 1: return "CharInput";
        case 2: return "LineInput";
        case 3: return "CharOutput";
        case 4: return "MouseInput";
        case 5: return "Timer";
        case 6: return "Graphics";
        case 7: return "DrawImage";
        case 8: return "Sound";
        case 9: return "SoundVolume";
        case 10: return "SoundNotify";
        case 11: return "Hyperlinks";
        case 12: return "HyperlinkInput";
        case 15: return "Unicode";
        case 16: return "UnicodeNorm";
        default: return "unknown";
    }
}

}  // namespace glupsk
