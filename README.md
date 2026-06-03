# termtrans 使用教程

`termtrans` 是一个 Unix filter 风格的终端 AI 翻译工具。它读取 stdin 或一个直接文本参数，通过 OpenAI-compatible Chat Completions 接口翻译文本，并且只把译文写入 stdout，方便继续接管道、重定向保存或交给分页器阅读。

常见用法：

```bash
termtrans "hello world"
cat README.md | termtrans --to zh-CN > README.zh.md
man ls | col -b | termtrans 
```

注意：`termtrans README.md` 会翻译字面文本 `README.md`，不会读取文件内容。翻译文件请使用 `cat README.md | termtrans`、PowerShell 的 `Get-Content`，或 shell 重定向。

## 功能概览

- 支持 Linux 和 Windows。
- 支持 OpenAI-compatible provider。
- 支持流式输出和非流式输出。
- 支持本地命名模型配置，可保存多个模型并设置默认模型。
- 支持历史记录复用，历史数据库使用 SQLite。
- 支持内置目标语言 prompt，也支持自定义或覆盖 prompt。
- stdout 只输出译文；错误、交互提示和进度信息输出到 stderr。

## 下载

请在 GitHub Releases 页面下载对应平台的文件：


[release](https://github.com/wumoin/termtrans/releases)

推荐下载：

- Linux Debian/Ubuntu：`termtrans_0.1.0_amd64.deb`
- Windows x64：`termtrans-0.1.0-windows-x64.exe`

后续版本发布时，请优先下载最新版本中与你系统匹配的文件。

## Linux 使用教程

### 1. 安装 Debian 包

从 GitHub Releases 下载 `.deb` 文件后，进入下载目录执行：

```bash
sudo apt install ./termtrans_0.1.0_amd64.deb
```

如果你使用 `dpkg` 安装并遇到依赖问题，可以再执行：

```bash
sudo dpkg -i ./termtrans_0.1.0_amd64.deb
sudo apt-get -f install
```

安装后命令位置通常是：

```text
/usr/bin/termtrans
```

验证：

```bash
termtrans --help
```

### 2. 配置文件位置

Linux 配置文件路径固定为：

```text
~/.config/termtrans/config.toml
```

历史数据库路径：

```text
~/.config/termtrans/history.sqlite
```

设置中文界面：

```bash
termtrans config set ui-language zh-CN
```

### 3. 添加模型

首次翻译前需要添加一个 OpenAI-compatible 模型配置：

```bash
termtrans --add-model
```

按提示填写：

```text
模型名称
Provider 类型，留空默认 openai-compatible
Base URL
模型 ID
API key
请求超时时间
是否设为默认模型
```

`Base URL` 可以填写 provider 根地址或 `/v1` 地址，例如：

```text
https://api.example.com/v1
```

程序会自动拼接 `/chat/completions`；如果你填写的地址已经以 `/chat/completions` 结尾，也会直接使用该地址。

`timeout_seconds` 留空默认 60 秒；填写 `0` 表示不设置请求超时上限。添加模型时程序会先做一次测试调用，测试成功后才会写入配置文件。

API key 会保存到本地 `config.toml`，请不要把该文件提交到公开仓库。

查看模型：

```bash
termtrans models list
```

设置默认模型：

```bash
termtrans models set-default my-model
```

### 4. Linux 翻译示例

直接翻译一段文本：

```bash
termtrans "hello world"
```

翻译codex -h输出的帮助文档
```bash
codex -h | termtrans --to zh-CN 
```

翻译 README 并保存：

```bash
cat README.md | termtrans --to zh-CN > README.zh.md
```



翻译 man 页面并用 `less` 阅读：

```bash
man git | col -b | termtrans --to zh-CN | less -R
```

翻译 git diff：

```bash
git diff | termtrans --to zh-CN
```

禁用历史记录：

```bash
cat README.md | termtrans --no-history
```

命中历史时直接复用：

```bash
cat README.md | termtrans --reuse-history
```

强制重新翻译并更新历史：

```bash
cat README.md | termtrans --force
```

## Windows 使用教程

### 1. 下载并放置 exe

从 GitHub Releases 下载 Windows x64 文件：

```text
termtrans-0.1.0-windows-x64.exe
```

建议把它重命名为：

```text
termtrans.exe
```

然后放到固定目录，例如：

```text
C:\Tools\termtrans\termtrans.exe
```

如果没有把这个目录加入 `PATH`，可以在 exe 所在目录用相对路径运行：

```powershell
.\termtrans.exe --help
```

如果已经把 `C:\Tools\termtrans\` 加入 `PATH`，可以直接运行：

```powershell
termtrans --help
```

### 2. 配置文件位置

Windows 配置文件路径：

```text
%APPDATA%\termtrans\config.toml
```

历史数据库路径：

```text
%APPDATA%\termtrans\history.sqlite
```

设置中文界面：

```powershell
termtrans config set ui-language zh-CN
```

### 3. 添加模型

首次翻译前需要添加一个 OpenAI-compatible 模型配置：

```powershell
termtrans --add-model
```

程序会依次询问：

```text
模型名称
Provider 类型，留空默认 openai-compatible
Base URL
模型 ID
API key
请求超时时间
是否设为默认模型
```

`Base URL` 可以填写 provider 根地址或 `/v1` 地址，例如：

```text
https://api.example.com/v1
```

程序会自动拼接 `/chat/completions`；如果你填写的地址已经以 `/chat/completions` 结尾，也会直接使用该地址。

`timeout_seconds` 留空默认 60 秒；填写 `0` 表示不设置请求超时上限。添加模型时程序会先做一次测试调用，测试成功后才会写入配置文件。

API key 会保存到本地 `config.toml`，请不要把该文件提交到公开仓库。

查看模型：

```powershell
termtrans models list
```

设置默认模型：

```powershell
termtrans models set-default my-model
```

### 4. Windows 翻译示例

直接翻译一段文本：

```powershell
termtrans "hello world"
```

翻译codex -h输出的帮助文档
```powershell
codex -h | termtrans --to zh-CN 
```


翻译文件内容：

```powershell
Get-Content .\README.md -Raw | termtrans --to zh-CN
```

翻译并保存：

```powershell
Get-Content .\README.md -Raw | termtrans --to zh-CN > README.zh.md
```

本次临时使用另一个模型：

```powershell
Get-Content .\README.md -Raw | termtrans --model my-other-model
```

非流式输出，适合希望翻译完成后一次性输出的场景：

```powershell
Get-Content .\README.md -Raw | termtrans --no-stream
```

## 常用命令

### 翻译选项

```bash
termtrans [翻译选项] [文本]
```

可用选项：

```text
--to <语言>        本次目标语言
--model <名称>     本次使用的本地模型配置
--stream           流式输出译文
--no-stream        完成后一次性输出译文
--force            跳过历史并重新翻译，成功后保存历史
--reuse-history    命中历史时直接复用
--no-history       禁用历史读取和写入
--add-model        交互式添加模型配置
```

输入规则：

- stdin 有管道输入时，优先翻译 stdin。
- 没有 stdin 时，只接受一个直接文本参数。
- 多词直接文本必须加引号。
- 不支持把文件路径当作输入文件读取。

### 配置命令

设置默认目标语言：

```bash
termtrans config set target-language zh-CN
```

设置终端界面语言：

```bash
termtrans config set ui-language zh-CN
```

设置默认是否流式输出：

```bash
termtrans config set stream true
termtrans config set stream false
```

设置默认历史模式：

```bash
termtrans config set history-mode ask
termtrans config set history-mode reuse
termtrans config set history-mode force
termtrans config set history-mode off
```

设置单个翻译分段的目标最大字节数：

```bash
termtrans config set max-input-bytes 0
termtrans config set max-input-bytes 12000
```

`0` 表示不按用户阈值限制分段大小。

### 目标语言和 Prompt

列出可用目标语言：

```bash
termtrans languages list
```

当前内置目标语言包括：

```text
ar, bn, de, en, es, fa, fr, he, hi, id, it, ja, ko, ms, nl, pl, pt, ru, th, tr, uk, ur, vi, zh-CN, zh-TW
```

查看某个语言当前生效的完整 prompt：

```bash
termtrans languages show zh-CN
```

从文件写入自定义 prompt：

```bash
termtrans languages set my-lang --file ./prompt.txt
```

删除配置中的 prompt：

```bash
termtrans languages unset my-lang
```

如果删除的是内置语言的覆盖 prompt，该语言会恢复内置 prompt。

### 模型管理

添加模型：

```bash
termtrans --add-model
```

列出模型：

```bash
termtrans models list
```

设置默认模型：

```bash
termtrans models set-default my-model
```

### 历史记录

列出历史记录元数据：

```bash
termtrans history list
```

清空历史记录：

```bash
termtrans history clear
```

历史模式说明：

```text
ask    命中历史时询问；无法交互时直接复用
reuse  命中历史时直接复用
force  忽略历史译文并重新翻译，成功后保存历史
off    不读取也不写入历史
```

历史命中只按输入文本 SHA-256 和目标语言判断，不按模型名称判断。

## 故障排查

### 提示没有默认模型

先添加模型：

```bash
termtrans --add-model
```

### 提示模型配置不存在

查看已有模型名称：

```bash
termtrans models list
```

然后设置默认模型：

```bash
termtrans models set-default <名称>
```

### 翻译文件时只输出了文件名

不要这样用：

```bash
termtrans README.md
```

Linux 请使用：

```bash
cat README.md | termtrans
```

Windows PowerShell 请使用：

```powershell
Get-Content .\README.md -Raw | termtrans
```

### 想关闭历史记录

单次关闭：

```bash
termtrans --no-history "hello world"
```

默认关闭：

```bash
termtrans config set history-mode off
```

### 想查看完整帮助

```bash
termtrans --help
```
