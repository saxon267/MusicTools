# Cardputer 五度圈固件（Circle of Fifths）

给 M5Stack **Cardputer / Cardputer-Adv** 的五度圈查询固件。开机即用，无网络、无菜单层级。

- 左侧 12 段圆环：外环 12 个大调，内环对应关系小调；当前调高亮并放大到中心
- 右侧：当前调的 7 个级数和弦（罗马数字 + 和弦名 + 构成音）
- 按 Enter 试听当前和弦（内置合成器，4 声部）
- 状态栏显示调号、电池电量、播放状态

![界面](docs/ui-mockup-ring-1x.png)

---

## 一、刷机

固件文件：`dist/cardputer-circle-of-fifths-8MB.bin`（**从 0x0 地址烧写**，已合并 bootloader + 分区表 + 应用）

### 方式 A：浏览器烧写（推荐，不用装任何环境）

1. 用 Chrome / Edge 打开 <https://espressif.github.io/esptool-js/>
2. 点 **Connect**，选择 Cardputer 的串口（COMx）
3. 选择文件 `cardputer-circle-of-fifths-8MB.bin`
4. **Flash Address 填 `0x0`**
5. 点 **Program**，等待进度条到 100%

### 方式 B：esptool 命令行

```bash
pip install esptool
esptool.py --chip esp32s3 -p COM3 -b 460800 write_flash 0x0 cardputer-circle-of-fifths-8MB.bin
```

（把 `COM3` 换成设备管理器中看到的串口）

### 方式 C：M5Burner

M5Burner → 选择设备 → **Custom** / 本地固件 → 选 `cardputer-circle-of-fifths-8MB.bin` → 烧录地址 `0x0`。

### 进不去下载模式时

按住侧面 **G0** 按钮再插入 USB-C，或按一下复位键后立即按住 G0；烧写完成后按复位。

---

## 二、操作

| 按键 | 功能 |
|---|---|
| `Fn` + `,` / `Fn` + `.` | 上一个 / 下一个调（沿五度圈移动） |
| `Fn` + `;` / `Fn` + `/` | 上一 / 下一个级数和弦 |
| `1` … `7` | 直接跳到第 n 级和弦 |
| `m` | 大调 / 关系小调视图切换 |
| `Enter` 或 `空格` | 试听当前和弦 |
| 侧面按钮（G0） | 切换下一个调并试听（键盘失效时的备用操作） |

按住 `Fn+,`/`Fn+.` 会自动连发。

---

## 三、界面读法

```
外环：C  G  D  A  E  B  F# | Db Ab Eb Bb F      ← 12 个大调（顺时针为升号方向）
内环：a  e  b  f# c# g# d# | bb f  c  g  d       ← 对应关系小调（小写=小调）
中心：当前调名 + 调号（0 / 3# / 4b）
右栏：I C C E G / ii Dm D F A / ... / vii° Bdim B D F
```

调号数量由音名拼写自动推导，不是查表：F# 大调会显示 `F# G# A# B C# D# E#`，Db 大调显示 `Db Eb F Gb Ab Bb C`，不会出现 `A# 大调` 这种错误拼写。

---

## 四、从源码构建

```bash
cd firmware
pio run                # 编译
pio run -t upload      # 直接烧写
```

构建产物：`.pio/build/cardputer/firmware.bin`（应用镜像，从 0x10000 烧写）。
`dist/` 里的合并镜像由 `tools/make_bin.ps1` 生成。

依赖：PlatformIO + `espressif32@6.9.0`；库 `M5Unified 0.3.0`、`M5GFX 0.3.0`、`M5Cardputer 1.1.1`（自动安装）。

---

## 五、代码结构

| 文件 | 职责 |
|---|---|
| `src/theory.h/.cpp` | 纯 C++ 乐理层，无硬件依赖：音名拼写、音阶、级数和弦、调号 |
| `src/ui.h/.cpp` | 240×135 界面：圆环、侧栏、状态栏，全部绘制到 sprite 后一次性推送 |
| `src/audio.h/.cpp` | 和弦合成（正弦叠加 + ADSR），16 kHz 单声道 |
| `src/main.cpp` | 按键边沿检测、状态机、主循环 |

资源占用：RAM 15.9%（52 KB 静态 + 约 93 KB 堆）、Flash 17.7%（557 KB / 3 MB）。

---

## 六、已知限制（重要）

1. **没有在真机上跑过** —— 编译、链接、镜像校验都通过，但生成固件时本机没有连接 Cardputer，实际显示/按键/出声未经验证。
2. **依赖 M5GFX 的板卡自动识别**：固件用 `M5.getBoard()` 区分一代（GPIO 矩阵键盘）和 Adv（TCA8418 键盘）。识别失败时键盘不会响应，此时侧面按钮仍可切换调。
3. **音频**：Adv 走 ES8311（M5Unified 内部回调），一代走 NS4148。若无声，先看串口日志确认 `spk=1`。
4. **串口日志**：115200 波特率，开机会打印 `[fifths] board=.. panel=..x.. sprite=1 spk=1`，排障先看这一行。

---

## 七、许可证

MIT，见 [LICENSE](LICENSE)。

界面字体使用 M5GFX 内置的 `Font0` / `Font2` / `Font4` / `FreeSans9pt7b`（Adafruit GFX 字体集，源自 GNU FreeFont，GPLv3 + font exception），随库分发。
