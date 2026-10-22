# Orbbec Gemini Max 相机使用工具

这是一个根据Orbbec官方SDK开发的相机录制CLI工具，可以录制不同的视频流。

Orbbec Gemini Max支持USB3.0协议，建议使用USB3.0线缆和支持USB3.0协议的设备使用这个相机以避免带宽不足。

## 1. 环境概览

| 项目 | 状态 |
| --- | --- |
| 相机 | Orbbec Gemini Max（SV1301S_U3，SN: AY2R76300H4） |
| 当前连接 | **USB 2.0（480Mbps）**，插在 Bus 003 |
| udev 规则 | 已安装（`/etc/udev/rules.d/99-obsensor-libusb.rules`） |
| 查看工具 | OrbbecViewer v1.10.37 |
| 录制工具 | 自研 `orbbec_recorder`（基于官方 SDK v1.10.37 编译，支持自由选流和配置） |
| 解析工具 | `bag_extract.py` + rosbags 库（Python 3.11 venv） |

**目录结构**：

## 项目结构说明

```
orbbecTool/
├── .git/                        Git 仓库元数据（版本历史、分支等）
├── .gitignore                   忽略规则（本次重写）
├── .lock                        uv 创建 venv 时的空锁标记，运行不需要
├── CACHEDIR.TAG                 标准缓存目录标记，备份/索引工具看到后会跳过
├── LICENSE.md                   PolyForm Noncommercial 1.0.0（非商业许可）
├── pyvenv.cfg                   venv 配置：指向 uv 管理的 CPython 3.11.17
├── bin/                         venv 可执行文件
├── lib/                         venv 依赖包（约 242 MB）
├── lib64 -> lib                 Linux 多架构目录惯例，软链接到 lib
├── Log/                         Orbbec SDK 运行日志
├── OrbbecSDKConfig_v1.0.xml     SDK 全局配置（运行必需）
├── orbbec_recorder              已编译的录制器程序
├── orbbec_recorder.cpp          录制器 C++ 源码
├── bag_extract.py               bag 解析 / 转视频 / 抽帧脚本
└── README.md                   完整操作手册
```

### 核心文件

