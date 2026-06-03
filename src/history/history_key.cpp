/**
 * @file history_key.cpp
 * @brief 实现翻译历史命中 key 计算。
 */

#include "history/history_key.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/sha.h>
#endif

namespace termtrans::history {
namespace {

constexpr std::size_t kSha256DigestLength = 32;

#ifdef _WIN32

/**
 * @brief 释放 BCrypt 算法 provider 的 RAII 包装。
 */
class BcryptAlgorithm {
 public:
  BcryptAlgorithm() = default;
  BcryptAlgorithm(const BcryptAlgorithm&) = delete;
  BcryptAlgorithm& operator=(const BcryptAlgorithm&) = delete;

  ~BcryptAlgorithm() {
    if (handle_ != nullptr) {
      BCryptCloseAlgorithmProvider(handle_, 0);
    }
  }

  BCRYPT_ALG_HANDLE* put() { return &handle_; }
  BCRYPT_ALG_HANDLE get() const { return handle_; }

 private:
  BCRYPT_ALG_HANDLE handle_ = nullptr;
};

/**
 * @brief 释放 BCrypt hash handle 的 RAII 包装。
 */
class BcryptHash {
 public:
  BcryptHash() = default;
  BcryptHash(const BcryptHash&) = delete;
  BcryptHash& operator=(const BcryptHash&) = delete;

  ~BcryptHash() {
    if (handle_ != nullptr) {
      BCryptDestroyHash(handle_);
    }
  }

  BCRYPT_HASH_HANDLE* put() { return &handle_; }
  BCRYPT_HASH_HANDLE get() const { return handle_; }

 private:
  BCRYPT_HASH_HANDLE handle_ = nullptr;
};

/**
 * @brief 校验 BCrypt 调用结果。
 */
void ThrowIfBcryptFailed(NTSTATUS status) {
  if (status < 0) {
    throw std::runtime_error("failed to compute SHA-256 with BCrypt");
  }
}

/**
 * @brief 用 Windows CNG/BCrypt 计算 SHA-256 摘要。
 */
std::array<unsigned char, kSha256DigestLength> Sha256(
    std::string_view input_text) {
  std::array<unsigned char, kSha256DigestLength> digest{};

  BcryptAlgorithm algorithm;
  ThrowIfBcryptFailed(BCryptOpenAlgorithmProvider(
      algorithm.put(), BCRYPT_SHA256_ALGORITHM, nullptr, 0));

  BcryptHash hash;
  ThrowIfBcryptFailed(
      BCryptCreateHash(algorithm.get(), hash.put(), nullptr, 0, nullptr, 0, 0));

  std::size_t offset = 0;
  while (offset < input_text.size()) {
    const std::size_t remaining = input_text.size() - offset;
    const ULONG chunk_size = static_cast<ULONG>(
        (std::min)(remaining,
                   static_cast<std::size_t>((std::numeric_limits<ULONG>::max)())));
    ThrowIfBcryptFailed(BCryptHashData(
        hash.get(),
        reinterpret_cast<PUCHAR>(
            const_cast<char*>(input_text.data() + offset)),
        chunk_size, 0));
    offset += chunk_size;
  }

  ThrowIfBcryptFailed(BCryptFinishHash(hash.get(), digest.data(),
                                       static_cast<ULONG>(digest.size()), 0));
  return digest;
}

#else

/**
 * @brief 用 OpenSSL 计算 SHA-256 摘要。
 */
std::array<unsigned char, kSha256DigestLength> Sha256(
    std::string_view input_text) {
  std::array<unsigned char, kSha256DigestLength> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(input_text.data()),
         input_text.size(), digest.data());
  return digest;
}

#endif

/**
 * @brief 将 SHA-256 摘要转换为小写十六进制字符串。
 *
 * @param digest 固定长度摘要。
 * @return 64 字符小写十六进制文本。
 */
std::string HexDigest(const std::array<unsigned char, kSha256DigestLength>&
                          digest) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const unsigned char byte : digest) {
    output << std::setw(2) << static_cast<int>(byte);
  }

  return output.str();
}

}  // namespace

/**
 * @brief 计算完整输入文本对应的历史 key。
 */
HistoryKey ComputeHistoryKey(std::string_view input_text,
                             std::string_view target_language) {
  return HistoryKey{
      .input_sha256 = HexDigest(Sha256(input_text)),
      .target_language = std::string(target_language),
  };
}

}  // namespace termtrans::history
