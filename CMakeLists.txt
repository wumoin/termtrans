# termtrans 命令行应用的 CMake 构建入口。
# 本文件定义当前构建目标、测试注册和编译基线；按用户要求，
# 业务默认值统一放在 src/config/app_constants.h 中维护。
cmake_minimum_required(VERSION 3.20)

project(termtrans VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

option(TERMTRANS_STATIC_RUNTIME "Use the static MSVC runtime." OFF)

set(CMAKE_CXX_STANDARD 20) # 当前代码使用 C++20 语法和标准库能力。
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 编译器不支持 C++20 时直接失败。
set(CMAKE_CXX_EXTENSIONS OFF) # 避免依赖 GNU/MSVC 私有扩展影响跨平台构建。

if(MSVC)
  if(TERMTRANS_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  add_compile_options(/utf-8)
endif()

# 所有自动化测试都通过 CTest 统一调度，符合仓库测试入口约束。
enable_testing()

# toml++ 是配置读写的唯一外部依赖。固定 tag 以避免上游主分支变更
# 影响本地 CTest 结果。
FetchContent_Declare(tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(tomlplusplus nlohmann_json)

# OpenAI-compatible provider 通过 libcurl 执行 HTTP(S) 请求；测试通过
# 注入 fake HTTP client 验证解析逻辑，不依赖真实网络。
find_package(CURL REQUIRED)
if(WIN32)
  find_package(unofficial-sqlite3 CONFIG QUIET)
endif()
if(unofficial-sqlite3_FOUND)
  set(TERMTRANS_SQLITE_LIBS unofficial::sqlite3::sqlite3)
else()
  find_package(SQLite3 REQUIRED)
  set(TERMTRANS_SQLITE_LIBS SQLite::SQLite3)
endif()
if(WIN32)
  set(TERMTRANS_CRYPTO_LIBS bcrypt)
else()
  find_package(OpenSSL REQUIRED)
  set(TERMTRANS_CRYPTO_LIBS OpenSSL::Crypto)
endif()

# 核心库用于让应用流程在测试中直接复用，避免每个单元测试都启动
# CLI 二进制。
add_library(termtrans_core STATIC
  src/app/application.cpp
  src/cli/args.cpp
  src/config/config.cpp
  src/config/platform_paths.cpp
  src/history/history_key.cpp
  src/history/history_policy.cpp
  src/history/sqlite_history_store.cpp
  src/i18n/messages.cpp
  src/input/reader.cpp
  src/output/plain_writer.cpp
  src/output/progress_indicator.cpp
  src/prompt/prompt_registry.cpp
  src/translate/chunker.cpp
  src/translate/curl_http_client.cpp
  src/translate/openai_compatible_translator.cpp
  src/translate/translation_session.cpp
)

target_include_directories(termtrans_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# termtrans_core 暴露配置读写接口，测试可直接复用 toml++ 类型解析能力。
target_link_libraries(termtrans_core
  PUBLIC
    CURL::libcurl
    ${TERMTRANS_CRYPTO_LIBS}
    ${TERMTRANS_SQLITE_LIBS}
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

# 应用二进制只保留 main() 外壳，具体命令行为由 termtrans_core 承载。
add_executable(termtrans
  src/main.cpp
)

target_link_libraries(termtrans
  PRIVATE
    termtrans_core
)

# 安装规则供系统包脚本复用。打包脚本通过 `cmake --install` 生成
# 平台无关的 staging 目录，再分别封装为 deb 或 Windows 安装器。
install(TARGETS termtrans
  RUNTIME DESTINATION bin
)

install(FILES doc/termtrans.md
  DESTINATION share/doc/termtrans
  RENAME README.md
)

# 注册一个普通 C++ 测试可执行文件。测试统一链接 termtrans_core，并通过
# CTest 调度，避免引入仓库规范之外的测试入口。
function(add_termtrans_test test_name test_source)
  add_executable(${test_name}
    ${test_source}
  )

  target_link_libraries(${test_name}
    PRIVATE
      termtrans_core
  )

  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_termtrans_test(test_application tests/test_application.cpp)
add_termtrans_test(test_args tests/test_args.cpp)
add_termtrans_test(test_app_constants tests/test_app_constants.cpp)
add_termtrans_test(test_chunker tests/test_chunker.cpp)
add_termtrans_test(test_config tests/test_config.cpp)
add_termtrans_test(test_history_key tests/test_history_key.cpp)
add_termtrans_test(test_history_policy tests/test_history_policy.cpp)
add_termtrans_test(test_history_store tests/test_history_store.cpp)
add_termtrans_test(test_input_reader tests/test_input_reader.cpp)
add_termtrans_test(test_messages tests/test_messages.cpp)
add_termtrans_test(test_model_profile tests/test_model_profile.cpp)
add_termtrans_test(test_openai_compatible_translator tests/test_openai_compatible_translator.cpp)
add_termtrans_test(test_plain_writer tests/test_plain_writer.cpp)
add_termtrans_test(test_prompt_registry tests/test_prompt_registry.cpp)
add_termtrans_test(test_translation_session tests/test_translation_session.cpp)
