# Orbbec Gemini Max 相机录制工具

基于 Orbbec 官方 SDK v1.10.37 的无界面相机录制工具，配套 bag 解析脚本。支持自由选择彩色 / 深度 / 红外流及其分辨率、帧率和曝光，录制为标准 ROSBAG V2.0；录制结果可转换为 mp4 或按秒抽取 JPEG。

项目已内置 SDK 头文件与运行库（`sdk/`），克隆后无需另行下载官方 SDK 即可使用。

相机型号：Orbbec Gemini Max（SV1301S_U3，SN: AY2R76300H4）。硬件支持 USB 3.0，建议使用 USB 3.0 线缆和端口以避免带宽不足。

## 目录

- [1. 环境与目录](#1-环境与目录)
- [2. 快速开始](#2-快速开始)
- [3. 查看实时画面](#3-查看实时画面)
- [4. 录制](#4-录制)
- [5. 解析与转换](#5-解析与转换)
- [6. 回放](#6-回放)
- [7. 编译与二次开发](#7-编译与二次开发)
- [8. 常见问题](#8-常见问题)
- [License](#license)

---

## 1. 环境与目录

### 1.1 路径约定

本文命令默认使用以下路径，如你的目录不同请自行替换：

| 用途 | 路径 |
| --- | --- |
| 项目目录 | `~/documents/code/projects/orbbecTool` |
| Orbbec SDK（项目内置） | `./sdk`（头文件 + 运行库） |
| OrbbecViewer（可选） | `~/documents/机器人队/相机/OrbbecViewer_v1.10.37_202607070725_linux_x64_release` |

### 1.2 测试环境

| 项目 | 状态 |
| --- | --- |
| 操作系统 | Ubuntu（Wayland） |
| 相机 | Orbbec Gemini Max（SV1301S_U3，SN: AY2R76300H4） |
| USB 连接 | 测试时为 **USB 2.0（480Mbps）**，Bus 003 |
| udev 规则 | 已安装 `/etc/udev/rules.d/99-obsensor-libusb.rules` |
| SDK | OrbbecSDK v1.10.37（项目内置） |
| 查看 / 回放 | OrbbecViewer v1.10.37 |
| 解析环境 | Python 3.11 venv（rosbags + OpenCV + NumPy） |

### 1.3 目录结构

```text
orbbecTool/
├── bin/                          Python venv 可执行文件（python、pip、rosbags-convert 等）
├── lib/                          venv 依赖（rosbags、opencv-python-headless、numpy、apsw 等）
├── lib64 -> lib                  venv 标准软链接
├── pyvenv.cfg                    venv 配置（uv 管理的 CPython 3.11）
├── sdk/                          Orbbec SDK v1.10.37（项目内置）
│   ├── include/libobsensor/      头文件
│   ├── lib/                      动态库（libOrbbecSDK、libob_usb、liblive555 等）
│   └── LICENSE.txt               SDK 许可证
├── Log/                          OrbbecSDK 运行日志（自动生成，可删除）
├── OrbbecSDKConfig_v1.0.xml      SDK 全局配置（运行必需）
├── orbbec_recorder               已编译的录制器（使用相对 RUNPATH）
├── orbbec_recorder.cpp           录制器源码
├── bag_extract.py                bag 解析 / 转 mp4 / 抽帧脚本
├── install_udev_rules.sh         安装相机 udev 规则（需 sudo）
├── 99-obsensor-libusb.rules      udev 规则本体
├── LICENSE.md                    PolyForm Noncommercial 1.0.0
└── README.md                     本文档
```

`sdk/` 内已包含编译所需头文件和运行时动态库，二进制通过 `$ORIGIN/sdk/lib` 相对路径加载，因此无需安装或下载官方 SDK，整个项目目录也可随意移动。

> `bin/`、`lib/`、`pyvenv.cfg` 属于本地 Python venv，默认不纳入 git。录制器不依赖 Python，克隆后即可使用；新机器若要运行 `bag_extract.py`，需自行创建 Python 3.11 venv 并安装 `rosbags`、`opencv-python-headless`、`numpy`。

### 1.4 相机访问权限

首次使用若提示 `OB_USB_STATUS_ACCESS`，安装 udev 规则后重新插拔相机：

```bash
cd ~/documents/code/projects/orbbecTool
sudo ./install_udev_rules.sh
```

`99-obsensor-libusb.rules` 已随项目提供，脚本会自动复制到 `/etc/udev/rules.d/`。

---

## 2. 快速开始

```bash
cd ~/documents/code/projects/orbbecTool

# 1) 查看相机支持的流配置
./orbbec_recorder --list

# 2) 录制 10 秒彩色 1080p30
./orbbec_recorder test.bag 10

# 3) 转成 mp4
./bin/python bag_extract.py test.bag -o test.mp4
```

录制前请确认 OrbbecViewer 已关闭，它运行时会独占相机。

---

## 3. 查看实时画面

使用官方 OrbbecViewer：

```bash
cd ~/documents/机器人队/相机/OrbbecViewer_v1.10.37_202607070725_linux_x64_release
LD_LIBRARY_PATH=. ./OrbbecViewer
```

- 启动后自动识别相机，在左侧点击设备即可出图。
- Color / Depth / IR 面板都有 profile 下拉框，可实时切换分辨率与帧率。
- 完整支持列表见[第 4.6 节](#46-相机支持的流配置)。

> OrbbecViewer 运行时会独占相机，此时无法录制；录制前先关闭它（`pkill -x OrbbecViewer`）。

---

## 4. 录制

### 4.1 命令格式

```bash
cd ~/documents/code/projects/orbbecTool

./orbbec_recorder [输出.bag] [时长秒] [选项]
```

| 选项 | 说明 | 默认值 |
| --- | --- | --- |
| 第 1 个位置参数 | 输出 `.bag` 文件名 | 自动按时间戳命名 |
| 第 2 个位置参数 | 录制时长（秒） | `60` |
| `--streams color,depth,ir` | 选择录制的流，逗号分隔，可任选组合 | `color` |
| `--color 宽x高x帧率` | 彩色流配置 | `1920x1080x30` |
| `--depth 宽x高x帧率` | 深度流配置 | `640x400x30` |
| `--ir 宽x高x帧率` | 红外流配置 | `640x400x30` |
| `--color-exposure N` | 彩色曝光，设置后自动关闭自动曝光 | 不设置 |
| `--depth-exposure N` | 深度曝光 | 不设置 |
| `--ir-exposure N` | 红外曝光 | 不设置 |
| `--color-gain N` | 彩色增益 | 不设置 |
| `--depth-gain N` | 深度增益 | 不设置 |
| `--ir-gain N` | 红外增益 | 不设置 |
| `--out-dir DIR` | 输出 bag 目录，不存在时自动创建；相对输出名会拼接到该目录 | 当前目录 |
| `--log-dir DIR` | SDK 日志目录，不存在时自动创建 | `./Log` |
| `--config FILE` | SDK 配置文件路径 | 当前目录的 `OrbbecSDKConfig_v1.0.xml` |
| `--props` | 查询各路曝光 / 增益的当前值和范围后退出 | — |
| `--list` | 列出相机支持的所有流配置后退出 | — |
| `--help` | 显示帮助 | — |

行为说明：

- 配置格式为 `宽x高x帧率`，例如 `1920x1080x30`；**指定某路配置或曝光 / 增益即自动启用该路流**。
- 像素格式由程序自动选择：彩色优先 MJPG，深度 / 红外优先 Y16 / Y11，无需手动指定。
- 请求了相机不支持的配置时，程序会报错并列出该路可用配置。
- 录制中随时 **Ctrl+C** 停止，文件会正常保存；终端每秒打印各路帧数和实测帧率。
- 默认从当前工作目录读取 `OrbbecSDKConfig_v1.0.xml`；用 `--config` 可指定任意配置文件。
- `--out-dir`、`--log-dir` 会递归创建；如果第 1 个位置参数是绝对路径，则不受 `--out-dir` 影响。
- `--list` / `--props` 同样支持 `--config` 和 `--log-dir`，且这两个选项可以写在命令任意位置。

### 4.2 常用示例

```bash
# 只录彩色 1080p30（默认）
./orbbec_recorder my_record.bag 120

# 彩色 + 深度
./orbbec_recorder rgbd.bag 120 --streams color,depth

# 彩色 720p + 深度 1280x800@15
./orbbec_recorder hd.bag 60 --color 1280x720x30 --depth 1280x800x15

# 只录深度
./orbbec_recorder depth.bag 60 --streams depth

# 只录红外
./orbbec_recorder ir.bag 60 --streams ir

# 三流同录
./orbbec_recorder all.bag 60 --streams color,depth,ir

# 低分辨率高帧率（彩色 60fps）
./orbbec_recorder fast.bag 30 --color 640x480x60

# 固定曝光录制
./orbbec_recorder fixed.bag 60 --color-exposure 100

# 分别指定输出目录、日志目录和 SDK 配置文件
./orbbec_recorder run.bag 60 --out-dir ~/camera_records --log-dir ~/camera_logs --config ./OrbbecSDKConfig_v1.0.xml

# 不指定文件名时按时间戳命名，并写入指定目录
./orbbec_recorder --out-dir ~/camera_records --log-dir ~/camera_logs

# 查看可用配置（同样支持自定义目录）
./orbbec_recorder --list
```

### 4.3 输出文件

录制的 `.bag` 是标准 **ROSBAG V2.0**（文件头 `#ROSBAG V2.0`），按启用的流生成 topic：

| Topic | 消息类型 | 内容 |
| --- | --- | --- |
| `/cam0/color_raw` | `sensor_msgs/msg/CompressedImage` | 彩色帧 |
| `/cam0/depth_raw` | `sensor_msgs/msg/CompressedImage` | 深度帧 |
| `/cam0/ir_raw` | `sensor_msgs/msg/CompressedImage` | 红外帧 |
| `/camera_params` | `custom_msg/msg/obCameraInfoList` | 相机内参 |
| `/device_info` | `custom_msg/msg/DeviceInfo` | 设备信息 |
| `/stream_info` | `custom_msg/msg/CameraProfile` | 流配置信息 |

### 4.4 磁盘占用估算

USB 2.0 实测，随画面内容浮动：

| 录制内容 | 码率 | 每分钟 |
| --- | --- | --- |
| 彩色 1920x1080@30 | ~4.2 MB/s | ~250 MB |
| 彩色 1280x720@30 | ~2 MB/s | ~120 MB |
| 彩色 640x480@60 | ~1.6 MB/s | ~100 MB |
| 深度 640x400@30 | ~1.8 MB/s | ~110 MB |
| 深度 1280x800@15 | ~0.7 MB/s | ~40 MB |
| 红外 640x400@30 | ~5.3 MB/s | ~320 MB |
| 三流同开（1080p30 + 640x400 + 640x400） | ~9.7 MB/s | ~580 MB |

长时间录制前请确认磁盘剩余空间。

### 4.5 曝光与增益

彩色、深度、红外三路都有独立的曝光（快门速度）和增益。设置曝光时会自动关闭对应的自动曝光。

先查询当前值和可设范围：

```bash
./orbbec_recorder --props
```

本机实测输出：

```text
== 彩色 ==
  自动曝光: 开
  曝光: 当前 157 | 范围 1 ~ 5000 (步进 1, 默认 157)
  增益: 当前 0 | 范围 0 ~ 100 (步进 1, 默认 0)
== 深度 ==
  曝光: 当前 3000 | 范围 0 ~ 10000 (步进 1, 默认 3000)
  增益: 当前 2000 | 范围 0 ~ 9999 (步进 1, 默认 2000)
== 红外 ==
  曝光: 当前 3000 | 范围 0 ~ 10000 (步进 1, 默认 3000)
  增益: 当前 2000 | 范围 0 ~ 9999 (步进 1, 默认 2000)
```

| 传感器 | 曝光范围 | 单位 / 行为 | 增益范围 |
| --- | --- | --- | --- |
| 彩色 | 1 ~ 5000 | **0.1 ms**（157 ≈ 15.7 ms） | 0 ~ 100 |
| 深度 | 0 ~ 10000 | 默认 3000；实测不影响帧率 | 0 ~ 9999 |
| 红外 | 0 ~ 10000 | 与深度同传感器，行为相同 | 0 ~ 9999 |

注意事项：

1. **彩色曝光超过帧周期会掉帧**：30fps 的帧周期约 33.3 ms（对应曝光值约 333）。实测 157 → 29fps，1570（157 ms）→ 4.8fps，5000（500 ms）→ 1.4fps。
2. **深度 / 红外曝光不影响帧率**：曝光 300 和 10000 均能跑满 30fps。
3. **超范围值会被相机拒绝**：程序会打印回读值，以回读值为实际生效值。
4. 曝光值保存在相机端，重启程序或电脑后仍保持。
5. 恢复自动曝光：不传曝光参数重新启动程序，然后在 OrbbecViewer 中打开 Auto Exposure（或代码中 `setBoolProperty(..., true)`）。

示例：

```bash
# 固定彩色曝光 10ms（光照稳定场景，避免自动曝光抖动）
./orbbec_recorder fixed.bag 60 --color-exposure 100

# 暗光：彩色曝光 100ms + 增益 50（帧率会明显下降）
./orbbec_recorder dark.bag 30 --color-exposure 1000 --color-gain 50 --color 640x480x30

# 固定深度曝光 5000 + 增益 3000
./orbbec_recorder depth_fixed.bag 60 --streams depth --depth-exposure 5000 --depth-gain 3000
```

### 4.6 相机支持的流配置

可随时运行 `./orbbec_recorder --list` 查看。以下帧率为 **USB 2.0 实测值**，会随画面内容和负载浮动。

#### 彩色流（color）

格式：MJPG（压缩，推荐）/ RGB / BGRA

| 分辨率 | 标称帧率 | USB 2.0 实测 | 说明 |
| --- | --- | --- | --- |
| 1920x1080 | 30 | ~29 fps | 默认配置 |
| 1280x720 | 30 | ~27 fps | 带宽紧张时的折中 |
| 640x480 | 5 / 10 / 15 / 30 | 30 fps | |
| 640x480 | 60 | ~56 fps | 低分辨率高帧率 |

#### 深度流（depth）

格式：Y11 / Y12，SDK 默认解包为 16bit 深度值，单位 mm。

| 分辨率 | 标称帧率 | USB 2.0 实测 | 说明 |
| --- | --- | --- | --- |
| 1280x800 | 15 / 30 | **~6 fps** | 官方 USB 2.0 规格限制为 7fps |
| 640x400 | 5 / 10 / 15 / 30 | ~29 fps | **推荐** |
| 640x400 | 60 | ~29 fps | 标称 60，USB 2.0 跑不满 |
| 320x200 | 5 / 10 / 15 / 30 / 60 | | 最低带宽 |

#### 红外流（ir）

格式：Y10，解包为 16bit。

| 分辨率 | 标称帧率 | USB 2.0 实测 |
| --- | --- | --- |
| 1280x800 | 15 / 30 | ~6 fps |
| 640x400 | 5 / 10 / 15 / 30 | ~28 fps |
| 640x400 | 60 | ~29 fps |
| 320x200 | 5 / 10 / 15 / 30 / 60 | |

#### 推荐组合

| 场景 | 命令 |
| --- | --- |
| 只要高清彩色 | `./orbbec_recorder out.bag 60` |
| RGB+D 标准 | `./orbbec_recorder out.bag 60 --streams color,depth` |
| RGB+D 带宽吃紧 | `./orbbec_recorder out.bag 60 --color 1280x720x30 --depth 640x400x30` |
| 高帧率彩色 | `./orbbec_recorder out.bag 30 --color 640x480x60` |
| 只要深度 | `./orbbec_recorder out.bag 60 --streams depth` |
| 只要红外 | `./orbbec_recorder out.bag 60 --streams ir` |
| 三流全录 | `./orbbec_recorder out.bag 60 --streams color,depth,ir`（各约 25fps） |

### 4.7 定时 / 自动录制

cron 示例：每小时整点录制 10 分钟到 `~/camera_records/`。

```cron
0 * * * * mkdir -p ~/camera_records && cd ~/documents/code/projects/orbbecTool && pkill -x OrbbecViewer 2>/dev/null; ./orbbec_recorder "$HOME/camera_records/rec_$(date +\%Y\%m\%d_\%H\%M).bag" 600 >> /tmp/orbbec_cron.log 2>&1
```

其他常用 schedule：

```cron
# 每天早上 8 点录 1 小时 RGB+D
0 8 * * * cd ~/documents/code/projects/orbbecTool && ./orbbec_recorder "$HOME/camera_records/rec_$(date +\%Y\%m\%d_\%H\%M).bag" 3600 --streams color,depth

# 开机后延迟 30 秒开始录 2 小时
@reboot sleep 30 && cd ~/documents/code/projects/orbbecTool && ./orbbec_recorder "$HOME/camera_records/boot_$(date +\%Y\%m\%d_\%H\%M).bag" 7200
```

注意事项：

- crontab 中的 `%` 必须转义为 `\%`。
- 录制前确保 OrbbecViewer 已关闭，上面的示例已内置 `pkill`。
- 建议先手动运行 `./orbbec_recorder test.bag 10` 验证，再配置定时任务。

需要“持续录制、每 N 分钟一个文件”时，可用 shell 循环：

```bash
while true; do
  ./orbbec_recorder "seg_$(date +%Y%m%d_%H%M%S).bag" 600 --streams color,depth
done
```

---

## 5. 解析与转换

`bag_extract.py` 使用 venv 中的 rosbags 读取 bag，输出 mp4 或 JPEG 序列。

### 5.1 bag 转 mp4

```bash
cd ~/documents/code/projects/orbbecTool

./bin/python bag_extract.py out.bag -o out.mp4
```

默认按录制实测帧率取整写入（例如实测 30.1 fps → 按 30 fps），可用 `ffprobe out.mp4` 验证。

### 5.2 抽帧

```bash
# 每秒保存 30 张 JPEG 到 frames/
./bin/python bag_extract.py out.bag --frames 30 -o frames/

# 每秒 1 张做缩略图
./bin/python bag_extract.py out.bag --frames 1 -o thumbs/
```

文件名带时间戳，例如 `f_000000_1792396277.102.jpg`。

### 5.3 参数

| 参数 | 说明 |
| --- | --- |
| `bag` | 输入的 `.bag` 文件 |
| `-o / --out` | 输出 mp4 文件或帧目录（必填） |
| `--topic` | 指定 topic，默认自动查找彩色流 `/cam0/color_raw` |
| `--fps` | 输出帧率，默认按录制实测值取整 |
| `--frames N` | 抽帧模式，每秒保存 N 张 JPEG |

示例：导出深度流

```bash
./bin/python bag_extract.py out.bag --topic /cam0/depth_raw -o depth.mp4
```

### 5.4 在自己的 Python 代码中读取

venv 中已装好 `rosbags`。Orbbec 的消息封装头非标准，读取时需跳过开头 4 字节：

```python
from rosbags.rosbag1 import Reader
import struct, cv2, numpy as np

def parse_compressed_image(raw):
    off = 4  # 跳过封装头（Orbbec 写的是 00 00 00 00）
    sec, nsec = struct.unpack_from('<iI', raw, off); off += 8
    (flen,) = struct.unpack_from('<I', raw, off); off += 4 + flen   # frame_id
    (flen,) = struct.unpack_from('<I', raw, off); off += 4          # format 长度
    off += flen
    (dlen,) = struct.unpack_from('<I', raw, off); off += 4
    return sec + nsec / 1e9, raw[off:off + dlen]

with Reader('out.bag') as r:
    for conn, ts, raw in r.messages():
        if conn.topic != '/cam0/color_raw':
            continue
        t, jpeg = parse_compressed_image(raw)
        frame = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
        # frame 即 BGR 图像，t 为时间戳（秒）
```

---

## 6. 回放

用 OrbbecViewer 打开录制的 bag：

```bash
cd ~/documents/机器人队/相机/OrbbecViewer_v1.10.37_202607070725_linux_x64_release
LD_LIBRARY_PATH=. ./OrbbecViewer
# File -> Open File -> 选择 .bag
```

---

## 7. 编译与二次开发

### 7.1 编译录制器

`orbbec_recorder.cpp` 可直接作为模板：

```bash
cd ~/documents/code/projects/orbbecTool
SDK=./sdk

g++ -std=c++11 -O2 orbbec_recorder.cpp \
    -I "$SDK/include" -L "$SDK/lib" -lOrbbecSDK -lpthread \
    -o orbbec_recorder -Wl,-rpath,'$ORIGIN/sdk/lib'
```

- SDK 头文件和动态库已内置在 `./sdk`，无需另行下载。
- `$ORIGIN` 必须放在单引号中，确保写入的是相对路径；这样整个项目目录可以任意移动或被克隆到其他机器。
- 预编译的 `orbbec_recorder` 已使用 `$ORIGIN/sdk/lib`，克隆后可直接运行。
- 如替换 SDK 版本，保持 `sdk/include/libobsensor/` 和 `sdk/lib/` 目录结构即可。

### 7.2 关键 API

```cpp
#include "libobsensor/hpp/Pipeline.hpp"

ob::Pipeline pipe;
auto colorProfile = pipe.getStreamProfileList(OB_SENSOR_COLOR)
    ->getVideoStreamProfile(1920, 1080, OB_FORMAT_MJPG, 30);
auto config = std::make_shared<ob::Config>();
config->enableStream(colorProfile);        // 需要多路就多次 enableStream
pipe.start(config);

pipe.startRecord("out.bag");               // 已启用的流都会录制
auto frameSet = pipe.waitForFrames(100);   // 取帧
pipe.stopRecord();
pipe.stop();
```

曝光 / 增益控制（属性 ID 见 `sdk/include/libobsensor/h/Property.h`）：

```cpp
auto dev = pipe.getDevice();
// 查询范围：OBIntPropertyRange { cur, max, min, step, def }
auto range = dev->getIntPropertyRange(OB_PROP_COLOR_EXPOSURE_INT);
// 先关自动曝光，再设手动曝光；彩色单位为 0.1ms
dev->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, false);
dev->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, 100);   // 10ms
dev->setIntProperty(OB_PROP_COLOR_GAIN_INT, 16);
```

---

## 8. 常见问题

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| OrbbecViewer 报 `file too short` | 发布包符号链接被拷贝破坏（`.so` 变成文本文件） | 将小于 1KB 的 `.so` 按内容重建为软链接，或重新解压官方 zip |
| 报 `OB_USB_STATUS_ACCESS` | 缺少 udev 规则 | 运行 `script/install_udev_rules.sh` 后重新插拔相机 |
| 录制时提示打不开相机 | OrbbecViewer 正占用相机 | 先 `pkill -x OrbbecViewer` 再录制 |
| `/dev/video*` 没有相机节点 | uvcvideo 驱动被 detach 后未重绑 | 物理重新插拔相机（只影响 V4L2，SDK 录制不受影响） |
| 帧率上不去 / `fps too low` | USB 2.0 带宽打满 | 降低分辨率或帧率，或换 USB 3.0 端口和线缆 |
| 深度 1280x800 只有 ~6fps | 官方 USB 2.0 规格限制（7fps） | 改用 640x400@30，或换 USB 3.0 |
| `[错误] 彩色 流不支持 ...` | 请求了相机不支持的配置 | 按提示的可用列表调整参数，或先用 `--list` 查看 |
| 设置了曝光但画面没变化 / 值不对 | 设置值超范围被相机拒绝 | 以程序打印的回读值为准，先用 `--props` 查范围 |
| 彩色帧率突然变很低 | 曝光时间超过帧周期（30fps 上限约 333） | 减小曝光值，或降低目标帧率 |
| 重启后曝光不是自动了 | 曝光值保存在相机端 | 不传曝光参数启动，或在 OrbbecViewer 中打开 Auto Exposure |
| 录制的画面时间戳偏约 1.5 小时 | 相机内部时钟未同步（该型号不支持时间同步属性） | 以 bag 消息时间为准，或在录制前后记录本机时间做对齐 |

## License

本项目使用 **PolyForm Noncommercial License 1.0.0**。

- 允许个人学习、爱好、学术研究、非营利组织和政府机构使用。
- **禁止商业使用、企业内部生产部署、付费 SaaS 和商业打包。**
- 如需商业授权，请联系：<jianyuewushuang@163.com>。

完整条款见 [LICENSE.md](./LICENSE.md)。

### 第三方组件

`sdk/` 中的 Orbbec SDK v1.10.37 头文件与动态库按 Orbbec 随附的许可证分发，详见 [sdk/LICENSE.txt](./sdk/LICENSE.txt)。
