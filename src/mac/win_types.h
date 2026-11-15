// macOS: the names of Windows types and constants that the shared code uses, with the values of Windows (the keys are
// the virtual keys of Windows: src/mac/app_mac.mm turns the keys of macOS into them). No function of Windows is here.
#pragma once

#include <cstdint>

using UINT = unsigned int;
using DWORD = std::uint32_t;
using WORD = std::uint16_t;
using BYTE = std::uint8_t;
using LONG = std::int32_t;
using UINT_PTR = std::uintptr_t;
using INT_PTR = std::intptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using COLORREF = std::uint32_t;
using HANDLE = void *;
using HWND = void *;
using HINSTANCE = void *;
using HMENU = void *;
using HFONT = void *;
using HBRUSH = void *;
using HDC = void *;
using HICON = void *;
using WNDPROC = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

struct POINT {
	LONG x, y;
};
struct RECT {
	LONG left, top, right, bottom;
};

// messages of the window (only the ones the shared code names)
inline constexpr UINT WM_CLOSE = 0x0010;
inline constexpr UINT WM_CHAR = 0x0102;
inline constexpr UINT WM_MOUSELEAVE = 0x02A3;
inline constexpr UINT WM_APP = 0x8000;

// message boxes
inline constexpr UINT MB_OK = 0x0;
inline constexpr UINT MB_OKCANCEL = 0x1;
inline constexpr UINT MB_YESNOCANCEL = 0x3;
inline constexpr UINT MB_YESNO = 0x4;
inline constexpr UINT MB_ICONERROR = 0x10;
inline constexpr UINT MB_ICONQUESTION = 0x20;
inline constexpr UINT MB_ICONWARNING = 0x30;
inline constexpr UINT MB_ICONINFORMATION = 0x40;
inline constexpr UINT MB_DEFBUTTON1 = 0x000;
inline constexpr UINT MB_DEFBUTTON2 = 0x100;
inline constexpr UINT MB_DEFBUTTON3 = 0x200;
inline constexpr int IDOK = 1;
inline constexpr int IDCANCEL = 2;
inline constexpr int IDYES = 6;
inline constexpr int IDNO = 7;

// virtual keys
inline constexpr int VK_BACK = 0x08;
inline constexpr int VK_TAB = 0x09;
inline constexpr int VK_RETURN = 0x0D;
inline constexpr int VK_SHIFT = 0x10;
inline constexpr int VK_CONTROL = 0x11;
inline constexpr int VK_MENU = 0x12;
inline constexpr int VK_ESCAPE = 0x1B;
inline constexpr int VK_SPACE = 0x20;
inline constexpr int VK_PRIOR = 0x21;
inline constexpr int VK_NEXT = 0x22;
inline constexpr int VK_END = 0x23;
inline constexpr int VK_HOME = 0x24;
inline constexpr int VK_LEFT = 0x25;
inline constexpr int VK_UP = 0x26;
inline constexpr int VK_RIGHT = 0x27;
inline constexpr int VK_DOWN = 0x28;
inline constexpr int VK_INSERT = 0x2D;
inline constexpr int VK_DELETE = 0x2E;
inline constexpr int VK_F1 = 0x70;
inline constexpr int VK_F2 = 0x71;
inline constexpr int VK_F3 = 0x72;
inline constexpr int VK_F4 = 0x73;
inline constexpr int VK_F5 = 0x74;
inline constexpr int VK_F6 = 0x75;
inline constexpr int VK_F7 = 0x76;
inline constexpr int VK_F8 = 0x77;
inline constexpr int VK_F9 = 0x78;
inline constexpr int VK_F10 = 0x79;
inline constexpr int VK_F11 = 0x7A;
inline constexpr int VK_F12 = 0x7B;
