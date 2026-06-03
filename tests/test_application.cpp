/**
 * @file test_application.cpp
 * @brief 测试应用层命令分发。
 */

#include "app/application.h"
#include "history/history_key.h"
#include "history/history_record.h"
#include "history/sqlite_history_store.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 *
 * @param condition 断言条件。
 * @param message 断言失败时写入 stderr 的说明。
 * @return 条件为 true 时返回 true。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 判断文本是否包含期望片段。
 *
 * @param text 被检查的完整文本。
 * @param expected 必须出现的片段。
 * @param message 断言失败时写入 stderr 的说明。
 * @return 包含期望片段时返回 true。
 */
bool ExpectContains(const std::string& text,
                    const std::string& expected,
                    const char* message) {
  if (text.find(expected) == std::string::npos) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 在测试期间临时修改环境变量，并在析构时恢复原始值。
 *
 * Application 使用默认配置路径，测试通过临时 HOME 隔离配置文件写入。
 *
 * @note 该类不是线程安全的。
 */
class EnvironmentVariableGuard {
 public:
  /**
   * @brief 保存环境变量原始值，供析构时恢复。
   *
   * @param name 环境变量名，调用方必须保证其生命周期长于本 guard。
   */
  explicit EnvironmentVariableGuard(const char* name)
      : name_(name), original_value_(Read(name)) {}

  /**
   * @brief 恢复构造时捕获的环境变量状态。
   *
   * 如果变量原本不存在，析构时会再次删除它。
   */
  ~EnvironmentVariableGuard() {
    if (original_value_.has_value()) {
      Set(name_, *original_value_);
      return;
    }

    Unset(name_);
  }

  /**
   * @brief 设置环境变量。
   *
   * @param name 环境变量名。
   * @param value 要写入的值。
   */
  static void Set(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
  }

  /**
   * @brief 删除环境变量。
   *
   * @param name 环境变量名。
   */
  static void Unset(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
  }

 private:
  /**
   * @brief 读取环境变量当前值。
   *
   * @param name 环境变量名。
   * @return 变量存在时返回其值；不存在时返回空 optional。
   */
  static std::optional<std::string> Read(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
      return std::nullopt;
    }

    return std::string(value);
  }

  const char* name_;  // 需要恢复的环境变量名。
  std::optional<std::string> original_value_;  // 测试修改前的原始值。
};

/**
 * @brief 创建应用层测试使用的临时配置根目录。
 *
 * @return 已创建的临时目录路径。
 */
std::filesystem::path MakeTempDirectory() {
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("termtrans_application_test_" + std::to_string(tick));
  std::filesystem::create_directories(path);
  return path;
}

/**
 * @brief 写入测试文本文件。
 *
 * @param path 目标文件路径；父目录不存在时会创建。
 * @param text 要写入的完整文本。
 */
void WriteText(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << text;
}

/**
 * @brief 将应用默认配置目录重定向到测试临时目录。
 *
 * Windows 使用 APPDATA 或 USERPROFILE，类 Unix 平台使用 HOME。测试同时设置这些
 * 变量，避免平台差异导致写入真实用户目录。
 *
 * @note 该类不是线程安全的。
 */
class ConfigEnvironmentGuard {
 public:
  /**
   * @brief 将所有配置路径相关环境变量指向同一临时根目录。
   *
   * @param home 测试隔离目录。
   */
  explicit ConfigEnvironmentGuard(const std::filesystem::path& home)
      : home_guard_("HOME"),
        app_data_guard_("APPDATA"),
        user_profile_guard_("USERPROFILE") {
    EnvironmentVariableGuard::Set("HOME", home.string());
    EnvironmentVariableGuard::Set("APPDATA", home.string());
    EnvironmentVariableGuard::Set("USERPROFILE", home.string());
  }

 private:
  EnvironmentVariableGuard home_guard_;  // 恢复类 Unix 默认配置根目录变量。
  EnvironmentVariableGuard app_data_guard_;  // 恢复 Windows roaming 配置根目录变量。
  EnvironmentVariableGuard user_profile_guard_;  // 恢复 Windows 用户主目录变量。
};

/**
 * @brief 一次 Application::Run() 调用的可观察结果。
 */
struct RunResult {
  int exit_code = 0;  // Application 返回的进程退出码。
  std::string stdout_text;  // 成功命令输出内容。
  std::string stderr_text;  // 诊断或交互提示内容。
};

