#include "TestHarness.h"

#include "Waffle/Project/RuntimeProjectConfig.h"

#include <filesystem>
#include <fstream>
#include <sstream>

WTEST(runtime_project_config_write_produces_readable_yaml)
{
	std::filesystem::path dir = std::filesystem::temp_directory_path() / "waffle_tests_config";
	std::filesystem::create_directories(dir);

	Waffle::RuntimeProjectConfig config;
	config.Name = "TestGame";
	config.StartScene = "Assets\\Scenes\\Main.waffle"; // backslashes must normalize on write
	config.IconPath = "Assets/icon.png";
	config.GravityY = -22.5f;
	config.Scenes = { "Assets\\Scenes\\Main.waffle", "Assets/Scenes/Second.waffle" };

	EXPECT_TRUE(Waffle::WriteRuntimeProjectConfig(dir, config), "write must succeed");

	std::ifstream in(dir / "project.wproj");
	EXPECT_TRUE(in.good(), "project.wproj must exist after write");
	std::stringstream buffer;
	buffer << in.rdbuf();
	in.close();
	std::string text = buffer.str();

	EXPECT_TRUE(text.find("Name: TestGame") != std::string::npos, "name must be written");
	EXPECT_TRUE(text.find("Gravity: -22.5") != std::string::npos, "gravity must be written");
	EXPECT_TRUE(text.find("StartScene: Assets/Scenes/Main.waffle") != std::string::npos,
		"start scene must be normalized to forward slashes");
	EXPECT_TRUE(text.find("Assets/Scenes/Main.waffle") != std::string::npos, "scene 1 present");
	EXPECT_TRUE(text.find("Assets/Scenes/Second.waffle") != std::string::npos, "scene 2 present");
	EXPECT_TRUE(text.find('\\') == std::string::npos, "written YAML must not contain backslashes");

	std::filesystem::remove_all(dir);
}
