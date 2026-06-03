/**
 * @file test_prompt_registry.cpp
 * @brief 测试目标语言 prompt 注册表。
 */

#include "prompt/prompt_registry.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

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
 * @brief 验证内置语言表包含第一版要求的目标语言。
 *
 * @return 成功时返回 0。
 */
int ShouldExposeBuiltinLanguages() {
  const termtrans::config::AppConfig config;
  const termtrans::prompt::PromptRegistry registry(config);
  const std::vector<termtrans::prompt::PromptEntry> entries =
      registry.ListLanguages();
  const std::optional<termtrans::prompt::PromptEntry> zh_cn =
      registry.Resolve("zh-CN");
  const std::optional<termtrans::prompt::PromptEntry> en =
      registry.Resolve("en");
  const std::optional<termtrans::prompt::PromptEntry> ar =
      registry.Resolve("ar");
  const std::optional<termtrans::prompt::PromptEntry> bn =
      registry.Resolve("bn");

  return Expect(entries.size() == 25, "内置目标语言数量应匹配设计文档") &&
                 Expect(registry.HasBuiltinLanguage("zh-CN"),
                        "zh-CN 应是内置语言") &&
                 Expect(registry.HasBuiltinLanguage("he"),
                        "he 应是内置语言") &&
                 Expect(!registry.HasBuiltinLanguage("pirate"),
                        "pirate 不应是内置语言") &&
                 Expect(zh_cn.has_value(), "zh-CN 应可解析") &&
                 Expect(en.has_value(), "en 应可解析") &&
                 Expect(ar.has_value(), "ar 应可解析") &&
                 Expect(bn.has_value(), "bn 应可解析") &&
                 Expect(zh_cn->source ==
                            termtrans::prompt::PromptSource::kBuiltin,
                        "默认 zh-CN prompt 来源应是 builtin") &&
                 ExpectContains(zh_cn->prompt, "只输出译文",
                                "中文 prompt 应包含只输出译文约束") &&
                 ExpectContains(zh_cn->prompt, "段落顺序",
                                "中文 prompt 应要求保留段落顺序") &&
                 ExpectContains(zh_cn->prompt, "空行",
                                "中文 prompt 应要求保留空行") &&
                 ExpectContains(zh_cn->prompt, "明显换行",
                                "中文 prompt 应要求保留明显换行") &&
                 ExpectContains(zh_cn->prompt, "缩进、表格和终端布局",
                                "中文 prompt 应要求保留终端结构") &&
                 ExpectContains(zh_cn->prompt, "代码块、命令、参数名",
                                "中文 prompt 应要求保留代码和命令") &&
                 ExpectContains(zh_cn->prompt, "路径、URL、占位符",
                                "中文 prompt 应要求保留路径和占位符") &&
                 ExpectContains(zh_cn->prompt, "Markdown 结构",
                                "中文 prompt 应要求保留 Markdown 结构") &&
                 ExpectContains(zh_cn->prompt, "普通文本可以译得自然",
                                "中文 prompt 应允许普通文本自然翻译") &&
                 ExpectContains(zh_cn->prompt, "不要主动合并段落",
                                "中文 prompt 应禁止主动合并段落") &&
                 ExpectContains(en->prompt, "Only output",
                                "英文 prompt 应包含只输出译文约束") &&
                 ExpectContains(en->prompt, "paragraph order",
                                "英文 prompt 应要求保留段落顺序") &&
                 ExpectContains(en->prompt, "blank lines",
                                "英文 prompt 应要求保留空行") &&
                 ExpectContains(en->prompt, "hard line breaks",
                                "英文 prompt 应要求保留硬换行") &&
                 ExpectContains(en->prompt, "indentation, tables, and terminal layout",
                                "英文 prompt 应要求保留终端结构") &&
                 ExpectContains(en->prompt, "code blocks, commands, option names",
                                "英文 prompt 应要求保留代码和命令") &&
                 ExpectContains(en->prompt, "paths, URLs, placeholders",
                                "英文 prompt 应要求保留路径和占位符") &&
                 ExpectContains(en->prompt, "Markdown structure",
                                "英文 prompt 应要求保留 Markdown 结构") &&
                 ExpectContains(en->prompt, "ordinary prose read naturally",
                                "英文 prompt 应允许普通文本自然翻译") &&
                 ExpectContains(en->prompt, "do not merge paragraphs",
                                "英文 prompt 应禁止主动合并段落") &&
                 ExpectContains(ar->prompt, "العربية",
                                "阿拉伯语 prompt 应使用阿拉伯语书写") &&
                 ExpectContains(bn->prompt, "বাংলায়",
                                "孟加拉语 prompt 应使用孟加拉语书写")
             ? 0
             : 1;
}