/**
 * @brief 在内存流中运行应用层命令，并注入 stdin 文本。
 *
 * 交互式命令通过 stdin 读取用户回答；该辅助函数避免测试依赖真实终端。
 *
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @param stdin_text 提供给应用的 stdin 文本。
 * @return 捕获到的退出码、stdout 和 stderr。
 */
RunResult RunApplicationWithInput(int argc,
                                  char* argv[],
                                  const std::string& stdin_text) {
  termtrans::app::Application application;
  std::istringstream stdin_stream(stdin_text);
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;
  const int exit_code =
      application.Run(argc, argv, stdin_stream, stdout_stream, stderr_stream);

  return RunResult{
      .exit_code = exit_code,
      .stdout_text = stdout_stream.str(),
      .stderr_text = stderr_stream.str(),
  };
}

/**
 * @brief 在内存流中运行应用层命令。
 *
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @return 捕获到的退出码、stdout 和 stderr。
 */
RunResult RunApplication(int argc, char* argv[]) {
  return RunApplicationWithInput(argc, argv, "");
}

/**
 * @brief 将合法模型配置写入测试默认配置文件。
 *
 * @param home 测试隔离目录。
 */
void WriteModelConfig(const std::filesystem::path& home) {
  const std::filesystem::path config_path =
#ifdef _WIN32
      home / "termtrans" / "config.toml";
#elif defined(__APPLE__)
      home / "Library" / "Application Support" / "termtrans" / "config.toml";
#else
      home / ".config" / "termtrans" / "config.toml";
#endif
  WriteText(config_path,
            "default_model = \"deepseek\"\n"
            "\n"
            "[[models]]\n"
            "name = \"deepseek\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.deepseek.com\"\n"
            "model_id = \"deepseek-v4-flash\"\n"
            "api_key = \"test-token-secret\"\n"
            "\n"
            "[[models]]\n"
            "name = \"local\"\n"
            "provider_type = \"openai-compatible\"\n"
            "base_url = \"https://api.example.com\"\n"
            "model_id = \"local-model\"\n"
            "api_key = \"test-token-local\"\n");
}

/**
 * @brief 返回测试环境下的默认历史数据库路径。
 *
 * @param home 测试隔离目录。
 * @return 与默认 config.toml 同目录的 history.sqlite 路径。
 */
std::filesystem::path HistoryPath(const std::filesystem::path& home) {
#ifdef _WIN32
  return home / "termtrans" / "history.sqlite";
#elif defined(__APPLE__)
  return home / "Library" / "Application Support" / "termtrans" /
         "history.sqlite";
#else
  return home / ".config" / "termtrans" / "history.sqlite";
#endif
}

/**
 * @brief 写入一条应用层历史命令测试使用的历史记录。
 *
 * @param home 测试隔离目录。
 */
void WriteHistoryRecord(const std::filesystem::path& home) {
  termtrans::history::SqliteHistoryStore store(HistoryPath(home));
  const std::string input_text = "hello";
  const std::string translated_text = "你好";
  const termtrans::history::HistoryRecord record{
      .key = termtrans::history::ComputeHistoryKey(input_text, "zh-CN"),
      .input_text = input_text,
      .translated_text = translated_text,
      .model_name = "deepseek",
      .provider_type = "openai-compatible",
      .model_id = "deepseek-v4-flash",
      .base_url = "https://api.deepseek.com",
      .input_bytes = input_text.size(),
      .output_bytes = translated_text.size(),
  };
  store.Save(record);
}

/**
 * @brief 验证应用层会执行合法配置写入命令。
 *
 * @return 成功时返回 0。
 */
int ShouldRunConfigSetCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char config[] = "config";
  char set[] = "set";
  char key[] = "stream";
  char value[] = "false";
  char* argv[] = {program, config, set, key, value};

  const RunResult result = RunApplication(5, argv);

  return Expect(result.exit_code == 0, "config set 应返回 0") &&
                 ExpectContains(result.stdout_text, "stream",
                                "配置写入成功输出应包含键名") &&
                 Expect(result.stderr_text.empty(),
                        "配置写入成功不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证非法配置值会在应用层转成用法错误退出码。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectInvalidConfigSetCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char config[] = "config";
  char set[] = "set";
  char key[] = "ui-language";
  char value[] = "ja";
  char* argv[] = {program, config, set, key, value};

  const RunResult result = RunApplication(5, argv);

  return Expect(result.exit_code == 2, "非法配置写入应返回 2") &&
                 ExpectContains(result.stderr_text, "ja",
                                "非法配置提示应包含配置值")
             ? 0
             : 1;
}

