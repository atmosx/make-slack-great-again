#include "linux/xkb_keyboard.h"

#include "core/input.h"

#include <cstdlib>

namespace plat::linux_input {

namespace {

struct EvdevKey {
    uint16_t code;
    Key      key;
};
// Linux evdev ABI numbers (also used by Wayland), independent of host headers.
// US-position table: the fallback for layouts with no Latin group, and the
// inverse map test injection uses.
constexpr EvdevKey kEvdev[] = {
    {30, Key::A},
    {48, Key::B},
    {46, Key::C},
    {32, Key::D},
    {18, Key::E},
    {33, Key::F},
    {34, Key::G},
    {35, Key::H},
    {23, Key::I},
    {36, Key::J},
    {37, Key::K},
    {38, Key::L},
    {50, Key::M},
    {49, Key::N},
    {24, Key::O},
    {25, Key::P},
    {16, Key::Q},
    {19, Key::R},
    {31, Key::S},
    {20, Key::T},
    {22, Key::U},
    {47, Key::V},
    {17, Key::W},
    {45, Key::X},
    {21, Key::Y},
    {44, Key::Z},
    {11, Key::Num0},
    {2, Key::Num1},
    {3, Key::Num2},
    {4, Key::Num3},
    {5, Key::Num4},
    {6, Key::Num5},
    {7, Key::Num6},
    {8, Key::Num7},
    {9, Key::Num8},
    {10, Key::Num9},
    {59, Key::F1},
    {60, Key::F2},
    {61, Key::F3},
    {62, Key::F4},
    {63, Key::F5},
    {64, Key::F6},
    {65, Key::F7},
    {66, Key::F8},
    {67, Key::F9},
    {68, Key::F10},
    {87, Key::F11},
    {88, Key::F12},
    {183, Key::F13},
    {184, Key::F14},
    {185, Key::F15},
    {186, Key::F16},
    {187, Key::F17},
    {188, Key::F18},
    {189, Key::F19},
    {190, Key::F20},
    {191, Key::F21},
    {192, Key::F22},
    {193, Key::F23},
    {194, Key::F24},
    {1, Key::Escape},
    {28, Key::Enter},
    {15, Key::Tab},
    {14, Key::Backspace},
    {111, Key::Delete},
    {110, Key::Insert},
    {102, Key::Home},
    {107, Key::End},
    {104, Key::PageUp},
    {109, Key::PageDown},
    {105, Key::Left},
    {106, Key::Right},
    {103, Key::Up},
    {108, Key::Down},
    {57, Key::Space},
    {12, Key::Minus},
    {13, Key::Equal},
    {26, Key::BracketLeft},
    {27, Key::BracketRight},
    {43, Key::Backslash},
    {39, Key::Semicolon},
    {40, Key::Apostrophe},
    {41, Key::Grave},
    {51, Key::Comma},
    {52, Key::Period},
    {53, Key::Slash},
    {58, Key::CapsLock},
    {42, Key::ShiftLeft},
    {54, Key::ShiftRight},
    {29, Key::ControlLeft},
    {97, Key::ControlRight},
    {56, Key::AltLeft},
    {100, Key::AltRight},
    {125, Key::SuperLeft},
    {126, Key::SuperRight},
    {127, Key::Menu},
    {99, Key::PrintScreen},
    {70, Key::ScrollLock},
    {119, Key::Pause},
    {69, Key::NumLock},
    {82, Key::Kp0},
    {79, Key::Kp1},
    {80, Key::Kp2},
    {81, Key::Kp3},
    {75, Key::Kp4},
    {76, Key::Kp5},
    {77, Key::Kp6},
    {71, Key::Kp7},
    {72, Key::Kp8},
    {73, Key::Kp9},
    {83, Key::KpDecimal},
    {98, Key::KpDivide},
    {55, Key::KpMultiply},
    {74, Key::KpSubtract},
    {78, Key::KpAdd},
    {96, Key::KpEnter},
    {117, Key::KpEqual},
    {158, Key::Back},
    {159, Key::Forward},
};

bool isKeypad(Key k) {
    return k >= Key::Kp0 && k <= Key::KpEqual;
}
bool isPositional(Key k) {
    // Keys whose identity is "what the layout types" — only these consult the
    // keysym; everything else (F-keys, arrows, modifiers, keypad) is physical.
    return (k >= Key::A && k <= Key::Z) || (k >= Key::Num0 && k <= Key::Num9) ||
           (k >= Key::Minus && k <= Key::Slash);
}

const char *localeForCompose() {
    for (const char *v : {"LC_ALL", "LC_CTYPE", "LANG"})
        if (const char *s = std::getenv(v); s && *s)
            return s;
    return "C";
}

#if !defined(__linux__)
// BSD X11 servers (e.g. OpenBSD wscons) may use xfree86 keycodes rather than
// evdev for arrows, navigation and function keys; resolve them via the XKB
// keymap's level-0 keysym first.
Key nonPositionalFromKeysym(xkb_keysym_t sym) {
    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F24)
        return Key(int(Key::F1) + int(sym - XKB_KEY_F1));
    if (sym >= XKB_KEY_KP_0 && sym <= XKB_KEY_KP_9)
        return Key(int(Key::Kp0) + int(sym - XKB_KEY_KP_0));
    static constexpr struct {
        xkb_keysym_t sym;
        Key          key;
    } keys[] = {
        {XKB_KEY_Escape, Key::Escape},
        {XKB_KEY_Return, Key::Enter},
        {XKB_KEY_Tab, Key::Tab},
        {XKB_KEY_ISO_Left_Tab, Key::Tab},
        {XKB_KEY_BackSpace, Key::Backspace},
        {XKB_KEY_Delete, Key::Delete},
        {XKB_KEY_Insert, Key::Insert},
        {XKB_KEY_Home, Key::Home},
        {XKB_KEY_End, Key::End},
        {XKB_KEY_Prior, Key::PageUp},
        {XKB_KEY_Next, Key::PageDown},
        {XKB_KEY_Left, Key::Left},
        {XKB_KEY_Right, Key::Right},
        {XKB_KEY_Up, Key::Up},
        {XKB_KEY_Down, Key::Down},
        {XKB_KEY_Caps_Lock, Key::CapsLock},
        {XKB_KEY_Shift_L, Key::ShiftLeft},
        {XKB_KEY_Shift_R, Key::ShiftRight},
        {XKB_KEY_Control_L, Key::ControlLeft},
        {XKB_KEY_Control_R, Key::ControlRight},
        {XKB_KEY_Alt_L, Key::AltLeft},
        {XKB_KEY_Meta_L, Key::AltLeft},
        {XKB_KEY_Alt_R, Key::AltRight},
        {XKB_KEY_Meta_R, Key::AltRight},
        {XKB_KEY_ISO_Level3_Shift, Key::AltRight},
        {XKB_KEY_Super_L, Key::SuperLeft},
        {XKB_KEY_Super_R, Key::SuperRight},
        {XKB_KEY_Menu, Key::Menu},
        {XKB_KEY_Multi_key, Key::Menu},
        {XKB_KEY_Print, Key::PrintScreen},
        {XKB_KEY_Sys_Req, Key::PrintScreen},
        {XKB_KEY_Scroll_Lock, Key::ScrollLock},
        {XKB_KEY_Pause, Key::Pause},
        {XKB_KEY_Num_Lock, Key::NumLock},
        {XKB_KEY_KP_Insert, Key::Kp0},
        {XKB_KEY_KP_End, Key::Kp1},
        {XKB_KEY_KP_Down, Key::Kp2},
        {XKB_KEY_KP_Next, Key::Kp3},
        {XKB_KEY_KP_Left, Key::Kp4},
        {XKB_KEY_KP_Begin, Key::Kp5},
        {XKB_KEY_KP_Right, Key::Kp6},
        {XKB_KEY_KP_Home, Key::Kp7},
        {XKB_KEY_KP_Up, Key::Kp8},
        {XKB_KEY_KP_Prior, Key::Kp9},
        {XKB_KEY_KP_Delete, Key::KpDecimal},
        {XKB_KEY_KP_Decimal, Key::KpDecimal},
        {XKB_KEY_KP_Divide, Key::KpDivide},
        {XKB_KEY_KP_Multiply, Key::KpMultiply},
        {XKB_KEY_KP_Subtract, Key::KpSubtract},
        {XKB_KEY_KP_Add, Key::KpAdd},
        {XKB_KEY_KP_Enter, Key::KpEnter},
        {XKB_KEY_KP_Equal, Key::KpEqual},
    };
    for (const auto &entry : keys)
        if (entry.sym == sym)
            return entry.key;
    return Key::Unknown;
}
#endif

} // namespace

