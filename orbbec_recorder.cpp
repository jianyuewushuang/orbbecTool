// Orbbec Gemini Max 无界面录制器 (基于官方 OrbbecSDK v1.10.37)
// 可自由选择录制的流类型 (彩色/深度/红外) 及每路流的分辨率、帧率、曝光
//
// 用法:
//   ./orbbec_recorder [输出.bag] [时长秒] [选项]
//
// 流选择:
//   --streams color,depth,ir   选择要录制的流 (默认: color)
//   --color 1920x1080x30       彩色流配置 (宽x高x帧率)
//   --depth 640x400x30         深度流配置
//   --ir    640x400x30         红外流配置
//
// 目录/配置 (均可独立指定):
//   --out-dir DIR              输出 bag 的目录 (默认: 当前目录)
//   --log-dir DIR              SDK 日志目录 (默认: ./Log)
//   --config FILE              SDK 配置文件路径 (默认: ./OrbbecSDKConfig_v1.0.xml)
//
// 曝光控制 (快门速度, 设置后自动关闭对应自动曝光):
//   --color-exposure 1000      彩色曝光值
//   --depth-exposure 3000      深度曝光值
//   --ir-exposure 3000         红外曝光值
//   --color-gain 16            彩色增益
//   --depth-gain 16            深度增益
//   --ir-gain 16               红外增益
//   --props                    查询各路曝光/增益当前值和范围后退出
//   --list                     列出相机支持的所有流配置后退出
//
// 示例:
//   ./orbbec_recorder                                  # 彩色 1080p30, 60秒
//   ./orbbec_recorder out.bag 120 --streams color,depth
//   ./orbbec_recorder out.bag 60 --color-exposure 2000 --color-gain 16
//   ./orbbec_recorder out.bag 60 --depth-exposure 5000
//   ./orbbec_recorder --props                          # 先看曝光范围
//
// 录制的 .bag 为标准 ROSBAG V2.0, 可用 OrbbecViewer (File->Open) 回放
// Ctrl+C 可随时停止, 文件会正常保存

#include "libobsensor/hpp/Pipeline.hpp"
#include "libobsensor/hpp/Context.hpp"
#include "libobsensor/hpp/StreamProfile.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "libobsensor/hpp/Frame.hpp"
#include "libobsensor/hpp/Device.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>

static volatile sig_atomic_t g_running = 1;
static void onSignal(int) { g_running = 0; }

static std::string timestampName() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    strftime(buf, sizeof(buf), "orbbec_%Y%m%d_%H%M%S.bag", localtime(&t));
    return buf;
}

// ---------- 目录工具 ----------
// 支持 "~" / "~/xxx" 形式, 便于直接传入用户目录
static std::string expandHome(const std::string &path) {
    if(path == "~" || path.compare(0, 2, "~/") == 0) {
        const char *home = getenv("HOME");
        if(home && *home) return std::string(home) + path.substr(1);
    }
    return path;
}

static bool isDir(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// 递归创建目录, 等价于 mkdir -p
static bool makeDirs(const std::string &path) {
    if(path.empty()) return true;
    std::string p = path;
    while(p.size() > 1 && p[p.size() - 1] == '/') p.pop_back();
    if(isDir(p)) return true;
    size_t pos = p.rfind('/');
    if(pos != std::string::npos && pos > 0) {
        if(!makeDirs(p.substr(0, pos))) return false;
    }
    if(mkdir(p.c_str(), 0755) == 0 || errno == EEXIST) return isDir(p);
    return false;
}

// 相对输出名拼接到输出目录; 绝对路径保持原样
static std::string joinPath(const std::string &dir, const std::string &name) {
    if(dir.empty() || name.empty() || name[0] == '/') return name;
    if(dir[dir.size() - 1] == '/') return dir + name;
    return dir + "/" + name;
}

// ---------- SDK 会话 ----------
struct SdkSession {
    std::shared_ptr<ob::Context>  ctx;
    std::shared_ptr<ob::Device>   dev;
    std::unique_ptr<ob::Pipeline> pipe;
};

// 未指定 configPath 时保持 SDK 默认行为 (从当前目录读取 OrbbecSDKConfig_v1.0.xml)
static SdkSession openSdk(const std::string &configPath, const std::string &logDir) {
    SdkSession s;

    // 先设置一次以捕获 Context 创建前的日志
    if(!logDir.empty()) ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, logDir.c_str());

    if(!configPath.empty()) s.ctx = std::make_shared<ob::Context>(configPath.c_str());

    // 配置文件中的 OutputDir 可能重置日志目录, 因此 Context 创建后再设置一次
    if(!logDir.empty() && s.ctx) ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, logDir.c_str());

    if(s.ctx) {
        auto list = s.ctx->queryDeviceList();
        if(!list || list->deviceCount() == 0) throw std::runtime_error("未发现相机设备");
        s.dev = list->getDevice(0);
        s.pipe.reset(new ob::Pipeline(s.dev));
    } else {
        s.pipe.reset(new ob::Pipeline());
    }
    return s;
}