/**
 * @brief 验证 UI 语言列表命令写入 stdout 且不污染 stderr。
 *
 * @return 成功时返回 0。
 */
int ShouldRunUiLanguagesListCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char ui_languages[] = "ui-languages";
  char list[] = "list";
  char* argv[] = {program, ui_languages, list};

  const RunResult result = RunApplication(3, argv);

  return Expect(result.exit_code == 0, "ui-languages list 应返回 0") &&
                 ExpectContains(result.stdout_text, "zh-CN",
                                "UI 语言列表应包含 zh-CN") &&
                 ExpectContains(result.stdout_text, "en",
                                "UI 语言列表应包含 en") &&
                 Expect(result.stderr_text.empty(),
                        "UI 语言列表不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言列表命令写入 stdout 且不污染 stderr。
 *
 * @return 成功时返回 0。
 */
int ShouldRunLanguagesListCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char languages[] = "languages";
  char list[] = "list";
  char* argv[] = {program, languages, list};

  const RunResult result = RunApplication(3, argv);

  return Expect(result.exit_code == 0, "languages list 应返回 0") &&
                 ExpectContains(result.stdout_text, "zh-CN",
                                "目标语言列表应包含 zh-CN") &&
                 ExpectContains(result.stdout_text, "builtin",
                                "目标语言列表应展示来源") &&
                 Expect(result.stderr_text.empty(),
                        "目标语言列表不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言 show 命令展示当前生效 prompt。
 *
 * @return 成功时返回 0。
 */
int ShouldRunLanguagesShowCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char languages[] = "languages";
  char show[] = "show";
  char language[] = "zh-CN";
  char* argv[] = {program, languages, show, language};

  const RunResult result = RunApplication(4, argv);

  return Expect(result.exit_code == 0, "languages show 应返回 0") &&
                 ExpectContains(result.stdout_text, "zh-CN",
                                "show 输出应包含目标语言") &&
                 ExpectContains(result.stdout_text, "只输出译文",
                                "show 输出应包含完整 prompt") &&
                 Expect(result.stderr_text.empty(),
                        "show 成功不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言 set/show/unset 会读写配置 prompt。
 *
 * @return 成功时返回 0。
 */
int ShouldRunLanguagesSetAndUnsetCommands() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  const std::filesystem::path prompt_path = home / "prompt.txt";
  WriteText(prompt_path, "custom pirate prompt\nOnly output pirate text.\n");

  char program[] = "termtrans";
  char languages[] = "languages";
  char set[] = "set";
  char custom_language[] = "pirate";
  char file[] = "--file";
  std::string prompt_path_text = prompt_path.string();
  char* set_argv[] = {program,
                      languages,
                      set,
                      custom_language,
                      file,
                      prompt_path_text.data()};

  const RunResult set_result = RunApplication(6, set_argv);
  if (!Expect(set_result.exit_code == 0, "languages set 应返回 0") ||
      !ExpectContains(set_result.stdout_text, "pirate",
                      "set 成功提示应包含语言 code") ||
      !Expect(set_result.stderr_text.empty(),
              "set 成功不应输出 stderr")) {
    return 1;
  }

  char show[] = "show";
  char* show_argv[] = {program, languages, show, custom_language};
  const RunResult show_result = RunApplication(4, show_argv);
  if (!Expect(show_result.exit_code == 0, "自定义语言 show 应返回 0") ||
      !ExpectContains(show_result.stdout_text, "custom pirate prompt",
                      "show 应展示配置 prompt") ||
      !ExpectContains(show_result.stdout_text, "config",
                      "show 应展示 config 来源")) {
    return 1;
  }

  char unset[] = "unset";
  char* unset_argv[] = {program, languages, unset, custom_language};
  const RunResult unset_result = RunApplication(4, unset_argv);
  if (!Expect(unset_result.exit_code == 0, "languages unset 应返回 0") ||
      !ExpectContains(unset_result.stdout_text, "pirate",
                      "unset 成功提示应包含语言 code")) {
    return 1;
  }

  const RunResult show_after_unset_result = RunApplication(4, show_argv);
  return Expect(show_after_unset_result.exit_code == 2,
                "删除自定义语言后 show 应返回 2") &&
                 ExpectContains(show_after_unset_result.stderr_text, "pirate",
                                "未知语言提示应包含语言 code")
             ? 0
             : 1;
}

/**
 * @brief 验证 languages unset 缺少配置项时保持幂等成功。
 *
 * @return 成功时返回 0。
 */
int ShouldTreatMissingLanguageUnsetAsNoop() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char languages[] = "languages";
  char unset[] = "unset";
  char language[] = "zh-CN";
  char* argv[] = {program, languages, unset, language};

  const RunResult result = RunApplication(4, argv);

  return Expect(result.exit_code == 0,
                "缺少配置覆盖时 unset 应保持幂等成功") &&
                 ExpectContains(result.stdout_text, "zh-CN",
                                "幂等提示应包含语言 code") &&
                 Expect(result.stderr_text.empty(),
                        "幂等 unset 不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证 prompt 文件读取失败会返回运行时错误。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectMissingPromptFile() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  const std::filesystem::path prompt_path = home / "missing.txt";

  char program[] = "termtrans";
  char languages[] = "languages";
  char set[] = "set";
  char custom_language[] = "pirate";
  char file[] = "--file";
  std::string prompt_path_text = prompt_path.string();
  char* argv[] = {program,
                  languages,
                  set,
                  custom_language,
                  file,
                  prompt_path_text.data()};

  const RunResult result = RunApplication(6, argv);

  return Expect(result.exit_code == 1,
                "缺失 prompt 文件应返回运行时错误") &&
                 ExpectContains(result.stderr_text, prompt_path_text,
                                "读取失败提示应包含文件路径")
             ? 0
             : 1;
}

/**
 * @brief 验证模型列表命令输出模型但不泄露 API key。
 *
 * @return 成功时返回 0。
 */
int ShouldRunModelsListCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  WriteModelConfig(home);

  char program[] = "termtrans";
  char models[] = "models";
  char list[] = "list";
  char* argv[] = {program, models, list};

  const RunResult result = RunApplication(3, argv);

  return Expect(result.exit_code == 0, "models list 应返回 0") &&
                 ExpectContains(result.stdout_text, "deepseek",
                                "模型列表应包含模型名称") &&
                 ExpectContains(result.stdout_text, "deepseek-v4-flash",
                                "模型列表应包含 provider 模型 ID") &&
                 Expect(result.stdout_text.find("test-token-secret") ==
                            std::string::npos,
                        "模型列表不应泄露 API key") &&
                 Expect(result.stderr_text.empty(),
                        "models list 成功不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证添加模型时会询问并校验 timeout_seconds。
 *
 * 非法 timeout_seconds 应在真实 provider 测试调用前被拒绝，避免用户输入
 * 明显非法时还发起网络请求。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectInvalidAddModelTimeoutSeconds() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char add_model[] = "--add-model";
  char* argv[] = {program, add_model};

  const RunResult result = RunApplicationWithInput(
      2, argv,
      "local\n"
      "\n"
      "https://api.example.com\n"
      "example-model\n"
      "test-token\n"
      "-1\n"
      "n\n");

  return Expect(result.exit_code == 2,
                "非法 timeout_seconds 应返回用法错误") &&
                 ExpectContains(result.stderr_text, "timeout",
                                "交互提示应说明 timeout_seconds") &&
                 ExpectContains(result.stderr_text, "0",
                                "交互提示应说明 0 的含义") &&
                 ExpectContains(result.stderr_text, "Invalid model profile",
                                "非法 timeout_seconds 应输出模型配置错误") &&
                 Expect(result.stderr_text.find("Testing model call") ==
                            std::string::npos,
                        "非法 timeout_seconds 不应进入 provider 测试")
             ? 0
             : 1;
}

/**
 * @brief 验证默认模型设置命令会写入配置。
 *
 * @return 成功时返回 0。
 */
int ShouldRunModelsSetDefaultCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  WriteModelConfig(home);

  char program[] = "termtrans";
  char models[] = "models";
  char set_default[] = "set-default";
  char model[] = "local";
  char* argv[] = {program, models, set_default, model};

  const RunResult result = RunApplication(4, argv);

  return Expect(result.exit_code == 0, "models set-default 应返回 0") &&
                 ExpectContains(result.stdout_text, "local",
                                "成功提示应包含模型名称") &&
                 Expect(result.stderr_text.empty(),
                        "set-default 成功不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证历史列表命令会输出记录元数据。
 *
 * @return 成功时返回 0。
 */
int ShouldRunHistoryListCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  WriteHistoryRecord(home);

  char program[] = "termtrans";
  char history[] = "history";
  char list[] = "list";
  char* argv[] = {program, history, list};

  const RunResult result = RunApplication(3, argv);

  return Expect(result.exit_code == 0, "history list 应返回 0") &&
                 ExpectContains(result.stdout_text, "deepseek",
                                "历史列表应包含模型名称") &&
                 ExpectContains(result.stdout_text, "zh-CN",
                                "历史列表应包含目标语言") &&
                 Expect(result.stdout_text.find("hello") ==
                            std::string::npos,
                        "历史列表不应输出完整原文") &&
                 Expect(result.stdout_text.find("你好") ==
                            std::string::npos,
                        "历史列表不应输出完整译文") &&
                 Expect(result.stderr_text.empty(),
                        "history list 成功不应输出 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证历史清空命令会删除全部记录。
 *
 * @return 成功时返回 0。
 */
int ShouldRunHistoryClearCommand() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);
  WriteHistoryRecord(home);

  char program[] = "termtrans";
  char history[] = "history";
  char clear[] = "clear";
  char* argv[] = {program, history, clear};

  const RunResult result = RunApplication(3, argv);
  if (!Expect(result.exit_code == 0, "history clear 应返回 0") ||
      !ExpectContains(result.stdout_text, "cleared",
                      "history clear 成功提示应说明已清空") ||
      !Expect(result.stderr_text.empty(),
              "history clear 成功不应输出 stderr")) {
    return 1;
  }

  char list[] = "list";
  char* list_argv[] = {program, history, list};
  const RunResult list_result = RunApplication(3, list_argv);
  return Expect(list_result.exit_code == 0,
                "清空后 history list 应返回 0") &&
                 ExpectContains(list_result.stdout_text, "No history",
                                "清空后历史列表应为空")
             ? 0
             : 1;
}

/**
 * @brief 验证没有模型配置时翻译主流程返回清晰错误。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectTranslationWithoutModel() {
  const std::filesystem::path home = MakeTempDirectory();
  ConfigEnvironmentGuard config_environment_guard(home);

  char program[] = "termtrans";
  char text[] = "hello";
  char* argv[] = {program, text};

  const RunResult result = RunApplication(2, argv);

  return Expect(result.exit_code == 1, "缺少模型时翻译应返回 1") &&
                 ExpectContains(result.stderr_text, "model",
                                "缺少模型提示应包含 model")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行应用层分发测试。
 *
 * @return 任一用例失败时返回 1。
 */
int main() {
  if (ShouldRunConfigSetCommand() != 0) {
    return 1;
  }
  if (ShouldRejectInvalidConfigSetCommand() != 0) {
    return 1;
  }
  if (ShouldRunUiLanguagesListCommand() != 0) {
    return 1;
  }
  if (ShouldRunLanguagesListCommand() != 0) {
    return 1;
  }
  if (ShouldRunLanguagesShowCommand() != 0) {
    return 1;
  }
  if (ShouldRunLanguagesSetAndUnsetCommands() != 0) {
    return 1;
  }
  if (ShouldTreatMissingLanguageUnsetAsNoop() != 0) {
    return 1;
  }
  if (ShouldRejectMissingPromptFile() != 0) {
    return 1;
  }
  if (ShouldRunModelsListCommand() != 0) {
    return 1;
  }
  if (ShouldRejectInvalidAddModelTimeoutSeconds() != 0) {
    return 1;
  }
  if (ShouldRunModelsSetDefaultCommand() != 0) {
    return 1;
  }
  if (ShouldRunHistoryListCommand() != 0) {
    return 1;
  }
  if (ShouldRunHistoryClearCommand() != 0) {
    return 1;
  }
  if (ShouldRejectTranslationWithoutModel() != 0) {
    return 1;
  }

  return 0;
}