Key keyFromEvdev(uint32_t evdev) {
    for (const auto &e : kEvdev)
        if (e.code == evdev)
            return e.key;
    return Key::Unknown;
}

uint32_t evdevFromKey(Key k) {
    for (const auto &e : kEvdev)
        if (e.key == k)
            return e.code;
    return 0;
}

Key keyFromKeysym(xkb_keysym_t sym) {
    // Latin-1 keysyms are their code points.
    return sym == XKB_KEY_space ? Key::Space : sym < 0x80 ? core::keyFromAscii(sym) : Key::Unknown;
}

XkbKeyboard::XkbKeyboard() {
    _ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
}

void XkbKeyboard::loadCompose() {
    // Deferred to the first key press: compiling the locale table costs ~3 ms
    // and ~140 KB, and which keysyms start a sequence is only known from the
    // table itself (locales and ~/.XCompose start them with arbitrary keys).
    _composeTried = true;
    if (!_ctx)
        return;
    _compose =
        xkb_compose_table_new_from_locale(_ctx, localeForCompose(), XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!_compose)
        _compose = xkb_compose_table_new_from_locale(_ctx, "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (_compose)
        _composeState = xkb_compose_state_new(_compose, XKB_COMPOSE_STATE_NO_FLAGS);
}

XkbKeyboard::~XkbKeyboard() {
    xkb_compose_state_unref(_composeState);
    xkb_compose_table_unref(_compose);
    xkb_state_unref(_state);
    xkb_keymap_unref(_keymap);
    xkb_context_unref(_ctx);
}

void XkbKeyboard::setKeymap(xkb_keymap *keymap, xkb_state *state) {
    xkb_state_unref(_state);
    xkb_keymap_unref(_keymap);
    _keymap = keymap;
    _state  = keymap ? (state ? state : xkb_state_new(keymap)) : nullptr;
    if (!_keymap)
        return;
    _shift = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_SHIFT);
    _ctrl  = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_CTRL);
    _alt   = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_ALT);
    _super = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_LOGO);
    _caps  = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_CAPS);
    _num   = xkb_keymap_mod_get_index(_keymap, XKB_MOD_NAME_NUM);
    if (_composeState)
        xkb_compose_state_reset(_composeState);
}

