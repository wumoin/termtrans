/**
 * @file test_model_profile.cpp
 * @brief 测试模型配置的 TOML 读写和默认模型解析。
 */

#include "config/config.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 创建模型配置测试使用的临时目录。
 */
std::filesystem::path MakeTempDirectory() {
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("termtrans_model_profile_test_" + std::to_string(tick));
  std::filesystem::create_directories(path);
  return path;
}

/**
 * @brief 写入测试文本文件。
 */
void WriteText(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << text;
}

/**
 * @brief 构造合法模型配置。
 */
termtrans::config::ModelProfile MakeModelProfile(std::string name) {
  return termtrans::config::ModelProfile{
      .name = std::move(name),
      .provider_type = "openai-compatible",
      .base_url = "https://api.example.com",
      .model_id = "example-model",
      .api_key = "secret-key",
      .timeout_seconds = 30,
  };
}

/**
 * @brief 验证配置文件能读取合法模型并解析默认模型。
 */
int ShouldLoadModelsAndResolveDefault() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "default_model = \"deepseek\"\n"
            "\n"
            "[[models]]\n"
            "name = \"deepseek\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.deepseek.com\"\n"
            "model_id = \"deepseek-v4-flash\"\n"
            "api_key = \"test-token\"\n"
            "timeout_seconds = 45\n");

  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);
  const termtrans::config::ResolvedConfig resolved =
      termtrans::config::ResolveConfig(load_result.config, {});

  return Expect(load_result.is_ok, "模型配置应读取成功") &&
                 Expect(load_result.config.models.size() == 1,
                        "应读取一个合法模型") &&
                 Expect(load_result.config.models.front().name == "deepseek",
                        "应读取模型名称") &&
                 Expect(load_result.config.models.front().model_id ==
                            "deepseek-v4-flash",
                        "应读取 provider 模型 ID") &&
                 Expect(load_result.config.models.front().timeout_seconds == 45,
                        "应读取 provider 请求超时时间") &&
                 Expect(resolved.default_model == "deepseek",
                        "显式 default_model 应生效")
             ? 0
             : 1;
}

/**
 * @brief 验证 timeout_seconds 为 0 时表示不设置请求超时上限。
 */
int ShouldAcceptUnlimitedTimeoutSeconds() {
  const std::filesystem::path load_config_path =
      MakeTempDirectory() / "load_config.toml";
  WriteText(load_config_path,
            "\n"
            "[[models]]\n"
            "name = \"unlimited\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"unlimited-model\"\n"
            "api_key = \"test-token\"\n"
            "timeout_seconds = 0\n");

  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(load_config_path);
  if (!Expect(load_result.is_ok, "无限超时模型配置应读取成功") ||
      !Expect(load_result.config.models.size() == 1,
              "timeout_seconds 为 0 的模型不应被忽略") ||
      !Expect(load_result.config.models.front().timeout_seconds == 0,
              "timeout_seconds 为 0 时应按原值保留")) {
    return 1;
  }

  const std::filesystem::path save_config_path =
      MakeTempDirectory() / "save_config.toml";
  termtrans::config::ModelProfile profile = MakeModelProfile("saved");
  profile.timeout_seconds = 0;
  const termtrans::config::SaveConfigResult save_result =
      termtrans::config::SaveModelProfile(save_config_path, profile, false);
  const termtrans::config::LoadConfigResult saved_load_result =
      termtrans::config::LoadConfig(save_config_path);

  return Expect(save_result.is_ok, "timeout_seconds 为 0 的模型应允许保存") &&
                 Expect(saved_load_result.is_ok, "保存后配置应可读取") &&
                 Expect(saved_load_result.config.models.size() == 1,
                        "保存后应存在一个模型") &&
                 Expect(saved_load_result.config.models.front()
                            .timeout_seconds == 0,
                        "保存后应保留 timeout_seconds 为 0")
             ? 0
             : 1;
}

/**
 * @brief 验证默认模型缺失时回退到第一个模型。
 */
int ShouldFallbackDefaultModelToFirstModel() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "\n"
            "[[models]]\n"
            "name = \"first\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"first-model\"\n"
            "api_key = \"test-token-first\"\n"
            "\n"
            "[[models]]\n"
            "name = \"second\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"second-model\"\n"
            "api_key = \"test-token-second\"\n");

  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);
  const termtrans::config::ResolvedConfig resolved =
      termtrans::config::ResolveConfig(load_result.config, {});

  return Expect(load_result.is_ok, "模型配置应读取成功") &&
                 Expect(load_result.config.models.size() == 2,
                        "应读取两个模型") &&
                 Expect(resolved.default_model == "first",
                        "缺少 default_model 时应使用第一个模型")
             ? 0
             : 1;
}

/**
 * @brief 验证非法模型和重复模型会被忽略。
 */
