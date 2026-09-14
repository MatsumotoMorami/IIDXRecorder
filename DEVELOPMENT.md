# 开发说明

当前版本：v0.6。此目录是后续开发使用的仓库；source/ 包含 DLL 和验证程序源码，IIDXRecorder.ini 为带中文说明的默认配置。

使用 Zig 0.14.1 在仓库根目录运行：

```powershell
./source/build.ps1 -ZigPath '你的 zig.exe 完整路径'
```

生成 IIDXRecorder.dll；编译产物已加入 .gitignore。游戏运行时不要替换已加载的 DLL。运行配置位置为 C:\IIDX\IIDXRecorder.ini。

验证程序 source/test_autoguard.cpp 和 source/test_naming.cpp 可用同一 Zig 编译，使用 -target x86_64-windows-gnu -O2 -static -std=c++17，并链接 -lmfplat -lmfuuid -lole32 -luuid；可执行文件放入 build/ 后运行。这两项为模拟与文件命名验证，不替代实机游玩。

已实机确认：Standard 音视频、VEFX＋EFFECT 跳关、段位连续录制。Arena 连续录制及新增名称元数据仍需实机验收。保持 ASIO 和游戏原生采集；强退保护最低优先级。

仓库不包含游戏 DLL、群插件、录像或编译工具。Git 仓库已初始化，未配置远程仓库。
