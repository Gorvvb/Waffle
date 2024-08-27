#pragma once
namespace Waffle
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}
}

// From glfw3.h
#define WF_KEY_SPACE           ::Waffle::Key::Space
#define WF_KEY_APOSTROPHE      ::Waffle::Key::Apostrophe    /* ' */
#define WF_KEY_COMMA           ::Waffle::Key::Comma         /* , */
#define WF_KEY_MINUS           ::Waffle::Key::Minus         /* - */
#define WF_KEY_PERIOD          ::Waffle::Key::Period        /* . */
#define WF_KEY_SLASH           ::Waffle::Key::Slash         /* / */
#define WF_KEY_0               ::Waffle::Key::D0
#define WF_KEY_1               ::Waffle::Key::D1
#define WF_KEY_2               ::Waffle::Key::D2
#define WF_KEY_3               ::Waffle::Key::D3
#define WF_KEY_4               ::Waffle::Key::D4
#define WF_KEY_5               ::Waffle::Key::D5
#define WF_KEY_6               ::Waffle::Key::D6
#define WF_KEY_7               ::Waffle::Key::D7
#define WF_KEY_8               ::Waffle::Key::D8
#define WF_KEY_9               ::Waffle::Key::D9
#define WF_KEY_SEMICOLON       ::Waffle::Key::Semicolon     /* ; */
#define WF_KEY_EQUAL           ::Waffle::Key::Equal         /* = */
#define WF_KEY_A               ::Waffle::Key::A
#define WF_KEY_B               ::Waffle::Key::B
#define WF_KEY_C               ::Waffle::Key::C
#define WF_KEY_D               ::Waffle::Key::D
#define WF_KEY_E               ::Waffle::Key::E
#define WF_KEY_F               ::Waffle::Key::F
#define WF_KEY_G               ::Waffle::Key::G
#define WF_KEY_H               ::Waffle::Key::H
#define WF_KEY_I               ::Waffle::Key::I
#define WF_KEY_J               ::Waffle::Key::J
#define WF_KEY_K               ::Waffle::Key::K
#define WF_KEY_L               ::Waffle::Key::L
#define WF_KEY_M               ::Waffle::Key::M
#define WF_KEY_N               ::Waffle::Key::N
#define WF_KEY_O               ::Waffle::Key::O
#define WF_KEY_P               ::Waffle::Key::P
#define WF_KEY_Q               ::Waffle::Key::Q
#define WF_KEY_R               ::Waffle::Key::R
#define WF_KEY_S               ::Waffle::Key::S
#define WF_KEY_T               ::Waffle::Key::T
#define WF_KEY_U               ::Waffle::Key::U
#define WF_KEY_V               ::Waffle::Key::V
#define WF_KEY_W               ::Waffle::Key::W
#define WF_KEY_X               ::Waffle::Key::X
#define WF_KEY_Y               ::Waffle::Key::Y
#define WF_KEY_Z               ::Waffle::Key::Z
#define WF_KEY_LEFT_BRACKET    ::Waffle::Key::LeftBracket   /* [ */
#define WF_KEY_BACKSLASH       ::Waffle::Key::Backslash     /* \ */
#define WF_KEY_RIGHT_BRACKET   ::Waffle::Key::RightBracket  /* ] */
#define WF_KEY_GRAVE_ACCENT    ::Waffle::Key::GraveAccent   /* ` */
#define WF_KEY_WORLD_1         ::Waffle::Key::World1        /* non-US #1 */
#define WF_KEY_WORLD_2         ::Waffle::Key::World2        /* non-US #2 */