int ShouldIgnoreInvalidAndDuplicateModels() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "\n"
            "[[models]]\n"
            "name = \"valid\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"valid-model\"\n"
            "api_key = \"test-token-valid\"\n"
            "\n"
            "[[models]]\n"
            "name = \"valid\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"duplicate-model\"\n"
            "api_key = \"test-token-duplicate\"\n"
            "\n"
            "[[models]]\n"
            "name = \"bad\"\n"
            "provider_type = \"other\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"bad-model\"\n"
            "api_key = \"test-token-bad\"\n"
            "\n"
            "[[models]]\n"
            "name = \"bad-timeout\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"bad-timeout-model\"\n"
            "api_key = \"test-token-bad-timeout\"\n"
            "timeout_seconds = -1\n");

  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);

  return Expect(load_result.is_ok, "包含非法模型的配置仍应读取成功") &&
                 Expect(load_result.config.models.size() == 1,
                        "只应保留第一个合法唯一模型") &&
                 Expect(load_result.config.models.front().model_id ==
                            "valid-model",
                        "重复模型不应覆盖第一个合法模型")
             ? 0
             : 1;
}

/**
 * @brief 验证保存模型会写入配置并自动设置第一个默认模型。
 */
int ShouldSaveModelAndAutoDefaultFirstModel() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  const termtrans::config::SaveConfigResult save_result =
      termtrans::config::SaveModelProfile(config_path,
                                          MakeModelProfile("deepseek"), false);
  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);
  const termtrans::config::ResolvedConfig resolved =
      termtrans::config::ResolveConfig(load_result.config, {});

  return Expect(save_result.is_ok, "保存合法模型应成功") &&
                 Expect(load_result.is_ok, "保存后配置应可读取") &&
                 Expect(load_result.config.models.size() == 1,
                        "保存后应存在一个模型") &&
                 Expect(resolved.default_model == "deepseek",
                        "第一个模型应自动成为默认模型")
             ? 0
             : 1;
}

/**
 * @brief 验证替换模型和设置默认模型。
 */
int ShouldReplaceModelAndSetDefault() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  termtrans::config::ModelProfile first = MakeModelProfile("first");
  termtrans::config::ModelProfile second = MakeModelProfile("second");
  second.model_id = "second-model";
  termtrans::config::ModelProfile replacement = MakeModelProfile("first");
  replacement.model_id = "replacement-model";

  const termtrans::config::SaveConfigResult first_result =
      termtrans::config::SaveModelProfile(config_path, first, false);
  const termtrans::config::SaveConfigResult second_result =
      termtrans::config::SaveModelProfile(config_path, second, true);
  const termtrans::config::SaveConfigResult replace_result =
      termtrans::config::SaveModelProfile(config_path, replacement, false);
  const termtrans::config::SaveConfigResult default_result =
      termtrans::config::SetDefaultModel(config_path, "first");
  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);
  const termtrans::config::ResolvedConfig resolved =
      termtrans::config::ResolveConfig(load_result.config, {});
  const termtrans::config::ModelProfile* first_profile =
      termtrans::config::FindModelProfile(load_result.config, "first");

  return Expect(first_result.is_ok, "保存 first 应成功") &&
                 Expect(second_result.is_ok, "保存 second 应成功") &&
                 Expect(replace_result.is_ok, "替换 first 应成功") &&
                 Expect(default_result.is_ok, "设置默认模型应成功") &&
                 Expect(load_result.config.models.size() == 2,
                        "替换模型不应新增重复条目") &&
                 Expect(first_profile != nullptr,
                        "应能查找到替换后的 first") &&
                 Expect(first_profile->model_id == "replacement-model",
                        "同名模型应被替换") &&
                 Expect(resolved.default_model == "first",
                        "默认模型应更新为 first")
             ? 0
             : 1;
}

/**
 * @brief 验证非法模型写入会失败。
 */
int ShouldRejectInvalidModelWrites() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  termtrans::config::ModelProfile invalid = MakeModelProfile("bad");
  invalid.provider_type = "unsupported";
  termtrans::config::ModelProfile negative_timeout =
      MakeModelProfile("negative-timeout");
  negative_timeout.timeout_seconds = -1;

  return Expect(!termtrans::config::SaveModelProfile(config_path, invalid, true)
                     .is_ok,
                "非法 provider 类型不应保存") &&
                 Expect(!termtrans::config::SaveModelProfile(
                              config_path, negative_timeout, true)
                             .is_ok,
                        "负数 timeout_seconds 不应保存") &&
                 Expect(!termtrans::config::SetDefaultModel(config_path,
                                                            "missing")
                             .is_ok,
                        "不存在的模型不能设为默认")
             ? 0
             : 1;
}

}  // namespace

/**
 * @brief 运行模型配置测试。
 */
int main() {
  if (ShouldLoadModelsAndResolveDefault() != 0) {
    return 1;
  }
  if (ShouldAcceptUnlimitedTimeoutSeconds() != 0) {
    return 1;
  }
  if (ShouldFallbackDefaultModelToFirstModel() != 0) {
    return 1;
  }
  if (ShouldIgnoreInvalidAndDuplicateModels() != 0) {
    return 1;
  }
  if (ShouldSaveModelAndAutoDefaultFirstModel() != 0) {
    return 1;
  }
  if (ShouldReplaceModelAndSetDefault() != 0) {
    return 1;
  }
  if (ShouldRejectInvalidModelWrites() != 0) {
    return 1;
  }

  return 0;
}