/**
 * @brief 验证配置 prompt 会覆盖同名内置语言。
 *
 * @return 成功时返回 0。
 */
int ShouldOverrideBuiltinPromptFromConfig() {
  termtrans::config::AppConfig config;
  config.prompts["zh-CN"] = "custom zh prompt";

  const termtrans::prompt::PromptRegistry registry(config);
  const std::optional<termtrans::prompt::PromptEntry> entry =
      registry.Resolve("zh-CN");

  return Expect(entry.has_value(), "覆盖后的 zh-CN 应可解析") &&
                 Expect(entry->prompt == "custom zh prompt",
                        "配置 prompt 应覆盖内置 prompt") &&
                 Expect(entry->source ==
                            termtrans::prompt::PromptSource::kConfig,
                        "覆盖后的 prompt 来源应是 config") &&
                 Expect(entry->display_name == "简体中文",
                        "覆盖内置语言时展示名应保留内置名称") &&
                 Expect(entry->is_builtin, "覆盖项仍应标记为内置语言")
             ? 0
             : 1;
}

/**
 * @brief 验证非内置配置 prompt 会新增自定义目标语言。
 *
 * @return 成功时返回 0。
 */
int ShouldAddCustomLanguageFromConfig() {
  termtrans::config::AppConfig config;
  config.prompts["pirate"] = "Translate into pirate English.";

  const termtrans::prompt::PromptRegistry registry(config);
  const std::optional<termtrans::prompt::PromptEntry> entry =
      registry.Resolve("pirate");

  return Expect(entry.has_value(), "自定义语言应可解析") &&
                 Expect(entry->prompt == "Translate into pirate English.",
                        "自定义语言应使用配置 prompt") &&
                 Expect(entry->display_name == "pirate",
                        "自定义语言展示名默认等于 code") &&
                 Expect(entry->source ==
                            termtrans::prompt::PromptSource::kConfig,
                        "自定义语言来源应是 config") &&
                 Expect(!entry->is_builtin,
                        "自定义语言不应标记为内置语言")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言列表按 code 稳定排序。
 *
 * @return 成功时返回 0。
 */
int ShouldListLanguagesInStableOrder() {
  termtrans::config::AppConfig config;
  config.prompts["aaa-custom"] = "Custom prompt.";

  const termtrans::prompt::PromptRegistry registry(config);
  const std::vector<termtrans::prompt::PromptEntry> entries =
      registry.ListLanguages();

  for (std::size_t index = 1; index < entries.size(); ++index) {
    if (!Expect(entries[index - 1].code < entries[index].code,
                "目标语言列表应按 code 递增排序")) {
      return 1;
    }
  }

  return Expect(entries.front().code == "aaa-custom",
                "自定义语言也应参与同一排序")
             ? 0
             : 1;
}

/**
 * @brief 验证 prompt 来源展示名称稳定。
 *
 * @return 成功时返回 0。
 */
int ShouldExposePromptSourceNames() {
  return Expect(termtrans::prompt::PromptSourceName(
                    termtrans::prompt::PromptSource::kBuiltin) == "builtin",
                "builtin 来源名称应稳定") &&
                 Expect(termtrans::prompt::PromptSourceName(
                            termtrans::prompt::PromptSource::kConfig) ==
                            "config",
                        "config 来源名称应稳定")
             ? 0
             : 1;
}

}  // namespace

/**
 * @brief 运行目标语言 prompt 注册表测试。
 *
 * @return 任一用例失败时返回 1。
 */
int main() {
  if (ShouldExposeBuiltinLanguages() != 0) {
    return 1;
  }
  if (ShouldOverrideBuiltinPromptFromConfig() != 0) {
    return 1;
  }
  if (ShouldAddCustomLanguageFromConfig() != 0) {
    return 1;
  }
  if (ShouldListLanguagesInStableOrder() != 0) {
    return 1;
  }
  if (ShouldExposePromptSourceNames() != 0) {
    return 1;
  }

  return 0;
}