/* Function keys */
#define WF_KEY_ESCAPE          ::Waffle::Key::Escape
#define WF_KEY_ENTER           ::Waffle::Key::Enter
#define WF_KEY_TAB             ::Waffle::Key::Tab
#define WF_KEY_BACKSPACE       ::Waffle::Key::Backspace
#define WF_KEY_INSERT          ::Waffle::Key::Insert
#define WF_KEY_DELETE          ::Waffle::Key::Delete
#define WF_KEY_RIGHT           ::Waffle::Key::Right
#define WF_KEY_LEFT            ::Waffle::Key::Left
#define WF_KEY_DOWN            ::Waffle::Key::Down
#define WF_KEY_UP              ::Waffle::Key::Up
#define WF_KEY_PAGE_UP         ::Waffle::Key::PageUp
#define WF_KEY_PAGE_DOWN       ::Waffle::Key::PageDown
#define WF_KEY_HOME            ::Waffle::Key::Home
#define WF_KEY_END             ::Waffle::Key::End
#define WF_KEY_CAPS_LOCK       ::Waffle::Key::CapsLock
#define WF_KEY_SCROLL_LOCK     ::Waffle::Key::ScrollLock
#define WF_KEY_NUM_LOCK        ::Waffle::Key::NumLock
#define WF_KEY_PRINT_SCREEN    ::Waffle::Key::PrintScreen
#define WF_KEY_PAUSE           ::Waffle::Key::Pause
#define WF_KEY_F1              ::Waffle::Key::F1
#define WF_KEY_F2              ::Waffle::Key::F2
#define WF_KEY_F3              ::Waffle::Key::F3
#define WF_KEY_F4              ::Waffle::Key::F4
#define WF_KEY_F5              ::Waffle::Key::F5
#define WF_KEY_F6              ::Waffle::Key::F6
#define WF_KEY_F7              ::Waffle::Key::F7
#define WF_KEY_F8              ::Waffle::Key::F8
#define WF_KEY_F9              ::Waffle::Key::F9
#define WF_KEY_F10             ::Waffle::Key::F10
#define WF_KEY_F11             ::Waffle::Key::F11
#define WF_KEY_F12             ::Waffle::Key::F12
#define WF_KEY_F13             ::Waffle::Key::F13
#define WF_KEY_F14             ::Waffle::Key::F14
#define WF_KEY_F15             ::Waffle::Key::F15
#define WF_KEY_F16             ::Waffle::Key::F16
#define WF_KEY_F17             ::Waffle::Key::F17
#define WF_KEY_F18             ::Waffle::Key::F18
#define WF_KEY_F19             ::Waffle::Key::F19
#define WF_KEY_F20             ::Waffle::Key::F20
#define WF_KEY_F21             ::Waffle::Key::F21
#define WF_KEY_F22             ::Waffle::Key::F22
#define WF_KEY_F23             ::Waffle::Key::F23
#define WF_KEY_F24             ::Waffle::Key::F24
#define WF_KEY_F25             ::Waffle::Key::F25

/* Keypad */
#define WF_KEY_KP_0            ::Waffle::Key::KP0
#define WF_KEY_KP_1            ::Waffle::Key::KP1
#define WF_KEY_KP_2            ::Waffle::Key::KP2
#define WF_KEY_KP_3            ::Waffle::Key::KP3
#define WF_KEY_KP_4            ::Waffle::Key::KP4
#define WF_KEY_KP_5            ::Waffle::Key::KP5
#define WF_KEY_KP_6            ::Waffle::Key::KP6
#define WF_KEY_KP_7            ::Waffle::Key::KP7
#define WF_KEY_KP_8            ::Waffle::Key::KP8
#define WF_KEY_KP_9            ::Waffle::Key::KP9
#define WF_KEY_KP_DECIMAL      ::Waffle::Key::KPDecimal
#define WF_KEY_KP_DIVIDE       ::Waffle::Key::KPDivide
#define WF_KEY_KP_MULTIPLY     ::Waffle::Key::KPMultiply
#define WF_KEY_KP_SUBTRACT     ::Waffle::Key::KPSubtract
#define WF_KEY_KP_ADD          ::Waffle::Key::KPAdd
#define WF_KEY_KP_ENTER        ::Waffle::Key::KPEnter
#define WF_KEY_KP_EQUAL        ::Waffle::Key::KPEqual

#define WF_KEY_LEFT_SHIFT      ::Waffle::Key::LeftShift
#define WF_KEY_LEFT_CONTROL    ::Waffle::Key::LeftControl
#define WF_KEY_LEFT_ALT        ::Waffle::Key::LeftAlt
#define WF_KEY_LEFT_SUPER      ::Waffle::Key::LeftSuper
#define WF_KEY_RIGHT_SHIFT     ::Waffle::Key::RightShift
#define WF_KEY_RIGHT_CONTROL   ::Waffle::Key::RightControl
#define WF_KEY_RIGHT_ALT       ::Waffle::Key::RightAlt
#define WF_KEY_RIGHT_SUPER     ::Waffle::Key::RightSuper
#define WF_KEY_MENU            ::Waffle::Key::Menu