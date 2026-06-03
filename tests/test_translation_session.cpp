/**
 * @file test_translation_session.cpp
 * @brief 测试统一分块翻译会话。
 */

#include "translate/translation_session.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 测试用输出 sink。
 */
class CapturingSink : public termtrans::translate::TranslationStreamSink {
 public:
  bool Write(std::string_view text) override {
    text_ += text;
    return !cancel_on_write_;
  }

  void set_cancel_on_write(bool cancel_on_write) {
    cancel_on_write_ = cancel_on_write;
  }

  const std::string& text() const { return text_; }

 private:
  std::string text_;
  bool cancel_on_write_ = false;
};

/**
 * @brief 测试用 translator。
 */
class FakeTranslator : public termtrans::translate::Translator {
 public:
  termtrans::translate::TranslateResult Translate(
      const termtrans::translate::TranslateRequest& request,
      termtrans::translate::TranslationStreamSink* sink) override {
    requests.push_back(request);
    if (fail_on_call > 0 &&
        static_cast<int>(requests.size()) == fail_on_call) {
      return termtrans::translate::TranslateResult{
          .is_ok = false,
          .error_message = "fake failure",
      };
    }

    std::string translated;
    if (!fixed_translations.empty() &&
        requests.size() <= fixed_translations.size()) {
      translated = fixed_translations[requests.size() - 1];
    } else {
      translated = "[" + request.source_text + "]";
    }
    if (request.stream && sink != nullptr && !sink->Write(translated)) {
      return termtrans::translate::TranslateResult{
          .is_ok = false,
          .is_cancelled = true,
          .error_message = "cancelled",
      };
    }

    return termtrans::translate::TranslateResult{
        .is_ok = true,
        .translated_text = translated,
    };
  }

  std::vector<termtrans::translate::TranslateRequest> requests;
  std::vector<std::string> fixed_translations;
  int fail_on_call = 0;
};

/**
 * @brief 验证多 chunk 会顺序翻译并传递最近译文上下文。
 */
int ShouldTranslateChunksAndPassRecentContext() {
  FakeTranslator translator;
  CapturingSink sink;
  termtrans::translate::TranslationSession session(
      &translator, termtrans::translate::Chunker(6), 100);

  const termtrans::translate::TranslationSessionResult result = session.Run(
      termtrans::translate::TranslationSessionRequest{
          .target_language = "zh-CN",
          .system_prompt = "prompt",
          .input_text = "first\nsecond\n",
          .stream = true,
      },
      &sink);

  return Expect(result.is_ok, "多 chunk 会话应成功") &&
                 Expect(translator.requests.size() == 2,
                        "应发起两个 chunk 请求") &&
                 Expect(translator.requests[1].recent_translated_text.find(
                            "[first") != std::string::npos,
                        "第二个请求应携带最近译文上下文") &&
                 Expect(sink.text() == "[first\n][second\n]",
                        "流式模式应增量写入 sink")
             ? 0
             : 1;
}

/**
 * @brief 验证非流式模式在完整会话结束后一次性输出。
 */
int ShouldWriteNonStreamAfterAllChunks() {
  FakeTranslator translator;
  CapturingSink sink;
  termtrans::translate::TranslationSession session(
      &translator, termtrans::translate::Chunker(6), 100);

  const termtrans::translate::TranslationSessionResult result = session.Run(
      termtrans::translate::TranslationSessionRequest{
          .target_language = "zh-CN",
          .system_prompt = "prompt",
          .input_text = "first\nsecond\n",
          .stream = false,
      },
      &sink);

  return Expect(result.is_ok, "非流式会话应成功") &&
                 Expect(sink.text() == "[first\n][second\n]",
                        "非流式应在会话完成后写入完整译文") &&
                 Expect(result.translated_text == sink.text(),
                        "会话结果应保存完整译文")
             ? 0
             : 1;
}

/**
 * @brief 验证 provider 失败会中断会话。
 */
int ShouldStopOnTranslatorFailure() {
  FakeTranslator translator;
  translator.fail_on_call = 2;
  CapturingSink sink;
  termtrans::translate::TranslationSession session(
      &translator, termtrans::translate::Chunker(6), 100);

  const termtrans::translate::TranslationSessionResult result = session.Run(
      termtrans::translate::TranslationSessionRequest{
          .target_language = "zh-CN",
          .system_prompt = "prompt",
          .input_text = "first\nsecond\n",
          .stream = true,
      },
      &sink);

  return Expect(!result.is_ok, "provider 失败后会话不应成功") &&
                 Expect(result.error_message == "fake failure",
                        "会话应返回 provider 错误") &&
                 Expect(translator.requests.size() == 2,
                        "第二个 chunk 失败后应停止")
             ? 0
             : 1;
}

/**
 * @brief 验证 sink 取消会中断流式会话。
 */
int ShouldStopOnSinkCancellation() {
  FakeTranslator translator;
  CapturingSink sink;
  sink.set_cancel_on_write(true);
  termtrans::translate::TranslationSession session(
      &translator, termtrans::translate::Chunker(100), 100);

  const termtrans::translate::TranslationSessionResult result = session.Run(
      termtrans::translate::TranslationSessionRequest{
          .target_language = "zh-CN",
          .system_prompt = "prompt",
          .input_text = "input",
          .stream = true,
      },
      &sink);

  return Expect(!result.is_ok, "sink 取消后会话不应成功") &&
                 Expect(result.is_cancelled, "sink 取消应传递取消状态")
             ? 0
             : 1;
}

/**
 * @brief 验证最近译文上下文裁剪保持 UTF-8 字符边界。
 */
int ShouldTrimRecentContextOnUtf8Boundary() {
  FakeTranslator translator;
  translator.fixed_translations = {
      "\xE4\xBD\xA0\xE5\xA5\xBD",  // 你好
      "\xE4\xB8\x96\xE7\x95\x8C",  // 世界
  };
  CapturingSink sink;
  termtrans::translate::TranslationSession session(
      &translator, termtrans::translate::Chunker(6), 4);

  const termtrans::translate::TranslationSessionResult result = session.Run(
      termtrans::translate::TranslationSessionRequest{
          .target_language = "zh-CN",
          .system_prompt = "prompt",
          .input_text = "first\nsecond\n",
          .stream = false,
      },
      &sink);

  return Expect(result.is_ok, "UTF-8 上下文裁剪会话应成功") &&
                 Expect(translator.requests.size() == 2,
                        "应产生第二个请求用于检查上下文") &&
                 Expect(translator.requests[1].recent_translated_text ==
                            "\xE5\xA5\xBD",
                        "最近译文上下文应从完整的“好”开始")
             ? 0
             : 1;
}

}  // namespace

int main() {
  if (ShouldTranslateChunksAndPassRecentContext() != 0) {
    return 1;
  }
  if (ShouldWriteNonStreamAfterAllChunks() != 0) {
    return 1;
  }
  if (ShouldStopOnTranslatorFailure() != 0) {
    return 1;
  }
  if (ShouldStopOnSinkCancellation() != 0) {
    return 1;
  }
  if (ShouldTrimRecentContextOnUtf8Boundary() != 0) {
    return 1;
  }

  return 0;
}
