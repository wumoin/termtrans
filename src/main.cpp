/**
 * @file main.cpp
 * @brief 提供 termtrans 可执行文件入口。
 */

#include "app/application.h"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief 启动应用外壳，并返回进程退出码。
 *
 * 入口函数把参数解析和用户可见行为交给 termtrans::app::Application，
 * 让后续阶段继续保持 main() 不承载业务逻辑。
 *
 * @param argc 进程参数数量。
 * @param argv 进程参数数组。
 * @return 应用层返回的进程退出码。
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif

  termtrans::app::Application application;
  return application.Run(argc, argv, std::cout, std::cerr);
}
