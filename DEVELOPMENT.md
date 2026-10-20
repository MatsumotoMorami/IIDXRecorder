# 开发说明

当前开发版本：v0.9。`source/` 包含录像 DLL 和验证程序；直播源码位于相邻的独立 IIDXLive 仓库。

使用 Zig 0.14.1 在仓库根目录运行：

```powershell
./source/build.ps1 -ZigPath '你的 zig.exe 完整路径'
```

生成 IIDXRecorder.dll；编译产物已加入 .gitignore。游戏运行时不要替换已加载的 DLL。运行配置位置为 C:\IIDX\IIDXRecorder.ini。

验证程序 source/test_autoguard.cpp 和 source/test_naming.cpp 可用同一 Zig 编译，使用 -target x86_64-windows-gnu -O2 -static -std=c++17，并链接 -lmfplat -lmfuuid -lole32 -luuid；可执行文件放入 build/ 后运行。这两项为模拟与文件命名验证，不替代实机游玩。

已实机确认：Standard 音视频、VEFX＋EFFECT 跳关、段位连续录制。Arena 连续录制及新增名称元数据仍需实机验收。保持 ASIO 和游戏原生采集；强退保护最低优先级。

`test_autoguard` 和 `test_naming` 无参数测试旧布局，传入 `updated` 测试四月布局，传入 `new latest` 测试九月布局；`test_autoguard` 传入三个任意参数测试当前 patch 后的布局。`test_image` 以游戏 DLL 路径为参数，仅映射 PE、不启动游戏，验证挂接和名称读取特征，并验证录像挂接后直播仍能自动定位。当前文件的标识为 `6aa23d57/C5FF000`。

直播构建和验证范围见 [IIDXLive README](../IIDXLive/README.md)。`test-discovery` 检查真实 PE 的自动定位与变更拒绝；`test-protocol` 检查缓冲区并发一致性；`test-integration` 使用未初始化的游戏 PE 中的音频分流函数和模拟上游 PCM，验证桥接 DLL 到真实 OBS 的音频链路。它不替代实机全屏与 ASIO 测试。

仓库不包含游戏 DLL、群插件、录像或编译工具。

2026-09-18：八月构建的核心 recorder/encoder 函数按指令和结构位移与四月构建对照；RTTI 核对六个场景与录制器/编码器方法表。新版段位与 Arena Flow 的 leave 均为转跳函数，仍按各自方法表槽挂接，不按旧函数长度推断。全局 main recorder 位于 RVA 0xb298518，mode/style/song 为 0xacd59a0/0xacd59a4/0xacd59d0，grade 为 0xaaafe20。保留完整 preview 生命周期，未恢复已撤回的跳过 preview 启动实验。

2026-10-20：按当前 patch 后的 `bm2dx.dll` 增加第四个构建配置。录制器/编码器 RTTI、原生导出写槽、两处录制资格判断及六个 Dan/Arena 场景虚表已逐项核对；在未执行游戏入口的 PE 装载测试中通过，连续录制和导出还通过模拟测试。实机仍需确认。