static const char *formatName(OBFormat f) {
    switch(f) {
        case OB_FORMAT_YUYV: return "YUYV";
        case OB_FORMAT_MJPG: return "MJPG";
        case OB_FORMAT_H264: return "H264";
        case OB_FORMAT_H265: return "H265";
        case OB_FORMAT_Y16:  return "Y16";
        case OB_FORMAT_Y8:   return "Y8";
        case OB_FORMAT_Y10:  return "Y10";
        case OB_FORMAT_Y11:  return "Y11";
        case OB_FORMAT_Y12:  return "Y12";
        case OB_FORMAT_Y14:  return "Y14";
        case OB_FORMAT_GRAY: return "GRAY";
        case OB_FORMAT_I420: return "I420";
        case OB_FORMAT_RGB:  return "RGB";
        case OB_FORMAT_BGR:  return "BGR";
        case OB_FORMAT_BGRA: return "BGRA";
        case OB_FORMAT_Z16:  return "Z16";
        case OB_FORMAT_YV12: return "YV12";
        case OB_FORMAT_BA81: return "BA81";
        default: {
            static char buf[16];
            snprintf(buf, sizeof(buf), "FMT_%d", (int)f);
            return buf;
        }
    }
}

// ---------- 流定义 ----------
struct StreamDef {
    const char  *name;
    const char  *label;
    OBSensorType type;
    OBFormat     pref[4];
};

static const StreamDef STREAMS[] = {
    {"color", "彩色", OB_SENSOR_COLOR, {OB_FORMAT_MJPG, OB_FORMAT_RGB, OB_FORMAT_BGR, OB_FORMAT_YUYV}},
    {"depth", "深度", OB_SENSOR_DEPTH, {OB_FORMAT_Y16, OB_FORMAT_Y11, OB_FORMAT_Y10, OB_FORMAT_Y8}},
    {"ir",    "红外", OB_SENSOR_IR,    {OB_FORMAT_Y16, OB_FORMAT_Y10, OB_FORMAT_Y11, OB_FORMAT_Y8}},
};
static const int NSTREAMS = 3;

// ---------- 曝光/增益属性定义 ----------
struct PropDef {
    OBPropertyID exposure;
    OBPropertyID autoExposure;
    OBPropertyID gain;
};
static const PropDef PROPS[NSTREAMS] = {
    {OB_PROP_COLOR_EXPOSURE_INT, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_PROP_COLOR_GAIN_INT},
    {OB_PROP_DEPTH_EXPOSURE_INT, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PROP_DEPTH_GAIN_INT},
    {OB_PROP_IR_EXPOSURE_INT,    OB_PROP_IR_AUTO_EXPOSURE_BOOL,    OB_PROP_IR_GAIN_INT},
};

struct Spec {
    bool enabled = false;
    int  w = 0, h = 0, fps = 0;
    int  exposure = -1;  // -1 = 不设置(保持自动)
    int  gain     = -1;
};

static bool enumProfiles(ob::Pipeline &pipe, OBSensorType type,
                         std::vector<std::shared_ptr<ob::VideoStreamProfile>> &out) {
    out.clear();
    auto list = pipe.getStreamProfileList(type);
    if(!list) return false;
    for(uint32_t i = 0; i < list->count(); i++) {
        auto v = list->getProfile(i)->as<ob::VideoStreamProfile>();
        if(v) out.push_back(v);
    }
    return true;
}

