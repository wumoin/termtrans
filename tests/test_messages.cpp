/**
 * @file test_messages.cpp
 * @brief 测试本地化用户可见文案。
 */

#include "i18n/messages.h"

#include <iostream>
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
 * @brief 调用本地化文案入口。
 *
 * @param id 文案标识。
 * @param args 文案插值参数。
 * @return 本地化后的文案文本。
 */
std::string Get(termtrans::i18n::MessageId id,
                const termtrans::i18n::MessageArgs& args = {}) {
  return termtrans::i18n::Messages::Get(id, args);
}

/**
 * @brief 验证文案入口按已解析界面语言选择中英文。
 *
 * @return 成功时返回 0。
 */
int ShouldUseResolvedLanguage() {
  if (!ExpectContains(Get(termtrans::i18n::MessageId::kHelpText,
                          {.language = "zh-CN"}),
                      "快速开始", "传入 zh-CN 时应使用中文文案")) {
    return 1;
  }

  if (!ExpectContains(
          Get(termtrans::i18n::MessageId::kHelpText, {.language = "en"}),
          "Usage", "传入 en 时应使用英文文案")) {
    return 1;
  }

  if (!ExpectContains(
          Get(termtrans::i18n::MessageId::kHelpText, {.language = "ja"}),
          "Usage", "i18n 收到未解析语言时应兜底英文")) {
    return 1;
  }

  return 0;
}

/**
 * @brief 验证帮助文本包含当前用户可见命令。
 *
 * @return 成功时返回 0。
 */
int ShouldExposeHelpTextCommands() {
  const std::string help =
      Get(termtrans::i18n::MessageId::kHelpText, {.language = "en"});

  return ExpectContains(help, "--to <language>",
                        "帮助文案应包含目标语言参数") &&
                 ExpectContains(help, "Quick Start",
                                "帮助文案应包含快速开始示例") &&
                 ExpectContains(help, "config set stream",
                                "帮助文案应包含 stream 配置命令") &&
                 ExpectContains(help, "ui-languages list",
                                "帮助文案应包含 UI 语言列表命令") &&
                 ExpectContains(help, "languages set <language> --file <file>",
                                "帮助文案应包含目标语言 prompt 写入命令") &&
                 ExpectContains(help, "models set-default <name>",
                                "帮助文案应包含默认模型命令") &&
                 ExpectContains(help, "history <list|clear>",
                                "帮助文案应包含历史命令用法") &&
                 ExpectContains(help, "config:",
                                "帮助文案应按 config 命令分组") &&
                 ExpectContains(help, "languages:",
                                "帮助文案应按 languages 命令分组") &&
                 ExpectContains(help, "models:",
                                "帮助文案应按 models 命令分组") &&
                 ExpectContains(help, "history:",
                                "帮助文案应按 history 命令分组") &&
                 ExpectContains(help, "termtrans --add-model",
                                "帮助文案应包含添加模型命令")
             ? 0
             : 1;
}

/**
 * @brief 验证帮助文本明确说明输入、输出、历史和文件规则。
 *
 * @return 成功时返回 0。
 */
int ShouldExplainHelpTextBehavior() {
  const std::string help =
      Get(termtrans::i18n::MessageId::kHelpText, {.language = "en"});

  return ExpectContains(help, "stdin is translated first",
                        "帮助文案应说明 stdin 优先") &&
                 ExpectContains(help, "exactly one direct text argument",
                                "帮助文案应说明只接受一个直接文本参数") &&
                 ExpectContains(help, "does not read files",
                                "帮助文案应说明不会读取文件路径") &&
                 ExpectContains(help, "termtrans \"hello world\"",
                                "帮助文案应包含多词文本加引号示例") &&
                 ExpectContains(help, "> README.zh.md",
                                "帮助文案应包含重定向保存示例") &&
                 ExpectContains(help, "less -R",
                                "帮助文案应包含分页器管道示例") &&
                 ExpectContains(help, "--reuse-history",
                                "帮助文案应包含历史复用示例或选项") &&
                 ExpectContains(help, "ask    Ask before retranslating",
                                "帮助文案应解释 ask 历史模式") &&
                 ExpectContains(help, "history.sqlite",
                                "帮助文案应说明历史数据库文件") &&
                 ExpectContains(help, "same directory as config.toml",
                                "帮助文案应说明历史文件与配置同目录") &&
                 ExpectContains(help, "stdout contains translated text only",
                                "帮助文案应说明 stdout 只输出译文") &&
                 ExpectContains(help, "stderr",
                                "帮助文案应说明诊断写入 stderr")
             ? 0
             : 1;
}

