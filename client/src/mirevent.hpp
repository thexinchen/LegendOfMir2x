#pragma once
//
// Native event and POD types replacing SDL3.
//
// All field names mirror the SDL3 event structs so consuming code only
// needs include/typedef changes. Timing helpers (mirGetTicks/mirDelay)
// replace SDL_GetTicks/SDL_Delay with steady_clock.
//

#include <cstdint>
#include <chrono>
#include <thread>
#include <string>

// ---------------------------------------------------------------------------
// Key codes (replacing SDL_Keycode / SDLK_*)
// ---------------------------------------------------------------------------
using MirKeycode = int32_t;

constexpr MirKeycode MIRK_UNKNOWN     = 0x00000000;
constexpr MirKeycode MIRK_BACKSPACE   = 0x00000008;
constexpr MirKeycode MIRK_TAB         = 0x00000009;
constexpr MirKeycode MIRK_RETURN      = 0x0000000D;
constexpr MirKeycode MIRK_ESCAPE      = 0x0000001B;
constexpr MirKeycode MIRK_SPACE       = 0x00000020;

constexpr MirKeycode MIRK_0           = 0x00000030;
constexpr MirKeycode MIRK_1           = 0x00000031;
constexpr MirKeycode MIRK_2           = 0x00000032;
constexpr MirKeycode MIRK_3           = 0x00000033;
constexpr MirKeycode MIRK_4           = 0x00000034;
constexpr MirKeycode MIRK_5           = 0x00000035;
constexpr MirKeycode MIRK_6           = 0x00000036;
constexpr MirKeycode MIRK_7           = 0x00000037;
constexpr MirKeycode MIRK_8           = 0x00000038;
constexpr MirKeycode MIRK_9           = 0x00000039;

constexpr MirKeycode MIRK_A           = 0x00000061;
constexpr MirKeycode MIRK_B           = 0x00000062;
constexpr MirKeycode MIRK_C           = 0x00000063;
constexpr MirKeycode MIRK_D           = 0x00000064;
constexpr MirKeycode MIRK_E           = 0x00000065;
constexpr MirKeycode MIRK_F           = 0x00000066;
constexpr MirKeycode MIRK_G           = 0x00000067;
constexpr MirKeycode MIRK_H           = 0x00000068;
constexpr MirKeycode MIRK_I           = 0x00000069;
constexpr MirKeycode MIRK_J           = 0x0000006A;
constexpr MirKeycode MIRK_K           = 0x0000006B;
constexpr MirKeycode MIRK_L           = 0x0000006C;
constexpr MirKeycode MIRK_M           = 0x0000006D;
constexpr MirKeycode MIRK_N           = 0x0000006E;
constexpr MirKeycode MIRK_O           = 0x0000006F;
constexpr MirKeycode MIRK_P           = 0x00000070;
constexpr MirKeycode MIRK_Q           = 0x00000071;
constexpr MirKeycode MIRK_R           = 0x00000072;
constexpr MirKeycode MIRK_S           = 0x00000073;
constexpr MirKeycode MIRK_T           = 0x00000074;
constexpr MirKeycode MIRK_U           = 0x00000075;
constexpr MirKeycode MIRK_V           = 0x00000076;
constexpr MirKeycode MIRK_W           = 0x00000077;
constexpr MirKeycode MIRK_X           = 0x00000078;
constexpr MirKeycode MIRK_Y           = 0x00000079;
constexpr MirKeycode MIRK_Z           = 0x0000007A;

constexpr MirKeycode MIRK_DELETE      = 0x0000007F;