| 文件 | 作用 |
| --- | --- |
| [orbbec_recorder.cpp](file:///home/jianyuelinux/documents/code/projects/orbbecTool/orbbec_recorder.cpp) | 无界面录制器源码。调用 OrbbecSDK 的 `Pipeline`，支持任选彩色/深度/红外流、指定 `宽x高x帧率`、设置曝光/增益，录制为标准 ROSBAG V2.0，`Ctrl+C` 正常收尾。 |
| [orbbec_recorder](file:///home/jianyuelinux/documents/code/projects/orbbecTool/orbbec_recorder) | 上述源码编译出的 48 KB ELF。依赖 `libOrbbecSDK.so.1.10`，RUNPATH 写死为 `相机/sdk/OrbbecSDK_v1.10.37/SDK/lib`，所以换机器需保证该路径存在或重新编译。 |
| [bag_extract.py](file:///home/jianyuelinux/documents/code/projects/orbbecTool/bag_extract.py) | 用 `rosbags` 读 bag：默认自动找彩色 topic，可转 mp4 或按“每秒 N 张”抽 JPEG。`parse_compressed_image()` 专门跳过 Orbbec 的非标准 4 字节封装头。 |
| [OrbbecSDKConfig_v1.0.xml](file:///home/jianyuelinux/documents/code/projects/orbbecTool/OrbbecSDKConfig_v1.0.xml) | SDK 启动时从工作目录读取的配置（日志目录 `./Log`、级别与滚动大小，内存池 2048 MB、帧队列长度、Pipeline/Device 参数）。**不能删**，否则回到 SDK 默认配置。 |
| [README.md](file:///home/jianyuelinux/documents/code/projects/orbbecTool/README.md) | 环境、查看、录制参数、曝光控制、bag 解析、回放、相机全部配置、cron 定时录制、FAQ 与已知限制。 |
| [LICENSE.md](file:///home/jianyuelinux/documents/code/projects/orbbecTool/LICENSE.md) | 项目许可证，限定非商业用途。 |

### 目录

| 目录 | 作用 |
| --- | --- |
| `bin/` | venv 入口：`python`/`python3.11`、`pip`、`activate*` 脚本，以及依赖自带的 `rosbags-convert`、`numpy-config`、`f2py`、`apsw`。运行解析脚本用 `./bin/python bag_extract.py ...`。 |
| `lib/python3.11/site-packages/` | 实际依赖：`rosbags 0.11.8`（读 ROSBAG）、`opencv-python-headless 5.0`（解码 JPEG/写 mp4）、`numpy 2.4.6`、`apsw`（rosbags 的 SQLite 索引）、`lz4`、`backwards_zstd`。 |
| `lib64` | 指向 `lib` 的软链接，venv 标准结构。 |
| `Log/` | OrbbecSDK 运行时自动生成，如 `OrbbecSDK.log.txt`（当前约 1.1 MB）。按 XML 配置最多 3 个、每个 100 MB，可随时删除。 |
| `.git/` | 版本库；当前还没有任何提交。 |

### 与参考目录的对应关系

- `相机/OrbbecViewer_v1.10.37_.../`：官方 GUI 查看器，自带 SDK、FFmpeg、GLFW 等全套库和 udev 规则。用于实时预览和回放 `.bag`，**不在本仓库内**。
- `相机/recorder/`：本项目的原始工作目录，内容基本一致，但额外有 `sample_rgbd.bag` 和 `1/`～`9/` 抽帧输出；其 `.gitignore` 内容只有一个 `*`。
- `相机/sdk/OrbbecSDK_v1.10.37/`：官方头文件、库和示例，是编译 `orbbec_recorder.cpp` 及运行二进制所需的依赖，**也不在本仓库内**。

---

## 2. 查看实时画面（OrbbecViewer）

```bash
cd ~/documents/机器人队/相机/OrbbecViewer_v1.10.37_202607070725_linux_x64_release
LD_LIBRARY_PATH=. ./OrbbecViewer
```

- 启动后自动识别相机，左侧点击设备即可出图
- 每个流（Color / Depth / IR）面板上有 profile 下拉框，可切换分辨率和帧率
- 相机支持的完整配置见 [第 6 节](#6-相机支持的全部配置)

> **注意：OrbbecViewer 运行时会独占相机，此时无法录制。录制前请先关闭它。**

---

## 3. 录制（自由选择流类型 / 分辨率 / 帧率）

### 3.1 命令格式

```bash
cd ~/documents/机器人队/相机/recorder

./orbbec_recorder [输出.bag] [时长秒] [选项]
```

| 选项 | 说明 | 默认值 |
| --- | --- | --- |
| 第 1 个位置参数 | 输出 `.bag` 文件名 | 自动按时间戳命名 |
| 第 2 个位置参数 | 录制时长（秒） | 60 |
| `--streams color,depth,ir` | 选择录制哪些流（逗号分隔，可任选组合） | `color` |
| `--color 宽x高x帧率` | 彩色流配置 | `1920x1080x30` |
| `--depth 宽x高x帧率` | 深度流配置 | `640x400x30` |
| `--ir 宽x高x帧率` | 红外流配置 | `640x400x30` |
| `--color-exposure N` | 彩色曝光（自动关闭自动曝光） | 不设置 |
| `--depth-exposure N` | 深度曝光 | 不设置 |
| `--ir-exposure N` | 红外曝光 | 不设置 |
| `--color-gain N` | 彩色增益 | 不设置 |
| `--depth-gain N` | 深度增益 | 不设置 |
| `--ir-gain N` | 红外增益 | 不设置 |
| `--props` | 查询各路曝光/增益当前值和范围后退出 | — |
| `--list` | 列出相机支持的所有流配置后退出 | — |
| `--help` | 显示帮助 | — |

说明：

- 格式写成 `1920x1080x30`（宽x高x帧率），**指定了某路的配置即自动启用该路流**
- 像素格式由程序按流类型自动挑选最优（彩色优先 MJPG 压缩，深度/红外优先 Y16/Y11），无需手动指定
- 配置不存在时会报错并列出该路可用的配置
- 设置曝光/增益同样会自动启用对应流（详见 [3.5 曝光控制](#35-曝光控制快门速度)）
- 录制中随时 **Ctrl+C** 停止，文件正常保存；终端每秒打印各路帧数统计

### 3.2 常用示例

```bash
# 只录彩色 1080p30（最常用）
./orbbec_recorder my_record.bag 120

# 彩色 + 深度（默认配置）
./orbbec_recorder rgbd.bag 120 --streams color,depth

# 彩色降到 720p，深度提到 1280x800@15
./orbbec_recorder hd.bag 60 --color 1280x720x30 --depth 1280x800x15

# 只录深度
./orbbec_recorder depth.bag 60 --streams depth

# 只录红外
./orbbec_recorder ir.bag 60 --streams ir

# 三流同录
./orbbec_recorder all.bag 60 --streams color,depth,ir

# 低分辨率高帧率（彩色 60fps）
./orbbec_recorder fast.bag 30 --color 640x480x60

# 固定曝光录制（暗光/防抖，详见 3.5）
./orbbec_recorder fixed.bag 60 --color-exposure 100

# 先看相机支持哪些配置
./orbbec_recorder --list
```

### 3.3 输出文件说明

录制的 `.bag` 是**标准 ROSBAG V2.0 格式**（文件头 `#ROSBAG V2.0`），按启用的流生成对应 topic：

| Topic | 类型 | 内容 |
| --- | --- | --- |
| `/cam0/color_raw` | `sensor_msgs/msg/CompressedImage` | 彩色帧 |
| `/cam0/depth_raw` | `sensor_msgs/msg/CompressedImage` | 深度帧 |
| `/cam0/ir_raw` | `sensor_msgs/msg/CompressedImage` | 红外帧 |
| `/camera_params` | `custom_msg/msg/obCameraInfoList` | 相机内参 |
| `/device_info` | `custom_msg/msg/DeviceInfo` | 设备信息 |
| `/stream_info` | `custom_msg/msg/CameraProfile` | 流配置信息 |

### 3.4 文件大小估算（USB 2.0 实测，随画面内容浮动）

| 录制内容 | 码率 | 每分钟 |
| --- | --- | --- |
| 彩色 1080p30 | ~4.2 MB/s | ~250 MB |
| 彩色 720p30 | ~2 MB/s | ~120 MB |
| 彩色 640x480@60 | ~1.6 MB/s | ~100 MB |
| 深度 640x400@30 | ~1.8 MB/s | ~110 MB |
| 深度 1280x800@15 | ~0.7 MB/s | ~40 MB |
| 红外 640x400@30 | ~5.3 MB/s | ~320 MB |
| 三流同开（1080p30+640x400+640x400） | ~9.7 MB/s | ~580 MB |

长时间录制注意磁盘空间。

### 3.5 曝光控制（快门速度）

**可以调。** 彩色、深度、红外三路都有独立的曝光（快门速度）和增益设置，设置曝光时会自动关闭对应的自动曝光。

先查询当前值和可设范围：

```bash
./orbbec_recorder --props
```

本相机实测输出：

```
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

| 传感器 | 曝光范围 | 单位/行为 | 增益范围 |
| --- | --- | --- | --- |
| 彩色 | 1 ~ 5000 | **0.1ms**（157 ≈ 15.7ms，默认值即典型室内自动曝光） | 0 ~ 100 |
| 深度 | 0 ~ 10000 | 默认 3000；实测不影响帧率 | 0 ~ 9999 |
| 红外 | 0 ~ 10000 | 与深度同步（同传感器），行为相同 | 0 ~ 9999 |

**注意事项**：

1. **彩色曝光大于帧周期会掉帧**：30fps 的帧周期是 33.3ms（约 333），曝光设得越长帧率越低。实测：157 → 29fps；1570（157ms）→ 4.8fps；5000（500ms）→ 1.4fps。需要长曝光就要接受低帧率
2. **深度/红外曝光不影响帧率**：实测 300 和 10000 都跑满 30fps
3. **超范围值会被拒绝**：设置超出范围的值相机会忽略并保持原值，程序会打印"回读值"，**以回读值为实际生效值**
4. 曝光值会存在相机端，重启程序/电脑后仍保持（下次不设曝光就是上次的值）
5. 想恢复自动曝光：不传曝光参数重开一次程序，再在 OrbbecViewer 里把 Auto Exposure 打开（或在代码里 setBoolProperty 置 true）

**示例**：

```bash
# 固定彩色曝光 10ms（适合光照稳定的场景，避免自动曝光抖动）
./orbbec_recorder fixed.bag 60 --color-exposure 100

# 暗光环境：彩色长曝光 100ms（帧率会降到 ~8fps）+ 增益 50
./orbbec_recorder dark.bag 30 --color-exposure 1000 --color-gain 50 --color 640x480x30

# 固定深度曝光 5000 + 增益 3000
./orbbec_recorder depth_fixed.bag 60 --streams depth --depth-exposure 5000 --depth-gain 3000
```

---

## 4. 解析与转换

### 4.1 bag → mp4 视频

```bash
cd ~/documents/机器人队/相机/recorder

./bin/python bag_extract.py sample_rgbd.bag -o out.mp4
```

自动按录制实测帧率输出（实测 30.1 fps → 按 30 fps 写入），可用 `ffprobe out.mp4` 验证。

### 4.2 抽帧（每秒 N 张 JPEG）

```bash
# 每秒存 30 张 JPEG 到 frames/ 目录
./bin/python bag_extract.py sample_rgbd.bag --frames 30 -o frames/

# 只抽 1 秒 1 张做缩略图
./bin/python bag_extract.py sample_rgbd.bag --frames 1 -o thumbs/
```

文件名带时间戳：`f_000000_1792396277.102.jpg`

### 4.3 bag_extract.py 参数

| 参数 | 说明 |
| --- | --- |
| `bag` | 输入的 .bag 文件 |
| `-o / --out` | 输出 mp4 文件或帧目录（必填） |
| `--topic` | 指定 topic，默认自动找彩色流 `/cam0/color_raw` |
| `--fps` | 输出帧率，默认按录制实测值取整 |
| `--frames N` | 抽帧模式，每秒保存 N 张 JPEG |

### 4.4 在自己代码中解析（Python）

venv 里已装好 rosbags 库，直接读取即可（Orbbec 的消息封装头非标准，见 `bag_extract.py` 中的 `parse_compressed_image`）：

```python
from rosbags.rosbag1 import Reader
import struct, cv2, numpy as np

def parse_compressed_image(raw):
    off = 4  # 跳过封装头 (Orbbec 写的是 00 00 00 00)
    sec, nsec = struct.unpack_from('<iI', raw, off); off += 8
    (flen,) = struct.unpack_from('<I', raw, off); off += 4 + flen   # frame_id
    (flen,) = struct.unpack_from('<I', raw, off); off += 4          # format 长度
    off += flen
    (dlen,) = struct.unpack_from('<I', raw, off); off += 4
    return sec + nsec / 1e9, raw[off:off + dlen]

with Reader('sample_rgbd.bag') as r:
    for conn, ts, raw in r.messages():
        if conn.topic != '/cam0/color_raw':
            continue
        t, jpeg = parse_compressed_image(raw)
        frame = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
        # frame 即 1920x1080 BGR 图像, t 为时间戳(秒)
```

---

## 5. 回放

用 OrbbecViewer 打开录制的文件：

```bash
cd ~/documents/机器人队/相机/OrbbecViewer_v1.10.37_202607070725_linux_x64_release
LD_LIBRARY_PATH=. ./OrbbecViewer
# File -> Open File -> 选择 .bag
```

---

## 6. 相机支持的全部配置

运行 `./orbbec_recorder --list` 可随时查看。以下为完整列表（**帧率为 USB 2.0 下实测值**，随画面内容和负载浮动）：

### 6.1 彩色流（color）

格式：MJPG（压缩，推荐）/ RGB / BGRA

| 分辨率 | 标称帧率 | USB 2.0 实测 | 说明 |
| --- | --- | --- | --- |
| 1920×1080 | 30 | ~29 fps | 默认配置 |
| 1280×720 | 30 | ~27 fps | 带宽紧张时的折中 |
| 640×480 | 5 / 10 / 15 / 30 | 30 | |
| 640×480 | 60 | ~56 fps | 低分辨率高帧率 |

### 6.2 深度流（depth）

格式：Y11 / Y12（SDK 默认解包为 16bit 深度值，单位 mm）

| 分辨率 | 标称帧率 | USB 2.0 实测 | 说明 |
| --- | --- | --- | --- |
| 1280×800 | 15 / 30 | **~6 fps** | 官方 USB 2.0 规格即限 7fps，带宽不够 |
| 640×400 | 5 / 10 / 15 / 30 | ~29 fps | **推荐** |
| 640×400 | 60 | ~29 fps | 标称 60，USB 2.0 跑不满 |
| 320×200 | 5 / 10 / 15 / 30 / 60 | | 最低带宽 |

### 6.3 红外流（ir）

格式：Y10（解包为 16bit）

| 分辨率 | 标称帧率 | USB 2.0 实测 |
| --- | --- | --- |
| 1280×800 | 15 / 30 | ~6 fps |
| 640×400 | 5 / 10 / 15 / 30 | ~28 fps |
| 640×400 | 60 | ~29 fps |
| 320×200 | 5 / 10 / 15 / 30 / 60 | |

### 6.4 推荐组合

| 场景 | 命令 |
| --- | --- |
| 只要高清彩色 | `./orbbec_recorder out.bag 60` |
| RGB+D 标准 | `./orbbec_recorder out.bag 60 --streams color,depth` |
| RGB+D 带宽吃紧时 | `./orbbec_recorder out.bag 60 --color 1280x720x30 --depth 640x400x30` |
| 高帧率彩色 | `./orbbec_recorder out.bag 30 --color 640x480x60` |
| 只要深度 | `./orbbec_recorder out.bag 60 --streams depth` |
| 只要红外 | `./orbbec_recorder out.bag 60 --streams ir` |
| 三流全录 | `./orbbec_recorder out.bag 60 --streams color,depth,ir`（各 ~25fps） |

> 以上是 USB 2.0 下协商到的配置。Gemini Max 硬件支持 USB 3.0（更高带宽，深度可满帧 1280×800@30），换到 USB 3.0 端口 + USB 3.0 线后可用 `lsusb -t` 验证是否协商到 5000M/10000M。

---

## 7. 定时 / 自动录制

### 7.1 cron 定时任务

```bash
crontab -e
```

示例——每小时的整点录制 10 分钟，存到 `~/camera_records/`：

```cron
0 * * * * mkdir -p ~/camera_records && cd ~/documents/机器人队/相机/recorder && pkill -x OrbbecViewer 2>/dev/null; ./orbbec_recorder "$HOME/camera_records/rec_$(date +\%Y\%m\%d_\%H\%M).bag" 600 >> /tmp/orbbec_cron.log 2>&1
```

其他常用 schedule：

```cron
# 每天早上 8 点录 1 小时 RGB+D
0 8 * * * cd ~/documents/机器人队/相机/recorder && ./orbbec_recorder "$HOME/camera_records/rec_$(date +\%Y\%m\%d_\%H\%M).bag" 3600 --streams color,depth

# 开机后自动开始录 2 小时
@reboot sleep 30 && cd ~/documents/机器人队/相机/recorder && ./orbbec_recorder "$HOME/camera_records/boot_$(date +\%Y\%m\%d_\%H\%M).bag" 7200
```

注意：

- crontab 里 `%` 必须写成 `\%`
- **录制前确保 OrbbecViewer 已关闭**（设备独占），上面示例已内置 `pkill`
- 建议先用 `./orbbec_recorder test.bag 10` 手动验证一次，再配定时任务

### 7.2 循环分段录制

需要"一直录、每 N 分钟一个文件"时，可用 shell 循环：

```bash
while true; do
  ./orbbec_recorder "seg_$(date +%Y%m%d_%H%M%S).bag" 600 --streams color,depth
done
```

---

## 8. 接入自己的程序

### 8.1 编译环境

录制器源码 `recorder/orbbec_recorder.cpp` 可作模板，编译命令：

```bash
SDK=~/documents/机器人队/相机/sdk/OrbbecSDK_v1.10.37/SDK
g++ -std=c++11 -O2 your_program.cpp \
    -I "$SDK/include" -L "$SDK/lib" -lOrbbecSDK -lpthread \
    -o your_program -Wl,-rpath,"$SDK/lib"
```

### 8.2 关键 API（SDK 高层接口）

```cpp
#include "libobsensor/hpp/Pipeline.hpp"

ob::Pipeline pipe;
auto colorProfile = pipe.getStreamProfileList(OB_SENSOR_COLOR)
    ->getVideoStreamProfile(1920, 1080, OB_FORMAT_MJPG, 30);
auto config = std::make_shared<ob::Config>();
config->enableStream(colorProfile);   // 按需 enableStream 多路
pipe.start(config);

pipe.startRecord("out.bag");   // 开始录制（所有已启用流都会被录）
auto frameSet = pipe.waitForFrames(100);  // 取帧
pipe.stopRecord();             // 停止录制
pipe.stop();
```

曝光/增益控制（属性 ID 见 `SDK/include/libobsensor/h/Property.h`）：

```cpp
auto dev = pipe.getDevice();
// 查询范围: OBIntPropertyRange { cur, max, min, step, def }
auto range = dev->getIntPropertyRange(OB_PROP_COLOR_EXPOSURE_INT);
// 关自动曝光后设手动曝光 (彩色单位 0.1ms; 深度/红外用 OB_PROP_DEPTH/IR_EXPOSURE_INT)
dev->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, false);
dev->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, 100);   // 10ms
dev->setIntProperty(OB_PROP_COLOR_GAIN_INT, 16);
```

SDK 自带大量示例：`sdk/OrbbecSDK_v1.10.37/Example/cpp/`（Sample-Recorder、Sample-Playback、Sample-PointCloud、Sample-CommonUsages 等），API 文档在 `sdk/OrbbecSDK_v1.10.37/Doc/`。

> **不要 `pip install pyorbbecsdk`**：PyPI 上唯一的 1.3.2 版本打包有误（manylinux wheel 里装的是 macOS 二进制），Linux 下不可用。用 C++ SDK 或上面第 4.4 节的 rosbags 解析。

---

## 9. 常见问题

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| OrbbecViewer 报 `file too short` | 发布包的符号链接被拷贝破坏（.so 变成文本文件） | 把 <1KB 的 .so 文件按内容重建为 symlink；或重新从官方 zip 解压 |
| 报 `OB_USB_STATUS_ACCESS` | 缺少 udev 规则 | `sudo .../OrbbecViewer_*/script/install_udev_rules.sh` 后重新插拔 |
| 录制时提示打不开相机 | OrbbecViewer 正占用相机 | 先 `pkill -x OrbbecViewer` 再录 |
| `/dev/video*` 没有相机节点 | uvcvideo 驱动被 detach 后未重绑 | 物理重新插拔相机（仅影响 V4L2 方式，SDK 录制不受影响） |
| 帧率上不去 / `fps too low` | USB 2.0 带宽打满 | 降低分辨率/帧率，或换 USB 3.0 口 + USB 3.0 线 |
| 深度 1280×800 只有 ~6fps | 官方 USB 2.0 规格限制（7fps） | 用 640×400@30，或换 USB 3.0 |
| `[错误] 彩色 流不支持 ...` | 请求了相机没有的配置 | 按提示里的可用列表改参数，或先 `--list` 查看 |
| 设置了曝光但画面没变化/值不对 | 设置值超范围被相机拒绝 | 看程序打印的"回读值"，以它为准；先用 `--props` 查范围 |
| 彩色帧率突然变很低 | 曝光时间设得超过帧周期（30fps 上限约 333） | 减小曝光值，或降低目标帧率 |
| 重启后曝光不是自动了 | 曝光值保存在相机端 | 不传曝光参数启动，或在 OrbbecViewer 里打开 Auto Exposure |
| 录制的画面时间戳偏了约 1.5 小时 | 相机内部时钟未同步（该型号不支持时间同步属性） | 以 bag 的消息时间为准，或录制前后记录本机时间做对齐 |