/**
 * @brief 验证帮助命令区为关键命令提供用途和关联操作说明。
 *
 * @return 成功时返回 0。
 */
int ShouldExplainHelpCommandRows() {
  const std::string english_help =
      Get(termtrans::i18n::MessageId::kHelpText, {.language = "en"});
  const std::string chinese_help =
      Get(termtrans::i18n::MessageId::kHelpText, {.language = "zh-CN"});

  return ExpectContains(chinese_help, "设置默认翻译语言",
                        "中文帮助应说明 target-language 的用途") &&
                 ExpectContains(chinese_help, "languages list",
                                "中文帮助应提示可列出支持语言") &&
                 ExpectContains(chinese_help, "languages set <语言> --file <文件>",
                                "中文帮助应提示可添加语言 prompt") &&
                 ExpectContains(chinese_help, "单个翻译分段的目标最大字节数",
                                "中文帮助应说明 max-input-bytes 控制分段上限") &&
                 ExpectContains(chinese_help, "0 表示不按用户上限分段",
                                "中文帮助应说明 max-input-bytes 的 0 语义") &&
                 ExpectContains(chinese_help, "保存前测试调用",
                                "中文帮助应说明添加模型会测试调用") &&
                 ExpectContains(chinese_help, "历史记录元数据",
                                "中文帮助应说明 history list 只列元数据") &&
                 ExpectContains(chinese_help, "清空本地历史记录",
                                "中文帮助应说明 history clear 的作用") &&
                 ExpectContains(english_help, "Set the default target language",
                                "英文帮助应说明 target-language 的用途") &&
                 ExpectContains(english_help, "languages list to see choices",
                                "英文帮助应提示可列出支持语言") &&
                 ExpectContains(english_help,
                                "languages set <language> --file <file>",
                                "英文帮助应提示可添加语言 prompt") &&
                 ExpectContains(english_help,
                                "maximum bytes per translation chunk",
                                "英文帮助应说明 max-input-bytes 控制分段上限") &&
                 ExpectContains(english_help, "0 means no user chunk limit",
                                "英文帮助应说明 max-input-bytes 的 0 语义") &&
                 ExpectContains(english_help, "test the call",
                                "英文帮助应说明添加模型会测试调用") &&
                 ExpectContains(english_help, "history metadata",
                                "英文帮助应说明 history list 只列元数据") &&
                 ExpectContains(english_help,
                                "Clear local translation history",
                                "英文帮助应说明 history clear 的作用")
             ? 0
             : 1;
}

/**
 * @brief 验证带插值参数的文案会包含关键上下文。
 *
 * @return 成功时返回 0。
 */
