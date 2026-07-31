import os
import subprocess
from pathlib import Path
import Utils

from pathlib import Path

class VulkanConfiguration:
    vulkanDirectory = "./Waffle/vendor/VulkanSDK"

    @classmethod
    def Validate(cls):
        if not cls.CheckVulkanSDK():
            print("Vulkan SDK not installed correctly.")
            return
            
        if not cls.CheckVulkanSDKDebugLibs():
            print("\nNo Vulkan SDK debug libs found. Install Vulkan SDK with debug libs.")
            print("Debug configuration disabled.")

    @classmethod
    def CheckVulkanSDK(cls):
        vulkanSDK = os.environ.get("VULKAN_SDK")
        if vulkanSDK is None or not Path(vulkanSDK).exists():
            print("\nYou don't have the Vulkan SDK installed!")
            cls.__InstallVulkanSDK()
            return False
        else:
            print(f"\nLocated Vulkan SDK at {vulkanSDK}")

        # Check for required files
        if cls.CheckVulkanSDKFiles(vulkanSDK):
            print(f"Vulkan SDK located at {vulkanSDK}")
            return True
        else:
            print(f"Vulkan SDK directory is missing required files.")
            cls.__InstallVulkanSDK()
            return False

    @classmethod
    def CheckVulkanSDKFiles(cls, sdkPath):
        # Check for specific files that should exist in the Vulkan SDK
        required_files = [
            "Lib/shaderc_sharedd.lib",
            "Include/vulkan/vulkan.h",
        ]
        for file in required_files:
            file_path = Path(sdkPath) / file
            if not file_path.exists():
                return False
        return True

    @classmethod
    def __InstallVulkanSDK(cls):
        permissionGranted = False
        while not permissionGranted:
            reply = str(input(f"Would you like to install VulkanSDK? [Y/N]: ")).lower().strip()[:1]
            if reply == 'n':
                return
            permissionGranted = (reply == 'y')

        vulkanInstallURL = "https://sdk.lunarg.com/sdk/download/1.3.290.0/windows/VulkanSDK-1.3.290.0-Installer.exe"
        vulkanPath = f"{cls.vulkanDirectory}/VulkanSDK-1.3.290.0-Installer.exe"
        print(f"Downloading {vulkanInstallURL} to {vulkanPath}")
        Utils.DownloadFile(vulkanInstallURL, vulkanPath)
        print("Running Vulkan SDK installer...")
        os.startfile(os.path.abspath(vulkanPath))
        print("Re-run this script after installation!")
        quit()

    @classmethod
    def CheckVulkanSDKDebugLibs(cls):
        shadercdLib = Path(f"{cls.vulkanDirectory}/Lib/shaderc_sharedd.lib")
        return shadercdLib.exists()

if __name__ == "__main__":
    VulkanConfiguration.Validate()