void XkbKeyboard::updateMask(
    uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group
) {
    if (_state)
        xkb_state_update_mask(_state, depressed, latched, locked, 0, 0, group);
}

void XkbKeyboard::updateMask(
    uint32_t depressed,
    uint32_t latched,
    uint32_t locked,
    uint32_t baseGroup,
    uint32_t latchedGroup,
    uint32_t lockedGroup
) {
    if (_state)
        xkb_state_update_mask(
            _state, depressed, latched, locked, baseGroup, latchedGroup, lockedGroup
        );
}

uint32_t XkbKeyboard::mods() const {
    if (!_state)
        return 0;
    auto on = [this](xkb_mod_index_t i) {
        return i != XKB_MOD_INVALID &&
               xkb_state_mod_index_is_active(_state, i, XKB_STATE_MODS_EFFECTIVE) > 0;
    };
    uint32_t m = 0;
    if (on(_shift))
        m |= ModShift;
    if (on(_ctrl))
        m |= ModCtrl;
    if (on(_alt))
        m |= ModAlt;
    if (on(_super))
        m |= ModSuper;
    if (on(_caps))
        m |= ModCaps;
    if (on(_num))
        m |= ModNum;
    return m;
}

bool XkbKeyboard::repeats(uint32_t keycode) const {
    return _keymap && xkb_keymap_key_repeats(_keymap, keycode);
}

