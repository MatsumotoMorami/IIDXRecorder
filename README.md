# IIDXRecorder

用于 IIDX 33 的原生录像插件。自动调用游戏内置的音视频录制流程，将录像保存为本地 MP4，便于后期剪辑。无需点击游戏里的录像按钮，保留现有 ASIO 音频设置。

## 功能

- 单曲录制：Standard、Step Up、PF 按单曲保存。
- 连续录制：段位、Arena 从第一曲开始，保留曲间过程，在离开总结果页时保存。
- 保存目录可选择直接存入指定路径，或按日期建立文件夹。
- 文件名包含日期、模式和歌名／段位，可选加入录制开始时间。
- 同名文件自动追加序号，避免覆盖。

Standard 录制、VEFX＋EFFECT 跳关保存和段位连续录制已实机验证。Step Up、PF、Arena，以及歌名／段位命名和自定义编码参数仍待实机验证。

## 使用

1. 从 [Releases](https://github.com/MatsumotoMorami/IIDXRecorder/releases) 下载 ZIP。
2. 将 `IIDXRecorder.dll` 和 `IIDXRecorder.ini` 解压到游戏目录。
3. 在启动器的 DLL 加载列表中加入 `IIDXRecorder.dll`。使用 Spice2x 命令行时，通过 `-k` 参数加载；已有其他 DLL 时保留它们。
4. 启动游戏。录像默认保存到 DLL 同目录下的 `recordings` 文件夹。

单曲录像在离开成绩页、原生录制收尾后保存；连续录像在离开总结果页后保存。升级前请备份自行修改过的 INI。

## 配置

编辑与 DLL 同目录的 `IIDXRecorder.ini`，修改后重启游戏。INI 内附每个选项的中文说明和可选值。

常用选项位于 `[Recorder]`：

```ini
OutputPath=recordings
DateFolders=0
FilenameTime=0
```

| 选项 | 说明 |
| --- | --- |
| `OutputPath` | 保存路径，可用相对路径或绝对路径 |
| `DateFolders` | `0` 直接保存；`1` 保存到 `YYYY-MM-DD` 子文件夹 |
| `FilenameTime` | `0` 文件名只含日期；`1` 加入时、分、秒 |

文件名示例：

```text
2026-09-15_Standard_冥.mp4
2026-09-15_段位_SP九段.mp4
2026-09-15_00-23-05_段位_SP九段.mp4
2026-09-15_Arena.mp4
```

日期和时间取录像开始时的本地时间。Arena 或名称无法读取时省略名称部分，同名文件追加 `_2`、`_3` 等序号。

`[Native]` 中的 `ContinuousCourses` 控制段位／Arena 连续录制。`FPS`、`Width`、`Height` 和 `AACBitrateKbps` 可配置原生编码参数，建议先保持 `0`，使用游戏默认值。

## 兼容性

当前版本针对特定 IIDX 33 `bm2dx.dll` 构建：PE 时间戳 `0x69c347cc`，映像大小 `0xBB3E000`。插件会检查版本和指令特征，不能保证兼容其他构建。

录像依赖游戏自身的原生编码环境。自动录制在正常收尾时导出文件，不保证强制结束游戏后仍能保存完整录像。诊断日志为 DLL 同目录下的 `IIDXRecorder.log`。

## 开发

源码位于 `source/`，构建方式见 [DEVELOPMENT.md](DEVELOPMENT.md)。
