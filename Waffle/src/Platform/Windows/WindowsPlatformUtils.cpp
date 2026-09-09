#include "wfpch.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Core/Application.h"

#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#ifdef WF_PLATFORM_WINDOWS
	#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Waffle {

	// Converts an ANSI filter ("Name\0*.ext\0" with a double-null
	// terminator) to its wide form for the W dialog APIs.
	static std::wstring WideFileFilter(const char* filter)
	{
		std::string narrow;
		if (filter)
		{
			const char* p = filter;
			while (*p)
			{
				narrow.append(p);
				narrow.push_back('\0');
				p += strlen(p);
			}
			narrow.push_back('\0');
		}

		int len = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
		std::wstring wide((size_t)len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.size(), wide.data(), len);
		return wide;
	}

	std::string FileDialogs::OpenFile(const char* filter)
	{
		// W (wide) API: the ANSI dialogs mangled paths outside the user's
		// code page (CJK, Cyrillic, ...) into '?' substitutions.
		std::wstring wFilter = WideFileFilter(filter);
		wchar_t szFile[MAX_PATH] = { 0 };
		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
		ofn.lStructSize = sizeof(OPENFILENAMEW);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = wFilter.c_str();
		ofn.nFilterIndex = 1;

		std::filesystem::path initPath = std::filesystem::current_path() / "projects";
		std::wstring initDir = initPath.wstring();
		if (std::filesystem::exists(initPath))
			ofn.lpstrInitialDir = initDir.c_str();

		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameW(&ofn) == TRUE)
		{
			return std::filesystem::path(szFile).string();
		}
		return std::string();
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		std::wstring wFilter = WideFileFilter(filter);
		wchar_t szFile[MAX_PATH] = { 0 };
		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
		ofn.lStructSize = sizeof(OPENFILENAMEW);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = wFilter.c_str();
		ofn.nFilterIndex = 1;

		std::filesystem::path initPath = std::filesystem::current_path() / "projects";
		std::wstring initDir = initPath.wstring();
		if (std::filesystem::exists(initPath))
			ofn.lpstrInitialDir = initDir.c_str();

		// No OFN_FILEMUSTEXIST on a save dialog - it expects a possibly-new
		// name; OVERWRITEPROMPT guards accidental replacement instead.
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		if (GetSaveFileNameW(&ofn) == TRUE)
		{
			return std::filesystem::path(szFile).string();
		}
		return std::string();
	}

	std::string FileDialogs::OpenFolder(const char* initialFolder)
	{
		std::string result = "";
		HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		IFileOpenDialog* pFileOpen = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
		if (SUCCEEDED(hr))
		{
			DWORD dwOptions = 0;
			if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
			{
				pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
			}

			std::filesystem::path initPath;
			if (initialFolder && strlen(initialFolder) > 0)
				initPath = initialFolder;
			else
				initPath = std::filesystem::current_path() / "projects";

			if (std::filesystem::exists(initPath))
			{
				IShellItem* pItem = nullptr;
				std::wstring wpath = initPath.wstring();
				if (SUCCEEDED(SHCreateItemFromParsingName(wpath.c_str(), NULL, IID_PPV_ARGS(&pItem))))
				{
					pFileOpen->SetFolder(pItem);
					pItem->Release();
				}
			}

			HWND hwnd = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
			if (SUCCEEDED(pFileOpen->Show(hwnd)))
			{
				IShellItem* pResultItem = nullptr;
				if (SUCCEEDED(pFileOpen->GetResult(&pResultItem)))
				{
					PWSTR pszFolderPath = nullptr;
					if (SUCCEEDED(pResultItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath)))
					{
						result = std::filesystem::path(pszFolderPath).string();
						CoTaskMemFree(pszFolderPath);
					}
					pResultItem->Release();
				}
			}
			pFileOpen->Release();
		}

		// Balance the CoInitializeEx above (S_FALSE still requires it).
		if (SUCCEEDED(hrCom))
			CoUninitialize();

		return result;
	}

	void PlatformUtils::OpenFileInEditor(const std::string& filepath)
	{
		std::filesystem::path absPath = std::filesystem::absolute(filepath);
		std::string pathStr = absPath.string();

		std::string quoted = "\"" + pathStr + "\"";
		HINSTANCE res = ShellExecuteA(NULL, "open", "code", quoted.c_str(), NULL, SW_SHOW);
		if ((INT_PTR)res <= 32)
		{
			ShellExecuteA(NULL, "open", "notepad.exe", quoted.c_str(), NULL, SW_SHOW);
		}
	}
}