// 按 WxHxFPS 精确匹配, 格式按 STREAMS 优先级挑选; 失败返回 nullptr 并打印可用项
static std::shared_ptr<ob::VideoStreamProfile> pickProfile(ob::Pipeline &pipe, const StreamDef &def,
                                                           int w, int h, int fps) {
    std::vector<std::shared_ptr<ob::VideoStreamProfile>> all;
    if(!enumProfiles(pipe, def.type, all)) {
        fprintf(stderr, "[错误] 获取 %s 流列表失败\n", def.label);
        return nullptr;
    }
    for(OBFormat fmt : def.pref) {
        for(auto &v : all)
            if(v->width() == w && v->height() == h && v->fps() == fps && v->format() == fmt) return v;
    }
    for(auto &v : all)
        if(v->width() == w && v->height() == h && v->fps() == fps) return v;

    fprintf(stderr, "[错误] %s 流不支持 %dx%d@%d, 可用配置:\n", def.label, w, h, fps);
    int lastW = -1, lastH = -1, lastF = -1;
    for(auto &v : all) {
        if(v->width() == lastW && v->height() == lastH && v->fps() == lastF) continue;
        lastW = v->width(); lastH = v->height(); lastF = v->fps();
        fprintf(stderr, "  %ux%u @%u (格式: %s 等)\n", v->width(), v->height(), v->fps(),
                formatName(v->format()));
    }
    return nullptr;
}

static void listProfiles(ob::Pipeline &pipe) {
    for(int s = 0; s < NSTREAMS; s++) {
        printf("== %s ==\n", STREAMS[s].label);
        std::vector<std::shared_ptr<ob::VideoStreamProfile>> all;
        if(!enumProfiles(pipe, STREAMS[s].type, all)) { printf("  (无)\n"); continue; }
        int lastW = -1, lastH = -1, lastF = -1;
        std::string fmts;
        for(auto &v : all) {
            if(v->width() == lastW && v->height() == lastH && v->fps() == lastF) {
                fmts += "/"; fmts += formatName(v->format());
                continue;
            }
            if(lastW >= 0) printf("  %dx%d @%d  (%s)\n", lastW, lastH, lastF, fmts.c_str());
            lastW = v->width(); lastH = v->height(); lastF = v->fps();
            fmts = formatName(v->format());
        }
        if(lastW >= 0) printf("  %dx%d @%d  (%s)\n", lastW, lastH, lastF, fmts.c_str());
    }
}

// 打印各路曝光/增益的当前值和可设范围
static void printProps(std::shared_ptr<ob::Device> dev) {
    for(int s = 0; s < NSTREAMS; s++) {
        auto &P = PROPS[s];
        printf("== %s ==\n", STREAMS[s].label);
        if(dev->isPropertySupported(P.autoExposure, OB_PERMISSION_READ)) {
            printf("  自动曝光: %s\n", dev->getBoolProperty(P.autoExposure) ? "开" : "关");
        }
        if(dev->isPropertySupported(P.exposure, OB_PERMISSION_READ)) {
            auto r   = dev->getIntPropertyRange(P.exposure);
            int  cur = dev->getIntProperty(P.exposure);
            printf("  曝光: 当前 %d | 范围 %d ~ %d (步进 %d, 默认 %d)\n",
                   cur, r.min, r.max, r.step, r.def);
        } else {
            printf("  曝光: 不支持\n");
        }
        if(dev->isPropertySupported(P.gain, OB_PERMISSION_READ)) {
            auto r   = dev->getIntPropertyRange(P.gain);
            int  cur = dev->getIntProperty(P.gain);
            printf("  增益: 当前 %d | 范围 %d ~ %d (步进 %d, 默认 %d)\n",
                   cur, r.min, r.max, r.step, r.def);
        } else {
            printf("  增益: 不支持\n");
        }
    }
}

