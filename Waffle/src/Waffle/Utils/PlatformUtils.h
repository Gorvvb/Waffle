#pragma once

#include <string>

namespace Waffle {

	class FileDialogs
	{
	public:
		// These returns empty string if the dialog is cancelled
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
	};
}