// scancode-based keycodes (SDL_SCANCODE_MASK = 0x40000000)
constexpr MirKeycode MIRK_F1          = 0x4000003A;
constexpr MirKeycode MIRK_F2          = 0x4000003B;
constexpr MirKeycode MIRK_F3          = 0x4000003C;
constexpr MirKeycode MIRK_F4          = 0x4000003D;
constexpr MirKeycode MIRK_F5          = 0x4000003E;
constexpr MirKeycode MIRK_F6          = 0x4000003F;
constexpr MirKeycode MIRK_F7          = 0x40000040;
constexpr MirKeycode MIRK_F8          = 0x40000041;
constexpr MirKeycode MIRK_F9          = 0x40000042;
constexpr MirKeycode MIRK_F10         = 0x40000043;
constexpr MirKeycode MIRK_F11         = 0x40000044;
constexpr MirKeycode MIRK_F12         = 0x40000045;

constexpr MirKeycode MIRK_HOME        = 0x4000004A;
constexpr MirKeycode MIRK_PAGEUP      = 0x4000004B;
constexpr MirKeycode MIRK_END         = 0x4000004D;
constexpr MirKeycode MIRK_PAGEDOWN    = 0x4000004E;
constexpr MirKeycode MIRK_RIGHT       = 0x4000004F;
constexpr MirKeycode MIRK_LEFT        = 0x40000050;
constexpr MirKeycode MIRK_DOWN        = 0x40000051;
constexpr MirKeycode MIRK_UP          = 0x40000052;

constexpr MirKeycode MIRK_KP_0        = 0x40000062;
constexpr MirKeycode MIRK_KP_1        = 0x40000059;
constexpr MirKeycode MIRK_KP_2        = 0x4000005A;
constexpr MirKeycode MIRK_KP_3        = 0x4000005B;
constexpr MirKeycode MIRK_KP_4        = 0x4000005C;
constexpr MirKeycode MIRK_KP_5        = 0x4000005D;
constexpr MirKeycode MIRK_KP_6        = 0x4000005E;
constexpr MirKeycode MIRK_KP_7        = 0x4000005F;
constexpr MirKeycode MIRK_KP_8        = 0x40000060;
constexpr MirKeycode MIRK_KP_9        = 0x40000061;

constexpr MirKeycode MIRK_LCTRL       = 0x400000E0;
constexpr MirKeycode MIRK_LSHIFT      = 0x400000E1;
constexpr MirKeycode MIRK_RCTRL       = 0x400000E4;
constexpr MirKeycode MIRK_RSHIFT      = 0x400000E5;

// ---------------------------------------------------------------------------
// Key modifiers (replacing SDL_Keymod / SDL_KMOD_*)
// ---------------------------------------------------------------------------
using MirKeymod = uint16_t;

constexpr MirKeymod MIRKMOD_NONE   = 0x0000;
constexpr MirKeymod MIRKMOD_LSHIFT = 0x0001;
constexpr MirKeymod MIRKMOD_RSHIFT = 0x0002;
constexpr MirKeymod MIRKMOD_LCTRL  = 0x0040;
constexpr MirKeymod MIRKMOD_RCTRL  = 0x0080;
constexpr MirKeymod MIRKMOD_LALT   = 0x0100;
constexpr MirKeymod MIRKMOD_RALT   = 0x0200;
constexpr MirKeymod MIRKMOD_NUM    = 0x1000;
constexpr MirKeymod MIRKMOD_CAPS   = 0x2000;
constexpr MirKeymod MIRKMOD_SHIFT  = (MIRKMOD_LSHIFT | MIRKMOD_RSHIFT);
constexpr MirKeymod MIRKMOD_CTRL   = (MIRKMOD_LCTRL  | MIRKMOD_RCTRL);
constexpr MirKeymod MIRKMOD_ALT    = (MIRKMOD_LALT   | MIRKMOD_RALT);

// ---------------------------------------------------------------------------
// Blend mode (replacing SDL_BlendMode / SDL_BLENDMODE_*)
// ---------------------------------------------------------------------------
enum MirBlendMode : uint32_t
{
    MIR_BLENDMODE_NONE  = 0x00000000,
    MIR_BLENDMODE_BLEND = 0x00000001,
    MIR_BLENDMODE_ADD   = 0x00000002,
    MIR_BLENDMODE_MOD   = 0x00000004,
    MIR_BLENDMODE_MUL   = 0x00000008,
};