int ShouldFormatParameterizedMessages() {
  const bool is_valid =
      ExpectContains(Get(termtrans::i18n::MessageId::kUnknownArgument,
                         {.argument = "--bad", .language = "en"}),
                     "--bad", "未知参数文案应插入参数") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kMissingOptionValue,
                         {.argument = "--to", .language = "en"}),
                     "--to", "缺少取值文案应插入参数") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kInvalidUsage,
                         {.argument = "config", .language = "en"}),
                     "config", "用法错误文案应插入命令") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kConfigSetSuccess,
                         {.key = "stream",
                          .value = "false",
                          .path = "/tmp/config.toml",
                          .language = "zh-CN"}),
                     "/tmp/config.toml", "配置成功文案应插入路径") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kConfigInvalidValue,
                         {.key = "ui-language",
                          .value = "ja",
                          .language = "en"}),
                     "ja", "非法配置文案应插入配置值") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kHistoryReadFailed,
                         {.argument = "sqlite error", .language = "en"}),
                     "sqlite error", "历史读取失败文案应插入错误摘要") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kHistoryWriteFailed,
                         {.argument = "readonly", .language = "en"}),
                     "readonly", "历史写入失败文案应插入错误摘要") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kLanguageSetSuccess,
                         {.argument = "pirate",
                          .path = "/tmp/config.toml",
                          .language = "en"}),
                     "pirate", "目标语言设置成功文案应插入语言 code") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kLanguagePromptHeader,
                         {.argument = "zh-CN",
                          .value = "builtin",
                          .language = "zh-CN"}),
                     "builtin", "prompt 展示头应插入来源") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kPromptFileReadFailed,
                         {.path = "/tmp/prompt.txt", .language = "en"}),
                     "/tmp/prompt.txt", "prompt 文件读取失败文案应插入路径") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kModelNotFound,
                         {.argument = "deepseek", .language = "en"}),
                     "deepseek", "模型不存在文案应插入模型名称") &&
      ExpectContains(Get(termtrans::i18n::MessageId::kTranslationFailed,
                         {.argument = "provider failed", .language = "en"}),
                     "provider failed", "翻译失败文案应插入错误摘要");

  return is_valid ? 0 : 1;
}

/**
 * @brief 验证 UI 语言列表文案包含所有支持语言。
 *
 * @return 成功时返回 0。
 */
int ShouldListUiLanguages() {
  const std::string text =
      Get(termtrans::i18n::MessageId::kUiLanguagesList, {.language = "en"});

  return ExpectContains(text, "zh-CN", "UI 语言列表应包含 zh-CN") &&
                 ExpectContains(text, "en", "UI 语言列表应包含 en")
             ? 0
             : 1;
}

/**
 * @brief 验证添加模型超时提示会解释单位、默认值和 0 的语义。
 *
 * @return 成功时返回 0。
 */
int ShouldExplainAddModelTimeoutPrompt() {
  const std::string english_prompt =
      Get(termtrans::i18n::MessageId::kModelAddPromptTimeoutSeconds,
          {.language = "en"});
  const std::string chinese_prompt =
      Get(termtrans::i18n::MessageId::kModelAddPromptTimeoutSeconds,
          {.language = "zh-CN"});

  return ExpectContains(english_prompt, "seconds",
                        "英文 timeout 提示应说明单位") &&
                 ExpectContains(english_prompt, "empty keeps 60",
                                "英文 timeout 提示应说明默认值") &&
                 ExpectContains(english_prompt, "0 means no request timeout",
                                "英文 timeout 提示应说明 0 语义") &&
                 ExpectContains(chinese_prompt, "秒",
                                "中文 timeout 提示应说明单位") &&
                 ExpectContains(chinese_prompt, "留空默认 60",
                                "中文 timeout 提示应说明默认值") &&
                 ExpectContains(chinese_prompt, "0 表示不设置请求超时上限",
                                "中文 timeout 提示应说明 0 语义")
             ? 0
             : 1;
}

/**
 * @brief 验证每个文案标识都有中英文兜底文本。
 *
 * 新增 MessageId 时必须同步更新该测试，防止业务流程拿到空字符串。
 *
 * @return 成功时返回 0。
 */