// 应用曝光/增益设置 (需在数据流启动后调用)
static void applyProps(std::shared_ptr<ob::Device> dev, const Spec &spec, const StreamDef &def) {
    auto &P = PROPS[&def - STREAMS];
    if(spec.exposure >= 0) {
        if(dev->isPropertySupported(P.autoExposure, OB_PERMISSION_WRITE))
            dev->setBoolProperty(P.autoExposure, false);  // 先关自动曝光
        if(dev->isPropertySupported(P.exposure, OB_PERMISSION_WRITE)) {
            dev->setIntProperty(P.exposure, spec.exposure);
            printf("[曝光] %s: 设为 %d (回读 %d)\n", def.label, spec.exposure,
                   dev->getIntProperty(P.exposure));
        } else {
            printf("[曝光] %s: 该传感器不支持手动曝光\n", def.label);
        }
    }
    if(spec.gain >= 0) {
        if(dev->isPropertySupported(P.gain, OB_PERMISSION_WRITE)) {
            dev->setIntProperty(P.gain, spec.gain);
            printf("[增益] %s: 设为 %d (回读 %d)\n", def.label, spec.gain,
                   dev->getIntProperty(P.gain));
        } else {
            printf("[增益] %s: 该传感器不支持增益调节\n", def.label);
        }
    }
}

static bool parseWHF(const char *s, int &w, int &h, int &fps) {
    return sscanf(s, "%dx%dx%d", &w, &h, &fps) == 3 && w > 0 && h > 0 && fps > 0;
}

static void usage() {
    printf(
        "用法: ./orbbec_recorder [输出.bag] [时长秒] [选项]\n"
        "  --streams color,depth,ir   选择录制的流 (默认 color)\n"
        "  --color/--depth/--ir WxHxFPS   各路流的分辨率和帧率 (如 1920x1080x30)\n"
        "  --color-exposure/--depth-exposure/--ir-exposure N   曝光 (自动关自动曝光)\n"
        "  --color-gain/--depth-gain/--ir-gain N               增益\n"
        "  --out-dir DIR             输出 bag 目录 (默认当前目录, 自动创建)\n"
        "  --log-dir DIR             SDK 日志目录 (默认 ./Log, 自动创建)\n"
        "  --config FILE             SDK 配置文件路径 (默认 ./OrbbecSDKConfig_v1.0.xml)\n"
        "  --props   查询各路曝光/增益当前值和范围\n"
        "  --list    列出所有流配置\n"
        "示例: ./orbbec_recorder out.bag 60 --streams color,depth --depth-exposure 5000\n"
        "      ./orbbec_recorder out.bag 60 --out-dir ~/records --log-dir ~/logs\n");
}