// ---------------------------------------------------------------------------
// Flip mode (replacing SDL_FlipMode / SDL_FLIP_*)
// ---------------------------------------------------------------------------
enum MirFlipMode : int
{
    MIR_FLIP_NONE       = 0,
    MIR_FLIP_HORIZONTAL = 1,
    MIR_FLIP_VERTICAL   = 2,
};

// ---------------------------------------------------------------------------
// Event types (replacing SDL_EventType / SDL_EVENT_*)
// ---------------------------------------------------------------------------
enum MirEventType : uint32_t
{
    MIR_EVENT_QUIT                       = 0x100,
    MIR_EVENT_WINDOW_RESIZED             = 0x202,
    MIR_EVENT_WINDOW_PIXEL_SIZE_CHANGED  = 0x203,
    MIR_EVENT_KEY_DOWN                   = 0x300,
    MIR_EVENT_KEY_UP                     = 0x301,
    MIR_EVENT_TEXT_INPUT                 = 0x302,
    MIR_EVENT_KEYMAP_CHANGED             = 0x303,
    MIR_EVENT_MOUSE_MOTION               = 0x400,
    MIR_EVENT_MOUSE_BUTTON_DOWN          = 0x401,
    MIR_EVENT_MOUSE_BUTTON_UP            = 0x402,
    MIR_EVENT_MOUSE_WHEEL                = 0x403,
};

// ---------------------------------------------------------------------------
// Mouse button constants (replacing SDL_BUTTON_*)
// ---------------------------------------------------------------------------
constexpr int MIR_BUTTON_LEFT   = 1;
constexpr int MIR_BUTTON_MIDDLE = 2;
constexpr int MIR_BUTTON_RIGHT  = 3;

inline constexpr uint32_t MIR_BUTTON_MASK(int X) { return 1u << (X - 1); }
constexpr uint32_t MIR_BUTTON_LMASK = MIR_BUTTON_MASK(MIR_BUTTON_LEFT);
constexpr uint32_t MIR_BUTTON_MMASK = MIR_BUTTON_MASK(MIR_BUTTON_MIDDLE);
constexpr uint32_t MIR_BUTTON_RMASK = MIR_BUTTON_MASK(MIR_BUTTON_RIGHT);

// ---------------------------------------------------------------------------
// Event structs (matching SDL3 field names exactly)
// ---------------------------------------------------------------------------
struct MirKeyEvent
{
    MirEventType type;
    MirKeycode   key;
    MirKeymod    mod;
    bool         down;
};

struct MirMouseButtonEvent
{
    MirEventType type;
    uint8_t      button;
    float        x;
    float        y;
    int          clicks;
    bool         down;
};

struct MirMouseMotionEvent
{
    MirEventType type;
    float        x;
    float        y;
    float        xrel;
    float        yrel;
    uint32_t     state;
};

struct MirMouseWheelEvent
{
    MirEventType type;
    float        x;
    float        y;
    float        mouse_x;
    float        mouse_y;
};

struct MirTextInputEvent
{
    MirEventType  type;
    const char   *text;
};

struct MirWindowEvent
{
    MirEventType type;
    int          x;
    int          y;
};

// ---------------------------------------------------------------------------
// MirEvent: union replacing SDL_Event
// ---------------------------------------------------------------------------
struct MirEvent
{
    MirEventType type;
    union
    {
        MirKeyEvent         key;
        MirMouseButtonEvent button;
        MirMouseMotionEvent motion;
        MirMouseWheelEvent  wheel;
        MirTextInputEvent   text;
        MirWindowEvent      window;
    };
};

// ---------------------------------------------------------------------------
// Timing helpers (replacing SDL_GetTicks / SDL_Delay)
// ---------------------------------------------------------------------------
inline uint64_t mirGetTicks()
{
    static const auto s_start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - s_start).count();
}

inline void mirDelay(uint32_t ms)
{
    if(ms > 0){
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}
