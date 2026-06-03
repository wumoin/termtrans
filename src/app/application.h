/**
 * @file application.h
 * @brief 声明应用层命令分发器。
 */

#pragma once

#include <iosfwd>

namespace termtrans::app {

/**
 * @brief 协调命令行应用流程。
 *
 * Application 负责解析命令行参数、加载配置、执行配置写入、UI 语言列表
 * 目标语言 Prompt 管理命令、模型管理命令、历史命令和 Plain 翻译主流程。
 * 它只编排历史查询和保存，不实现 provider、SQLite 细节或外部分页器。
 *
 * @note 该类不是线程安全的。
 */
class Application {
 public:
  /**
   * @brief 运行当前进程调用对应的 CLI 外壳。
   *
   * @param argc main() 传入的参数数量。
   * @param argv main() 传入的参数数组；在少数宿主环境下 argv[0] 可能为空，
   *     解析流程也不依赖它。
   * @param stdout_stream 成功命令输出的目标流。
   * @param stderr_stream 诊断信息、交互提示和进度提示的目标流。
   * @return 进程退出码：0 表示命令已完成，1 表示运行时读写失败或当前
   *     阶段暂不可执行的工作，2 表示命令行用法错误或配置值非法。
   *
   * @note 该函数会在真实翻译流程中消费标准输入。
   */
  int Run(int argc,
          char* argv[],
          std::ostream& stdout_stream,
          std::ostream& stderr_stream) const;

  /**
   * @brief 运行当前进程调用，并允许测试注入 stdin。
   *
   * @param argc main() 传入的参数数量。
   * @param argv main() 传入的参数数组。
   * @param stdin_stream 翻译主流程读取输入的来源。
   * @param stdout_stream 成功命令和译文输出的目标流。
   * @param stderr_stream 诊断信息、交互提示和进度提示的目标流。
   * @return 进程退出码。
   *
   * @note 该函数会在真实翻译流程中消费 stdin_stream。
   */
  int Run(int argc,
          char* argv[],
          std::istream& stdin_stream,
          std::ostream& stdout_stream,
          std::ostream& stderr_stream) const;
};

}  // 命名空间 termtrans::app
