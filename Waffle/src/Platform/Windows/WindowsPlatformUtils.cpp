#include "wfpch.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Core/Application.h"

#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Waffle {

	std::string FileDialogs::OpenFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		
		std::filesystem::path initPath = std::filesystem::current_path() / "projects";
		std::string initPathStr = initPath.string();
		if (std::filesystem::exists(initPath))
			ofn.lpstrInitialDir = initPathStr.c_str();

		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;

		std::filesystem::path initPath = std::filesystem::current_path() / "projects";
		std::string initPathStr = initPath.string();
		if (std::filesystem::exists(initPath))
			ofn.lpstrInitialDir = initPathStr.c_str();

		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetSaveFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::string FileDialogs::OpenFolder(const char* initialFolder)
	{
		std::string result = "";
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

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

		return result;
	}
}