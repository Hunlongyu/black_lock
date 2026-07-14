# 更新日志

本项目遵循[语义化版本](https://semver.org/lang/zh-CN/)。

## [1.0.0] - 2026-07-14

首个正式版本。

### 功能

- 全局快捷键 `Alt + L`（可配置）触发**真黑屏**锁定（覆盖全部显示器，而非息屏）
- 输入密码回车解锁（默认 `ciqtek`，配置文件可改，明文）
- 锁定期间阻止显示器息屏与系统睡眠
- 配置文件热重载：密码 / 快捷键 / 开机自启改完保存即时生效
- 配置双位置查找：exe 同目录 `config.ini`（便携优先）> `%APPDATA%\BlackLock\config.ini`
- 无界面 + 系统托盘右键菜单：版本号 / 开机自启 / 暂停 / 配置 / 退出
- 开机自启（注册表 Run，配置或托盘可开关）
- 锁定期间拦截 `Win` / `Alt+Tab` / `Alt+F4` / `Ctrl+Esc` 等切换热键
- 多显示器与高 DPI 适配，分辨率变化 / 热插拔自动重铺
- `Ctrl + Alt + Del → 任务管理器` 逃生口（黑窗自动让位，可鼠标结束进程）
- 修复：用 `Alt+L` 触发时残留的修饰键“卡住”问题
- 纯 C（C17）+ Win32，单文件、零第三方依赖，x64 约 30 KB / x86 约 26 KB
- MSVC-only CMake 工程 + CMakePresets + GitHub Actions 标签自动发布（x64 & x86）

[1.0.0]: https://github.com/Hunlongyu/black_lock/releases/tag/v1.0.0