int main(int argc, char **argv) {
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    Spec specs[NSTREAMS];
    specs[0].enabled = true; specs[0].w = 1920; specs[0].h = 1080; specs[0].fps = 30;
    specs[1].enabled = false; specs[1].w = 640; specs[1].h = 400; specs[1].fps = 30;
    specs[2].enabled = false; specs[2].w = 640; specs[2].h = 400; specs[2].fps = 30;

    std::string outFile;
    std::string outDir, logDir, configPath;
    int         seconds = 60;
    bool        outSet = false, secSet = false;
    bool        wantList = false, wantProps = false;

    for(int i = 1; i < argc; i++) {
        char *a = argv[i];

        // ---- 位置参数 (不以 -- 开头) ----
        if(a[0] != '-' || a[1] != '-') {
            if(!outSet) { outFile = a; outSet = true; }
            else if(!secSet) { seconds = atoi(a); secSet = true; }
            else { fprintf(stderr, "[错误] 多余的参数 '%s'\n", a); return 1; }
            continue;
        }

        // ---- 选项: 拆分 name 和 value (支持 --name=value 和 --name value) ----
        char *name = a + 2;
        char *eq   = strchr(name, '=');
        char *val  = nullptr;
        if(eq) { *eq = 0; val = eq + 1; }
        auto needVal = [&]() -> char * {
            if(val) return val;
            if(i + 1 < argc) return argv[++i];
            return nullptr;
        };

        // 无值选项
        if(!strcmp(name, "help") || !strcmp(name, "h")) { usage(); return 0; }
        if(!strcmp(name, "list")) { wantList = true; continue; }
        if(!strcmp(name, "props")) { wantProps = true; continue; }

        // 目录 / 配置文件选项 (可分别独立指定)
        if(!strcmp(name, "out-dir") || !strcmp(name, "log-dir") || !strcmp(name, "config")) {
            char *v = needVal();
            if(!v || !*v) {
                fprintf(stderr, "[错误] --%s 需要一个路径\n", name);
                return 1;
            }
            if(!strcmp(name, "out-dir")) outDir = v;
            else if(!strcmp(name, "log-dir")) logDir = v;
            else configPath = v;
            continue;
        }

        // --color/--depth/--ir WxHxFPS
        int s = -1;
        for(int k = 0; k < NSTREAMS; k++)
            if(!strcmp(name, STREAMS[k].name)) { s = k; break; }
        if(s >= 0) {
            char *v = needVal();
            if(!v || !parseWHF(v, specs[s].w, specs[s].h, specs[s].fps)) {
                fprintf(stderr, "[错误] %s 参数格式应为 宽x高x帧率, 如 %s 1920x1080x30\n",
                        STREAMS[s].label, name);
                return 1;
            }
            specs[s].enabled = true;
            continue;
        }

        // --<流>-exposure / --<流>-gain
        bool isGain = false;
        int  sk     = -1;
        for(int k = 0; k < NSTREAMS; k++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s-exposure", STREAMS[k].name);
            if(!strcmp(name, buf)) { sk = k; break; }
            snprintf(buf, sizeof(buf), "%s-gain", STREAMS[k].name);
            if(!strcmp(name, buf)) { sk = k; isGain = true; break; }
        }
        if(sk >= 0) {
            char *v = needVal();
            if(!v || atoi(v) < 0) {
                fprintf(stderr, "[错误] %s 需要一个非负整数 (可用 --props 查看范围)\n", name);
                return 1;
            }
            if(isGain) specs[sk].gain = atoi(v);
            else specs[sk].exposure = atoi(v);
            specs[sk].enabled = true;  // 设置了曝光/增益即启用该流
            continue;
        }

        // --streams color,depth,ir
        if(!strcmp(name, "streams")) {
            char *v = needVal();
            if(!v) { fprintf(stderr, "[错误] --streams 需要参数, 如 --streams color,depth\n"); return 1; }
            for(int k = 0; k < NSTREAMS; k++) specs[k].enabled = false;
            std::string list(v);
            size_t pos = 0;
            while(pos < list.size()) {
                std::string tok = list.substr(pos, list.find(',', pos) - pos);
                pos += tok.size() + 1;
                bool found = false;
                for(int k = 0; k < NSTREAMS; k++)
                    if(tok == STREAMS[k].name) { specs[k].enabled = true; found = true; }
                if(!found) {
                    fprintf(stderr, "[错误] 未知流类型 '%s', 可选: color, depth, ir\n", tok.c_str());
                    return 1;
                }
            }
            continue;
        }

        fprintf(stderr, "[错误] 未知选项 '--%s'\n", name);
        usage();
        return 1;
    }

    // 检查: 给未启用的流设置了曝光/增益
    for(int s = 0; s < NSTREAMS; s++) {
        if((specs[s].exposure >= 0 || specs[s].gain >= 0) && !specs[s].enabled) {
            fprintf(stderr, "[错误] 给 %s 流设置了曝光/增益, 但该流未启用; "
                    "请用 --streams 启用它, 或去掉对应参数\n", STREAMS[s].label);
            return 1;
        }
    }

    // 目录参数预处理
    outDir     = expandHome(outDir);
    logDir     = expandHome(logDir);
    configPath = expandHome(configPath);

    if(!logDir.empty() && !makeDirs(logDir)) {
        fprintf(stderr, "[错误] 无法创建日志目录 '%s'\n", logDir.c_str());
        return 1;
    }
    if(!configPath.empty()) {
        struct stat st;
        if(stat(configPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            fprintf(stderr, "[错误] SDK 配置文件不存在: '%s'\n", configPath.c_str());
            return 1;
        }
    }

    try {
        // --list / --props 也要能使用自定义配置和日志目录
        if(wantList || wantProps) {
            SdkSession sdk = openSdk(configPath, logDir);
            if(wantList) listProfiles(*sdk.pipe);
            else printProps(sdk.pipe->getDevice());
            return 0;
        }

        if(!outDir.empty() && !makeDirs(outDir)) {
            fprintf(stderr, "[错误] 无法创建输出目录 '%s'\n", outDir.c_str());
            return 1;
        }
        if(!outSet) outFile = timestampName();
        if(!outDir.empty()) outFile = joinPath(outDir, outFile);
        if(seconds <= 0) { fprintf(stderr, "[错误] 时长必须为正数\n"); return 1; }

        printf("[初始化] 打开相机...\n");
        SdkSession sdk = openSdk(configPath, logDir);
        ob::Pipeline &pipe = *sdk.pipe;

        std::shared_ptr<ob::VideoStreamProfile> profiles[NSTREAMS];
        for(int s = 0; s < NSTREAMS; s++) {
            if(!specs[s].enabled) continue;
            profiles[s] = pickProfile(pipe, STREAMS[s], specs[s].w, specs[s].h, specs[s].fps);
            if(!profiles[s]) return 1;
            printf("[配置] %s: %ux%u @%u %s\n", STREAMS[s].label, profiles[s]->width(),
                   profiles[s]->height(), profiles[s]->fps(), formatName(profiles[s]->format()));
        }

        auto config = std::make_shared<ob::Config>();
        for(int s = 0; s < NSTREAMS; s++)
            if(specs[s].enabled) config->enableStream(profiles[s]);

        printf("[启动] 启动数据流...\n");
        pipe.start(config);

        // 应用曝光/增益 (需在流启动后)
        for(int s = 0; s < NSTREAMS; s++)
            if(specs[s].enabled && (specs[s].exposure >= 0 || specs[s].gain >= 0))
                applyProps(pipe.getDevice(), specs[s], STREAMS[s]);

        printf("[录制] 写入 %s (时长 %d 秒, Ctrl+C 停止)\n", outFile.c_str(), seconds);
        pipe.startRecord(outFile.c_str());

        auto     t0   = std::chrono::steady_clock::now();
        auto     tRep = t0;
        uint64_t cnt[NSTREAMS] = {0, 0, 0};
        while(g_running) {
            auto fs = pipe.waitForFrames(100);
            if(fs) {
                for(int s = 0; s < NSTREAMS; s++) {
                    if(!specs[s].enabled) continue;
                    std::shared_ptr<ob::Frame> f;
                    if(s == 0) f = fs->colorFrame();
                    else if(s == 1) f = fs->depthFrame();
                    else f = fs->irFrame();
                    if(f) cnt[s]++;
                }
                auto now = std::chrono::steady_clock::now();
                if(std::chrono::duration<double>(now - tRep).count() >= 1.0) {
                    double el = std::chrono::duration<double>(now - t0).count();
                    printf("\r[统计] %.1fs / %ds |", el, seconds);
                    for(int s = 0; s < NSTREAMS; s++)
                        if(specs[s].enabled)
                            printf(" %s %llu (%.1ffps) |", STREAMS[s].label,
                                   (unsigned long long)cnt[s], cnt[s] / el);
                    fflush(stdout);
                    tRep = now;
                }
                if(std::chrono::duration<double>(now - t0).count() >= seconds) break;
            }
        }

        printf("\n[停止] 结束录制...\n");
        pipe.stopRecord();
        pipe.stop();
        double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("[完成] %s | 时长 %.1fs |", outFile.c_str(), el);
        for(int s = 0; s < NSTREAMS; s++)
            if(specs[s].enabled) printf(" %s %llu 帧 |", STREAMS[s].label, (unsigned long long)cnt[s]);
        printf("\n[提示] 回放: OrbbecViewer -> File -> Open\n");
    } catch(ob::Error &e) {
        fprintf(stderr, "\n[错误] %s\n", e.getMessage());
        return 1;
    } catch(std::exception &e) {
        fprintf(stderr, "\n[错误] %s\n", e.what());
        return 1;
    }
    return 0;
}