int ShouldReturnNonEmptyTextForEveryMessage() {
  const termtrans::i18n::MessageId ids[] = {
      termtrans::i18n::MessageId::kHelpText,
      termtrans::i18n::MessageId::kUnknownArgument,
      termtrans::i18n::MessageId::kMissingOptionValue,
      termtrans::i18n::MessageId::kInvalidUsage,
      termtrans::i18n::MessageId::kEmptyInput,
      termtrans::i18n::MessageId::kTranslationUnavailable,
      termtrans::i18n::MessageId::kConfigSetSuccess,
      termtrans::i18n::MessageId::kConfigInvalidValue,
      termtrans::i18n::MessageId::kConfigWriteFailed,
      termtrans::i18n::MessageId::kConfigReadFailed,
      termtrans::i18n::MessageId::kUiLanguagesList,
      termtrans::i18n::MessageId::kLanguagesListHeader,
      termtrans::i18n::MessageId::kLanguagePromptHeader,
      termtrans::i18n::MessageId::kLanguageSetSuccess,
      termtrans::i18n::MessageId::kLanguageUnsetSuccess,
      termtrans::i18n::MessageId::kLanguageUnsetNoConfig,
      termtrans::i18n::MessageId::kLanguageNotFound,
      termtrans::i18n::MessageId::kPromptFileReadFailed,
      termtrans::i18n::MessageId::kPromptInvalidValue,
      termtrans::i18n::MessageId::kModelsListHeader,
      termtrans::i18n::MessageId::kModelsListEmpty,
      termtrans::i18n::MessageId::kModelsSetDefaultSuccess,
      termtrans::i18n::MessageId::kModelNotFound,
      termtrans::i18n::MessageId::kModelInvalidValue,
      termtrans::i18n::MessageId::kModelAddPromptName,
      termtrans::i18n::MessageId::kModelAddPromptProviderType,
      termtrans::i18n::MessageId::kModelAddPromptBaseUrl,
      termtrans::i18n::MessageId::kModelAddPromptModelId,
      termtrans::i18n::MessageId::kModelAddPromptApiKey,
      termtrans::i18n::MessageId::kModelAddPromptTimeoutSeconds,
      termtrans::i18n::MessageId::kModelAddPromptSetDefault,
      termtrans::i18n::MessageId::kModelAddTesting,
      termtrans::i18n::MessageId::kModelAddSuccess,
      termtrans::i18n::MessageId::kModelAddFailed,
      termtrans::i18n::MessageId::kDefaultModelMissing,
      termtrans::i18n::MessageId::kInputReadFailed,
      termtrans::i18n::MessageId::kTranslationFailed,
      termtrans::i18n::MessageId::kTranslationProgress,
      termtrans::i18n::MessageId::kOutputCancelled,
      termtrans::i18n::MessageId::kHistoryHitPrompt,
      termtrans::i18n::MessageId::kHistoryListHeader,
      termtrans::i18n::MessageId::kHistoryListEmpty,
      termtrans::i18n::MessageId::kHistoryClearSuccess,
      termtrans::i18n::MessageId::kHistoryReadFailed,
      termtrans::i18n::MessageId::kHistoryWriteFailed,
  };

  for (const termtrans::i18n::MessageId id : ids) {
    if (!Expect(!Get(id, {.argument = "arg",
                         .key = "stream",
                         .value = "false",
                         .path = "/tmp/config.toml",
                         .language = "en"})
                     .empty(),
                "每个文案标识都应返回非空英文文案")) {
      return 1;
    }
    if (!Expect(!Get(id, {.argument = "arg",
                         .key = "stream",
                         .value = "false",
                         .path = "/tmp/config.toml",
                         .language = "zh-CN"})
                     .empty(),
                "每个文案标识都应返回非空中文文案")) {
      return 1;
    }
  }

  return 0;
}

}  // 匿名命名空间

/**
 * @brief 运行本地化文案测试。
 *
 * @return 任一用例失败时返回 1。
 */
int main() {
  if (ShouldUseResolvedLanguage() != 0) {
    return 1;
  }
  if (ShouldExposeHelpTextCommands() != 0) {
    return 1;
  }
  if (ShouldExplainHelpTextBehavior() != 0) {
    return 1;
  }
  if (ShouldExplainHelpCommandRows() != 0) {
    return 1;
  }
  if (ShouldFormatParameterizedMessages() != 0) {
    return 1;
  }
  if (ShouldListUiLanguages() != 0) {
    return 1;
  }
  if (ShouldExplainAddModelTimeoutPrompt() != 0) {
    return 1;
  }
  if (ShouldReturnNonEmptyTextForEveryMessage() != 0) {
    return 1;
  }

  return 0;
}