Key XkbKeyboard::keyFor(uint32_t keycode) const {
    const Key physical = keyFromEvdev(keycode - 8);
    if (!_keymap)
        return physical;
#if !defined(__linux__)
    {
        const xkb_layout_index_t active = _state ? xkb_state_key_get_layout(_state, keycode) : 0;
        const xkb_keysym_t      *syms   = nullptr;
        const int count = xkb_keymap_key_get_syms_by_level(_keymap, keycode, active, 0, &syms);
        for (int s = 0; s < count; ++s)
            if (Key k = nonPositionalFromKeysym(syms[s]); k != Key::Unknown)
                return k;
    }
#endif
    if ((physical != Key::Unknown && !isPositional(physical)) || isKeypad(physical))
        return physical;
    // Level-0 symbol of the active layout first, then every other layout the
    // user has, so Ctrl+C on a Russian+US setup still means C.
    const xkb_layout_index_t active = _state ? xkb_state_key_get_layout(_state, keycode) : 0;
    const xkb_layout_index_t n      = xkb_keymap_num_layouts_for_key(_keymap, keycode);
    for (xkb_layout_index_t i = 0; i <= n; ++i) {
        const xkb_layout_index_t layout = i == 0 ? active : i - 1;
        if (i > 0 && layout == active)
            continue;
        const xkb_keysym_t *syms = nullptr;
        const int count = xkb_keymap_key_get_syms_by_level(_keymap, keycode, layout, 0, &syms);
        for (int s = 0; s < count; ++s)
            if (Key k = keyFromKeysym(syms[s]); k != Key::Unknown)
                return k;
    }
    return physical;
}

XkbKeyboard::Result XkbKeyboard::key(uint32_t keycode, bool down, bool updateState) {
    Result r;
    r.key  = keyFor(keycode);
    r.mods = mods();
    if (!_state)
        return r;

    if (down) {
        const xkb_keysym_t sym      = xkb_state_key_get_one_sym(_state, keycode);
        bool               composed = false;
        if (!_composeTried && sym != XKB_KEY_NoSymbol)
            loadCompose();
        if (_composeState && sym != XKB_KEY_NoSymbol &&
            xkb_compose_state_feed(_composeState, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
            switch (xkb_compose_state_get_status(_composeState)) {
            case XKB_COMPOSE_COMPOSING:
                r.composing = true;
                break;
            case XKB_COMPOSE_COMPOSED: {
                char      buf[64];
                const int n = xkb_compose_state_get_utf8(_composeState, buf, sizeof buf);
                if (n > 0)
                    r.text.assign(buf, size_t(n));
                xkb_compose_state_reset(_composeState);
                composed = true;
                break;
            }
            case XKB_COMPOSE_CANCELLED:
                xkb_compose_state_reset(_composeState);
                composed = true; // the cancelling key types nothing
                break;
            case XKB_COMPOSE_NOTHING:
                break;
            }
        }
        if (!r.composing && !composed && !(r.mods & (ModCtrl | ModSuper))) {
            char      buf[64];
            const int n = xkb_state_key_get_utf8(_state, keycode, buf, sizeof buf);
            if (n > 0) {
                const auto c = (unsigned char)buf[0];
                if (!(n == 1 && (c < 0x20 || c == 0x7f))) // Enter/Tab/Backspace are keys, not text
                    r.text.assign(buf, size_t(n));
            }
        }
    }
    if (updateState)
        xkb_state_update_key(_state, keycode, down ? XKB_KEY_DOWN : XKB_KEY_UP);
    return r;
}

} // namespace plat::linux_input
