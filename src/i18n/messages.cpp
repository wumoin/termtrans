/**
 * @file messages.cpp
 * @brief 实现本地化用户可见文案。
 */

#include "i18n/messages.h"

#include "config/app_constants.h"

#include <string>

namespace termtrans::i18n {
namespace {

/**
 * @brief 判断是否使用中文终端文案。
 *
 * 上游配置解析已经保证合法 UI 语言只有 zh-CN 和 en；这里仍对非 zh-CN
 * 值统一回退英文，避免调用方传入未解析语言时输出空文案。
 *
 * @param language 已解析或待兜底的界面语言。
 * @return 仅 zh-CN 返回 true。
 */
bool ShouldUseChinese(std::string_view language) {
  return language == "zh-CN";
}

/**
 * @brief 生成配置写入相关文案中的配置项描述。
 *
 * 部分配置错误可能只携带原始值，不携带键名；该函数统一处理两种
 * 插值形态。
 *
 * @param args 文案插值参数。
 * @return `key = value` 或单独的 value 文本。
 */
std::string ConfigSetSubject(const MessageArgs& args) {
  if (!args.key.empty()) {
    return std::string(args.key) + " = " + std::string(args.value);
  }

  return std::string(args.value);
}

}  // 匿名命名空间

/**
 * @brief 按文案标识和界面语言返回用户可见字符串。
 *
 * 该函数只做静态文案选择和少量字符串插值，不读取配置、不探测系统
 * 语言。未识别的界面语言统一使用英文兜底。
 *
 * @param id 文案标识。
 * @param args 文案插值参数。
 * @return 本地化后的文案；所有合法 MessageId 都应返回非空字符串。
 */
std::string Messages::Get(MessageId id, const MessageArgs& args) {
  if (ShouldUseChinese(args.language)) {
    switch (id) {
      case MessageId::kHelpText:
        return std::string(config::constants::app::kApplicationName) +
               " - Unix filter 风格的终端 AI 翻译工具，只输出译文。\n\n"
               "快速开始:\n"
               "  man git | col -b | termtrans\n"
               "  termtrans \"hello world\"\n"
               "  cat README.md | termtrans --to zh-CN > README.zh.md\n"
               "  man git | col -b | termtrans | less -R\n"
               "  cat README.md | termtrans --no-stream\n"
               "  cat README.md | termtrans --reuse-history\n"
               "  cat README.md | termtrans --force\n\n"
               "用法:\n"
               "  termtrans [翻译选项] [文本]\n"
               "  termtrans config set <键> <值>\n"
               "  termtrans languages <list|show|set|unset> ...\n"
               "  termtrans models <list|set-default>\n"
               "  termtrans history <list|clear>\n"
               "  termtrans ui-languages list\n\n"
               "输入:\n"
               "  - stdin 有管道输入时，优先翻译 stdin。\n"
               "  - 没有 stdin 时，只接受一个直接文本参数。\n"
               "  - termtrans README.md 会翻译字面文本 README.md，不读取文件。\n"
               "  - 多词直接文本必须加引号，例如 termtrans \"hello world\"。\n\n"
               "输出:\n"
               "  - stdout 只写译文，适合重定向保存或继续接管道。\n"
               "  - 错误、交互提示和非流式进度写入 stderr。\n"
               "  - 分页、搜索和保存交给 shell，例如 termtrans | less -R。\n\n"
               "翻译选项:\n"
               "  --to <语言>             本次目标语言\n"
               "  --model <名称>          本次使用的本地模型配置\n"
               "  --stream                流式输出译文\n"
               "  --no-stream             完成后一次性输出译文\n"
               "  --force                 跳过历史并重新翻译，成功后保存历史\n"
               "  --reuse-history         命中历史时直接复用\n"
               "  --no-history            禁用历史读取和写入\n"
               "  --add-model             交互式添加模型配置\n\n"
               "命令:\n"
               "  config:\n"
               "    termtrans config set target-language <语言>\n"
               "      设置默认翻译语言；可用 languages list 查看支持语言，\n"
               "      也可用 languages set <语言> --file <文件> 添加 prompt。\n"
               "    termtrans config set ui-language <zh-CN|en>\n"
               "      设置终端界面语言。\n"
               "    termtrans config set stream <true|false>\n"
               "      设置默认是否流式输出译文。\n"
               "    termtrans config set history-mode <ask|reuse|force|off>\n"
               "      设置默认历史策略。\n"
               "    termtrans config set max-input-bytes <非负整数>\n"
               "      设置单个翻译分段的目标最大字节数；0 表示不按用户上限分段。\n"
               "  languages:\n"
               "    termtrans languages list\n"
               "      列出可用目标语言和 prompt 来源。\n"
               "    termtrans languages show <语言>\n"
               "      查看目标语言当前生效的完整 prompt。\n"
               "    termtrans languages set <语言> --file <文件>\n"
               "      从文件添加自定义语言 prompt 或覆盖内置 prompt。\n"
               "    termtrans languages unset <语言>\n"
               "      删除配置 prompt；内置语言会恢复默认 prompt。\n"
               "  models:\n"
               "    termtrans --add-model\n"
               "      交互式添加模型配置，并在保存前测试调用。\n"
               "    termtrans models list\n"
               "      列出本地模型配置，不显示 API key。\n"
               "    termtrans models set-default <名称>\n"
               "      设置默认模型配置。\n"
               "  history:\n"
               "    termtrans history list\n"
               "      列出历史记录元数据，不输出完整原文或译文。\n"
               "    termtrans history clear\n"
               "      清空本地历史记录。\n"
               "  ui-languages:\n"
               "    termtrans ui-languages list\n"
               "      列出支持的终端界面语言。\n\n"
               "历史模式:\n"
               "  ask    命中历史时询问；无法交互时直接复用\n"
               "  reuse  命中历史时直接复用\n"
               "  force  忽略历史译文并重新翻译\n"
               "  off    不读取也不写入历史\n\n"
               "文件:\n"
               "  config.toml 保存配置和模型 API key。\n"
               "  history.sqlite 保存历史记录，并与 config.toml 位于同一目录。\n";
      case MessageId::kUnknownArgument:
        return "未知参数: " + std::string(args.argument);
      case MessageId::kMissingOptionValue:
        return "参数缺少取值: " + std::string(args.argument);
      case MessageId::kInvalidUsage:
        return "命令用法错误: " + std::string(args.argument);
      case MessageId::kEmptyInput:
        return "当前没有输入。请通过 stdin 管道输入文本，或传入一个"
               "直接文本参数。";
      case MessageId::kTranslationUnavailable:
        return "翻译主流程将在后续阶段实现。";
      case MessageId::kConfigSetSuccess:
        return "已写入配置: " + ConfigSetSubject(args) +
               "\n配置文件: " + std::string(args.path);
      case MessageId::kConfigInvalidValue:
        return "非法配置值: " + ConfigSetSubject(args);
      case MessageId::kConfigWriteFailed:
        return "写入配置文件失败: " + std::string(args.path);
      case MessageId::kConfigReadFailed:
        return "读取配置文件失败: " + std::string(args.path);
      case MessageId::kUiLanguagesList:
        return "支持的终端界面语言:\nzh-CN\nen";
      case MessageId::kLanguagesListHeader:
        return "可用目标语言:\nCODE\tNAME\tSOURCE";
      case MessageId::kLanguagePromptHeader:
        return "目标语言: " + std::string(args.argument) +
               "\nPrompt 来源: " + std::string(args.value) + "\n";
      case MessageId::kLanguageSetSuccess:
        return "已写入目标语言 prompt: " + std::string(args.argument) +
               "\n配置文件: " + std::string(args.path);
      case MessageId::kLanguageUnsetSuccess:
        return "已删除目标语言配置 prompt: " + std::string(args.argument) +
               "\n配置文件: " + std::string(args.path);
      case MessageId::kLanguageUnsetNoConfig:
        return "没有可删除的目标语言配置 prompt: " +
               std::string(args.argument);
      case MessageId::kLanguageNotFound:
        return "目标语言不可用: " + std::string(args.argument);
      case MessageId::kPromptFileReadFailed:
        return "读取 prompt 文件失败: " + std::string(args.path);
      case MessageId::kPromptInvalidValue:
        return "非法 prompt 配置: " + std::string(args.argument);
      case MessageId::kModelsListHeader:
        return "模型配置:\nNAME\tTYPE\tMODEL_ID\tDEFAULT";
      case MessageId::kModelsListEmpty:
        return "尚未配置模型。请运行 termtrans --add-model。";
      case MessageId::kModelsSetDefaultSuccess:
        return "已设置默认模型: " + std::string(args.argument) +
               "\n配置文件: " + std::string(args.path);
      case MessageId::kModelNotFound:
        return "模型配置不存在: " + std::string(args.argument);
      case MessageId::kModelInvalidValue:
        return "非法模型配置: " + std::string(args.argument);
      case MessageId::kModelAddPromptName:
        return "模型名称: ";
      case MessageId::kModelAddPromptProviderType:
        return "Provider 类型 [默认 openai-compatible]: ";
      case MessageId::kModelAddPromptBaseUrl:
        return "Base URL: ";
      case MessageId::kModelAddPromptModelId:
        return "模型 ID: ";
      case MessageId::kModelAddPromptApiKey:
        return "API key: ";
      case MessageId::kModelAddPromptTimeoutSeconds:
        return "请求超时时间（秒，控制单次 provider HTTP 请求最多等待多久；"
               "留空默认 60；0 表示不设置请求超时上限）: ";
      case MessageId::kModelAddPromptSetDefault:
        return "设为默认模型？[y/N]: ";
      case MessageId::kModelAddTesting:
        return "正在测试模型调用...";
      case MessageId::kModelAddSuccess:
        return "已添加模型: " + std::string(args.argument) +
               "\n配置文件: " + std::string(args.path);
      case MessageId::kModelAddFailed:
        return "添加模型失败: " + std::string(args.argument);
      case MessageId::kDefaultModelMissing:
        return "没有可用默认模型。请先运行 termtrans --add-model。";
      case MessageId::kInputReadFailed:
        return "读取输入失败。";
      case MessageId::kTranslationFailed:
        return "翻译失败: " + std::string(args.argument);
      case MessageId::kTranslationProgress:
        return "正在翻译...";
      case MessageId::kOutputCancelled:
        return "输出已取消。";
      case MessageId::kHistoryHitPrompt:
        return "命中历史记录，重新翻译？[y/N]: ";
      case MessageId::kHistoryListHeader:
        return "历史记录:\nUPDATED_AT\tTARGET\tINPUT_BYTES\tOUTPUT_BYTES\tMODEL\tINPUT_SHA256";
      case MessageId::kHistoryListEmpty:
        return "暂无历史记录。";
      case MessageId::kHistoryClearSuccess:
        return "已清空历史记录。";
      case MessageId::kHistoryReadFailed:
        return "读取历史记录失败: " + std::string(args.argument);
      case MessageId::kHistoryWriteFailed:
        return "写入历史记录失败: " + std::string(args.argument);
    }

    return "";
  }

  switch (id) {
    case MessageId::kHelpText:
      return std::string(config::constants::app::kApplicationName) +
             " - Unix filter style terminal AI translator. It only writes "
             "translated text.\n\n"
             "Quick Start:\n"
             "  man git | col -b | termtrans\n"
             "  termtrans \"hello world\"\n"
             "  cat README.md | termtrans --to zh-CN > README.zh.md\n"
             "  man git | col -b | termtrans | less -R\n"
             "  cat README.md | termtrans --no-stream\n"
             "  cat README.md | termtrans --reuse-history\n"
             "  cat README.md | termtrans --force\n\n"
             "Usage:\n"
             "  termtrans [translation options] [text]\n"
             "  termtrans config set <key> <value>\n"
             "  termtrans languages <list|show|set|unset> ...\n"
             "  termtrans models <list|set-default>\n"
             "  termtrans history <list|clear>\n"
             "  termtrans ui-languages list\n\n"
             "Input:\n"
             "  - If stdin has piped input, stdin is translated first.\n"
             "  - Without stdin, exactly one direct text argument is accepted.\n"
             "  - termtrans README.md translates literal text README.md; it "
             "does not read files.\n"
             "  - Quote multi-word direct text, for example "
             "termtrans \"hello world\".\n\n"
             "Output:\n"
             "  - stdout contains translated text only, suitable for "
             "redirection or pipes.\n"
             "  - Errors, prompts, and non-stream progress go to stderr.\n"
             "  - Paging, search, and saving are handled by the shell, for "
             "example termtrans | less -R.\n\n"
             "Translation options:\n"
             "  --to <language>          Target language for this run\n"
             "  --model <name>           Named local model profile for this run\n"
             "  --stream                 Stream translated text to stdout\n"
             "  --no-stream              Write translated text after completion\n"
             "  --force                  Skip history and translate again; save "
             "on success\n"
             "  --reuse-history          Reuse a history hit directly\n"
             "  --no-history             Disable history reads and writes\n"
             "  --add-model              Add a model profile interactively\n\n"
             "Commands:\n"
             "  config:\n"
             "    termtrans config set target-language <language>\n"
             "      Set the default target language. Use languages list to "
             "see choices,\n"
             "      or languages set <language> --file <file> to add a "
             "prompt.\n"
             "    termtrans config set ui-language <zh-CN|en>\n"
             "      Set the terminal UI language.\n"
             "    termtrans config set stream <true|false>\n"
             "      Set whether translations stream by default.\n"
             "    termtrans config set history-mode <ask|reuse|force|off>\n"
             "      Set the default history policy.\n"
             "    termtrans config set max-input-bytes <non-negative integer>\n"
             "      Set the target maximum bytes per translation chunk; 0 "
             "means no user chunk limit.\n"
             "  languages:\n"
             "    termtrans languages list\n"
             "      List target languages and prompt sources.\n"
             "    termtrans languages show <language>\n"
             "      Show the full active prompt for a target language.\n"
             "    termtrans languages set <language> --file <file>\n"
             "      Add a custom prompt or override a built-in prompt.\n"
             "    termtrans languages unset <language>\n"
             "      Remove configured prompt; built-in languages return to "
             "defaults.\n"
             "  models:\n"
             "    termtrans --add-model\n"
             "      Add a model profile interactively and test the call.\n"
             "    termtrans models list\n"
             "      List local model profiles without API keys.\n"
             "    termtrans models set-default <name>\n"
             "      Set the default model profile.\n"
             "  history:\n"
             "    termtrans history list\n"
             "      List history metadata, not full source or translated text.\n"
             "    termtrans history clear\n"
             "      Clear local translation history.\n"
             "  ui-languages:\n"
             "    termtrans ui-languages list\n"
             "      List supported terminal UI languages.\n\n"
             "History modes:\n"
             "  ask    Ask before retranslating a history hit; reuse when "
             "non-interactive\n"
             "  reuse  Reuse a history hit directly\n"
             "  force  Ignore history text and translate again\n"
             "  off    Do not read or write history\n\n"
             "Files:\n"
             "  config.toml stores config and model API keys.\n"
             "  history.sqlite stores translation history in the same directory "
             "as config.toml.\n";
    case MessageId::kUnknownArgument:
      return "Unknown argument: " + std::string(args.argument);
    case MessageId::kMissingOptionValue:
      return "Missing value for option: " + std::string(args.argument);
    case MessageId::kInvalidUsage:
      return "Invalid command usage: " + std::string(args.argument);
    case MessageId::kEmptyInput:
      return "No input was provided. Pipe text through stdin or pass one direct "
             "text argument.";
    case MessageId::kTranslationUnavailable:
      return "The translation flow will be implemented in a later phase.";
    case MessageId::kConfigSetSuccess:
      return "Config updated: " + ConfigSetSubject(args) +
             "\nConfig file: " + std::string(args.path);
    case MessageId::kConfigInvalidValue:
      return "Invalid config value: " + ConfigSetSubject(args);
    case MessageId::kConfigWriteFailed:
      return "Failed to write config file: " + std::string(args.path);
    case MessageId::kConfigReadFailed:
      return "Failed to read config file: " + std::string(args.path);
    case MessageId::kUiLanguagesList:
      return "Supported UI languages:\nzh-CN\nen";
    case MessageId::kLanguagesListHeader:
      return "Available target languages:\nCODE\tNAME\tSOURCE";
    case MessageId::kLanguagePromptHeader:
      return "Target language: " + std::string(args.argument) +
             "\nPrompt source: " + std::string(args.value) + "\n";
    case MessageId::kLanguageSetSuccess:
      return "Target language prompt updated: " + std::string(args.argument) +
             "\nConfig file: " + std::string(args.path);
    case MessageId::kLanguageUnsetSuccess:
      return "Target language prompt config removed: " +
             std::string(args.argument) + "\nConfig file: " +
             std::string(args.path);
    case MessageId::kLanguageUnsetNoConfig:
      return "No target language prompt config to remove: " +
             std::string(args.argument);
    case MessageId::kLanguageNotFound:
      return "Target language is not available: " + std::string(args.argument);
    case MessageId::kPromptFileReadFailed:
      return "Failed to read prompt file: " + std::string(args.path);
    case MessageId::kPromptInvalidValue:
      return "Invalid prompt config: " + std::string(args.argument);
    case MessageId::kModelsListHeader:
      return "Model profiles:\nNAME\tTYPE\tMODEL_ID\tDEFAULT";
    case MessageId::kModelsListEmpty:
      return "No model profiles are configured. Run termtrans --add-model.";
    case MessageId::kModelsSetDefaultSuccess:
      return "Default model updated: " + std::string(args.argument) +
             "\nConfig file: " + std::string(args.path);
    case MessageId::kModelNotFound:
      return "Model profile not found: " + std::string(args.argument);
    case MessageId::kModelInvalidValue:
      return "Invalid model profile: " + std::string(args.argument);
    case MessageId::kModelAddPromptName:
      return "Model name: ";
    case MessageId::kModelAddPromptProviderType:
      return "Provider type [default openai-compatible]: ";
    case MessageId::kModelAddPromptBaseUrl:
      return "Base URL: ";
    case MessageId::kModelAddPromptModelId:
      return "Model ID: ";
    case MessageId::kModelAddPromptApiKey:
      return "API key: ";
    case MessageId::kModelAddPromptTimeoutSeconds:
      return "Request timeout seconds (limits one provider HTTP request; "
             "empty keeps 60; 0 means no request timeout limit): ";
    case MessageId::kModelAddPromptSetDefault:
      return "Set as default model? [y/N]: ";
    case MessageId::kModelAddTesting:
      return "Testing model call...";
    case MessageId::kModelAddSuccess:
      return "Model profile added: " + std::string(args.argument) +
             "\nConfig file: " + std::string(args.path);
    case MessageId::kModelAddFailed:
      return "Failed to add model profile: " + std::string(args.argument);
    case MessageId::kDefaultModelMissing:
      return "No default model is available. Run termtrans --add-model first.";
    case MessageId::kInputReadFailed:
      return "Failed to read input.";
    case MessageId::kTranslationFailed:
      return "Translation failed: " + std::string(args.argument);
    case MessageId::kTranslationProgress:
      return "Translating...";
    case MessageId::kOutputCancelled:
      return "Output was cancelled.";
    case MessageId::kHistoryHitPrompt:
      return "History hit. Translate again? [y/N]: ";
    case MessageId::kHistoryListHeader:
      return "History records:\nUPDATED_AT\tTARGET\tINPUT_BYTES\tOUTPUT_BYTES\tMODEL\tINPUT_SHA256";
    case MessageId::kHistoryListEmpty:
      return "No history records.";
    case MessageId::kHistoryClearSuccess:
      return "History records cleared.";
    case MessageId::kHistoryReadFailed:
      return "Failed to read history: " + std::string(args.argument);
    case MessageId::kHistoryWriteFailed:
      return "Failed to write history: " + std::string(args.argument);
  }

  return "";
}

}  // 命名空间 termtrans::i18n
