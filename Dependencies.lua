-- Waffle Dependencies

VULKAN_SDK = os.getenv("VULKAN_SDK")

Includedir = {}

Includedir["GLFW"] = "%{wks.location}/Waffle/vendor/GLFW/include"
Includedir["GLAD"] = "%{wks.location}/Waffle/vendor/GLAD/include"
Includedir["Box2D"] = "%{wks.location}/Waffle/vendor/Box2D/include"
Includedir["ImGui"] = "%{wks.location}/Waffle/vendor/imgui"
Includedir["glm"] = "%{wks.location}/Waffle/vendor/glm"
Includedir["stb_image"] = "%{wks.location}/Waffle/vendor/stb_image"
Includedir["entt"] = "%{wks.location}/Waffle/vendor/entt/include"
Includedir["yaml_cpp"] = "%{wks.location}/Waffle/vendor/yaml-cpp/include"
Includedir["ImGuizmo"] = "%{wks.location}/Waffle/vendor/ImGuizmo"

Includedir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
Includedir["shaderc"] = "%{VULKAN_SDK}/Include/shaderc/"
Includedir["SPIRV_Cross"] = "%{VULKAN_SDK}/Include/SPIRV-Cross"

Librarydir = {}

Librarydir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{Librarydir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{Librarydir.VulkanSDK}/VkLayer_utils.lib"

Library["ShaderC_Debug"] = "%{Librarydir.VulkanSDK}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{Librarydir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{Librarydir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{Librarydir.VulkanSDK}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{Librarydir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{Librarydir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{Librarydir.VulkanSDK}/spirv-cross-glsl.lib"