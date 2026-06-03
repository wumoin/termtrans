/**
 * @file messages.h
 * @brief 声明本地化用户可见文案。
 */

#pragma once

#include <string>
#include <string_view>

namespace termtrans::i18n {

/**
 * @brief 用户可见文案的稳定标识。
 *
 * 业务代码通过枚举选择文案，避免直接依赖某个语言表，或把用户可见
 * 字符串散落在流程中。
 */
enum class MessageId {
  kHelpText,                    // CLI 帮助文本。
  kUnknownArgument,             // 未知参数或命令词诊断。
  kMissingOptionValue,          // 带值选项缺少取值诊断。
  kInvalidUsage,                // 命令形状或位置参数用法错误诊断。
  kEmptyInput,                  // 无 stdin 且无直接文本时的提示。
  kTranslationUnavailable,      // 翻译主流程不可用提示。
  kConfigSetSuccess,            // 配置字段写入成功提示。
  kConfigInvalidValue,          // 配置键或配置值非法提示。
  kConfigWriteFailed,           // 配置文件写入失败提示。
  kConfigReadFailed,            // 配置文件读取失败提示。
  kUiLanguagesList,             // 支持的终端界面语言列表。
  kLanguagesListHeader,         // 可用目标语言列表表头。
  kLanguagePromptHeader,        // 展示目标语言 prompt 前的说明。
  kLanguageSetSuccess,          // 目标语言 prompt 写入成功提示。
  kLanguageUnsetSuccess,        // 目标语言 prompt 删除成功提示。
  kLanguageUnsetNoConfig,       // 删除缺失配置 prompt 时的幂等提示。
  kLanguageNotFound,            // 目标语言不可用提示。
  kPromptFileReadFailed,        // prompt 文件读取失败提示。
  kPromptInvalidValue,          // prompt 语言代码或内容非法提示。
  kModelsListHeader,            // 模型列表表头。
  kModelsListEmpty,             // 模型列表为空提示。
  kModelsSetDefaultSuccess,     // 默认模型设置成功提示。
  kModelNotFound,               // 模型配置不存在提示。
  kModelInvalidValue,           // 模型配置字段非法提示。
  kModelAddPromptName,          // 添加模型时询问本地名称。
  kModelAddPromptProviderType,  // 添加模型时询问 provider 类型。
  kModelAddPromptBaseUrl,       // 添加模型时询问 base URL。
  kModelAddPromptModelId,       // 添加模型时询问 provider 模型 ID。
  kModelAddPromptApiKey,        // 添加模型时询问 API key。
  kModelAddPromptTimeoutSeconds,  // 添加模型时询问请求超时秒数。
  kModelAddPromptSetDefault,    // 添加模型时询问是否设为默认模型。
  kModelAddTesting,             // 添加模型时测试 provider 调用提示。
  kModelAddSuccess,             // 添加模型成功提示。
  kModelAddFailed,              // 添加模型失败提示。
  kDefaultModelMissing,         // 翻译时没有可用默认模型提示。
  kInputReadFailed,             // 输入读取失败提示。
  kTranslationFailed,           // 翻译失败提示。
  kTranslationProgress,         // 非流式翻译进度提示。
  kOutputCancelled,             // stdout 下游关闭或用户取消提示。
  kHistoryHitPrompt,            // 命中历史时询问是否重新翻译。
  kHistoryListHeader,           // 历史记录列表表头。
  kHistoryListEmpty,            // 历史记录为空提示。
  kHistoryClearSuccess,         // 历史记录清空成功提示。
  kHistoryReadFailed,           // 历史记录读取失败提示。
  kHistoryWriteFailed,          // 历史记录写入失败提示。
};

/**
 * @brief 文案格式化时使用的可选参数。
 *
 * 当前阶段只需要少量字符串插值。未使用的字段保持空值即可，后续新增
 * 文案时可继续扩展该结构体。
 */
struct MessageArgs {
  std::string_view argument;  // 参数名、命令名或触发错误的原始文本。
  std::string_view key;  // 配置键名，仅配置相关文案使用。
  std::string_view value;  // 配置值，仅配置相关文案使用。
  std::string_view path;  // 配置文件路径，仅配置读写文案使用。
  std::string_view language;  // 已解析的界面语言；非 zh-CN 时统一回退英文。
};

/**
 * @brief 为 CLI 用户可见输出提供单一静态文案入口。
 *
 * Messages 集中管理应用外壳输出的文案，避免业务模块直接嵌入用户可见
 * 字符串。
 * 它不暴露语言表对象；调用方只需要传入文案标识和必要参数。
 * 它不负责解析系统语言或配置文件，调用方必须传入已解析的界面语言。
 *
 * @note 该类不持有共享可变状态，当前实现可并发调用。
 */
class Messages {
 public:
  /**
   * @brief 获取指定文案的本地化字符串。
   *
   * @param id 文案标识。
   * @param args 文案插值参数。args.language 为已经解析好的界面语言；只有 zh-CN
   *     会选择中文，其它值统一使用英文。
   * @return 本地化后的用户可见字符串。
   */
  static std::string Get(MessageId id, const MessageArgs& args = {});

  Messages() = delete;
};

}  // 命名空间 termtrans::i18n
