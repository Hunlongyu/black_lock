<div align="center">

# BlackLock

**无界面、极简、单文件的 Windows 黑屏锁屏工具**

一键真黑屏锁住屏幕（而非息屏），回车或密码解锁；锁定期间不睡眠、动鼠标不解锁。

![platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20(x64%20%C2%B7%20x86)-0078D6)
![language](https://img.shields.io/badge/language-C17-00599C)
![build](https://img.shields.io/badge/build-MSVC-8250DF)
![size](https://img.shields.io/badge/size-~28.5%20KB-brightgreen)
![deps](https://img.shields.io/badge/third--party%20deps-0-success)
![license](https://img.shields.io/badge/license-MIT-blue)

</div>

---

## 简介

BlackLock 是一个常驻后台的小工具：按下全局快捷键（默认 `Alt + L`）后，用一块覆盖所有显示器的黑色窗口把桌面遮住。默认按回车（或再按一次快捷键）即可解锁；也可开启密码保护。它没有任何窗口和设置界面，全部行为通过一个带注释的配置文件调整，编译产物是**单个约 30 KB 的 exe，零第三方依赖**。

适用场景：临时离开工位时快速遮屏防窥、午休黑屏而不让电脑睡眠、需要"锁屏但后台任务继续跑"的场合。

> ⚠️ BlackLock 是"防君子不防小人"的**便捷临时遮挡锁**，不是对抗专业攻击者的安全产品。详见 [安全声明](#安全声明)。

## 功能特性

- 🖤 **真黑屏锁定**：覆盖全部显示器，隐藏鼠标指针，动鼠标 / 点击都不解锁
- ⌨️ **全局快捷键**：默认 `Alt + L`，可在配置文件自定义（大小写不敏感）
- 🔓 **两种解锁模式**：默认「简单锁屏」——按 **回车** 或 **再次按快捷键** 即可解锁；勾选「开启密码」后需输入密码解锁
- 🚫 **锁定期间不睡眠**：阻止显示器息屏与系统睡眠，后台任务照常运行
- ♻️ **配置热重载**：改完配置保存即时生效（密码 / 快捷键 / 开机自启 / 是否需要密码），无需重启
- 🚀 **开机自启**：可在配置文件或托盘菜单开关
- 📌 **系统托盘菜单**：开机自启 / 开启密码 / 暂停 / 配置 / 退出
- 🧩 **拦截切换热键**：锁定期间吞掉 `Win`、`Alt+Tab`、`Alt+F4`、`Ctrl+Esc` 等
- 🖥️ **多屏 & 高 DPI**：正确覆盖整个虚拟桌面，分辨率变化 / 热插拔自动重铺
- 🪶 **极致轻量**：纯 C（C17）+ Win32，单文件 exe，无第三方运行时依赖

## 快速开始

1. 下载 / 编译得到 `BlackLock.exe`。
2. 双击运行 —— 程序驻留系统托盘，无窗口弹出。
3. 按 `Alt + L` —— 屏幕立即变黑。
4. **解锁**：默认无需密码，按 **回车** 或 **再按一次 `Alt + L`** 即可解锁回到桌面。

想要密码保护？右键托盘图标勾选 **「开启密码」**，并在配置文件里把 `password` 设成你的密码；之后解锁就需要输入密码 + 回车。

首次运行会自动生成配置文件（见下）。想开机自启，右键托盘勾选「开机自启」，或把配置里的 `autostart` 改成 `true`。

## 配置说明

### 文件位置（读取优先级，先命中者生效）

1. **exe 同目录** `config.ini` —— 便携优先，最高优先级
2. `%APPDATA%\BlackLock\config.ini` —— 回退

若两处都没有，程序会**只在 `%APPDATA%\BlackLock\` 自动生成**默认配置（保持 exe 目录洁净）。想要便携使用，把 `config.ini` 放到 exe 旁边即可——它优先级最高。

### 配置项

| 段 | 键 | 默认 | 说明 |
|----|----|------|------|
| `[general]` | `hotkey` | `Alt+L` | 触发锁定的全局快捷键 |
| `[general]` | `autostart` | `false` | 是否开机自启（同步到注册表启动项） |
| `[security]` | `require_password` | `false` | 是否需要密码解锁（对应托盘「开启密码」）。`false` 时回车/再按快捷键即解锁 |
| `[security]` | `password` | *(空)* | 解锁密码（明文，仅 `require_password = true` 时生效） |

### 示例

```ini
; 支持 ; 与 # 整行注释

[general]
; 修饰键可用 Alt / Ctrl / Shift / Win，用 + 连接，例如 Alt+L、Ctrl+Alt+L
hotkey = Alt+L
autostart = false

[security]
; false = 回车/再按快捷键即解锁；true = 需输入下面的 password
require_password = false
password =
```

> 💡 **热重载**：修改并保存配置文件后立即生效，无需重启程序。托盘菜单"配置"会用系统默认程序打开当前正在生效的那个文件。

### 快捷键格式

- 修饰键：`Alt` / `Ctrl` / `Shift` / `Win`，用 `+` 连接。
- 主键：字母 `A~Z`、数字 `0~9`、功能键 `F1~F24`。
- **大小写不敏感**：`alt+l`、`Alt+L`、`ALT+L` 等价。
- 修饰键需精确匹配：`Alt+L` 不等于 `Alt+Shift+L`。

## 从源码构建

**本项目仅使用 MSVC 编译**（CMakeLists 内置守卫，非 MSVC 直接报错）。

**依赖**

- CMake ≥ 3.21
- Visual Studio 2022/2026 的 C++ 桌面开发工具集（含 MSVC 与 Windows SDK）

**命令**（已提供 `CMakePresets.json`）

```powershell
cmake --preset msvc                 # 配置 (Visual Studio 生成器, x64)
cmake --build --preset msvc-minsize # 构建体积最小版
```

产物输出到 `dist\BlackLock.exe`，同时在 `dist\` 生成一份默认 `config.ini`。

**32 位 (x86) 构建**：

```powershell
cmake --preset msvc-x86
cmake --build --preset msvc-x86-minsize
```

> 注意：x64 与 x86 共用输出目录 `dist\`，本地一次只保留最后构建的那个架构；发布时由 GitHub Actions 分别产出。

其它配置：`cmake --build --preset msvc-release` / `msvc-debug`。

> 使用其它 VS 版本时，改 `CMakePresets.json` 里的 `generator`（如 `Visual Studio 17 2022`）即可。

### 依赖清单

无任何第三方运行时依赖，仅链接 Windows 系统库：

| 库 | 用途 |
|----|------|
| `user32` | 窗口、消息、热键、键盘钩子、光标 |
| `gdi32` | 黑色画刷 |
| `bcrypt` | 密码 SHA-256 |
| `shell32` | 托盘图标、命令行、打开配置 |
| `advapi32` | 注册表（开机自启） |

## 版本与发布

- 版本号在 [CMakeLists.txt](CMakeLists.txt) 的 `project(... VERSION x.y.z)` 定义，会显示在**托盘右键菜单顶部**（置灰不可点击）与 exe 文件属性中。
- **自动发布**：推送形如 `v1.2.3` 的 Git 标签，[GitHub Actions](.github/workflows/release.yml) 会自动编译 Release 版并上传 `BlackLock.exe` 与打包 zip 到对应的 GitHub Release。发布版本号取自标签（`-DBL_VERSION_OVERRIDE`），无需改代码。

  ```bash
  git tag v1.2.3
  git push origin v1.2.3   # 触发构建与发布
  ```

- **不含在线自动升级**：BlackLock 仅通过 GitHub Release 手动下载更新。工具本身不到 30 KB，重新下载成本极低。

## 常见问题

**Q：忘记密码 / 程序卡住怎么办？**
A：按 `Ctrl + Alt + Del`（内核级安全序列，任何用户态程序都拦不住）→ 选「任务管理器」。检测到任务管理器出现在前台时，黑屏会主动让到它下方，你即可**用鼠标**右键 `BlackLock` → 结束任务解除锁定。（锁定期间键盘被接管，任务管理器里请用鼠标操作。）

**Q：锁定期间电脑会睡眠吗？**
A：不会。锁定期间会阻止显示器息屏与系统睡眠；解锁后恢复默认电源策略。

**Q：怎么临时关掉快捷键？**
A：右键托盘图标 → 勾选"暂停"，`Alt+L` 即不再触发；再点一次恢复。

**Q：杀毒软件报警？**
A：BlackLock 使用了低级键盘钩子（`WH_KEYBOARD_LL`）来收密码，部分杀软会对此关注。代码开源、无网络行为，可自行审阅或对二进制签名以减少误报。

**Q：怎么彻底卸载？**
A：退出程序（托盘 → 退出），删除 `BlackLock.exe`；如启用过开机自启，删注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 下的 `BlackLock` 值；如有配置残留，删 `%APPDATA%\BlackLock` 目录。

## 工作原理

程序是单进程双状态机：

- **空闲态**：无可见窗口，仅托盘图标，用 `RegisterHotKey` 监听全局快捷键。
- **锁定态**：创建覆盖虚拟桌面的黑色置顶窗口，安装 `WH_KEYBOARD_LL` 键盘钩子收集密码并吞掉所有按键，用 `SetThreadExecutionState` 阻止睡眠；密码经 SHA-256 比对，正确则退出锁定态。

## 安全声明

BlackLock 定位为**便捷的临时遮挡锁**，请知悉其边界：

- `Ctrl + Alt + Del` 无法被拦截，能物理接触键盘者可借此结束进程解锁——这是**有意保留**的逃生口。
- 密码以**明文**存储在配置文件中，能读到该文件的人即可看到密码。
- 不承诺对抗专业攻击、内核级绕过、物理攻击。

需要真正的安全锁定，请使用系统 `Win + L` / 账户密码 / BitLocker。

## 目录结构

```
lock/
├─ CMakeLists.txt         # CMake 工程 (仅 MSVC)
├─ CMakePresets.json      # 构建预设
├─ cmake/                 # 构建辅助脚本
├─ docs/
│  └─ 软件开发设计文档.md    # 完整设计文档 (SDD)
└─ src/                   # 源码 (main / config / hotkey / lockwin / keyhook / autostart / tray / util)
```

完整设计与实现细节见 [docs/软件开发设计文档.md](docs/软件开发设计文档.md)。

## 许可

基于 [MIT License](LICENSE) 开源。
