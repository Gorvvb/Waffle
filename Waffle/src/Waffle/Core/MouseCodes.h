#pragma once

namespace Waffle
{
	typedef enum class MouseCode : uint16_t
	{
		// From glfw3.h
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Button6 = 6,
		Button7 = 7,

		ButtonLast = Button7,
		ButtonLeft = Button0,
		ButtonRight = Button1,
		ButtonMiddle = Button2
	} Mouse;

	inline std::ostream& operator<<(std::ostream& os, MouseCode mouseCode)
	{
		os << static_cast<int32_t>(mouseCode);
		return os;
	}
}

#define WF_MOUSE_BUTTON_0      ::Waffle::Mouse::Button0
#define WF_MOUSE_BUTTON_1      ::Waffle::Mouse::Button1
#define WF_MOUSE_BUTTON_2      ::Waffle::Mouse::Button2
#define WF_MOUSE_BUTTON_3      ::Waffle::Mouse::Button3
#define WF_MOUSE_BUTTON_4      ::Waffle::Mouse::Button4
#define WF_MOUSE_BUTTON_5      ::Waffle::Mouse::Button5
#define WF_MOUSE_BUTTON_6      ::Waffle::Mouse::Button6
#define WF_MOUSE_BUTTON_7      ::Waffle::Mouse::Button7
#define WF_MOUSE_BUTTON_LAST   ::Waffle::Mouse::ButtonLast
#define WF_MOUSE_BUTTON_LEFT   ::Waffle::Mouse::ButtonLeft
#define WF_MOUSE_BUTTON_RIGHT  ::Waffle::Mouse::ButtonRight