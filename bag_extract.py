#!/usr/bin/env python3
"""Orbbec 录制文件 (.bag, 标准 ROSBAG V2.0) 解析工具

用法:
  # 转成 mp4 视频
  ./bin/python bag_extract.py sample_rgbd.bag -o out.mp4

  # 每秒抽 N 帧存 JPEG (即"每秒拍N张照片")
  ./bin/python bag_extract.py sample_rgbd.bag --frames 30 -o frames/

  # 指定 topic (默认自动找彩色流 /cam0/color_raw)
  ./bin/python bag_extract.py x.bag --topic /cam0/depth_raw -o depth.mp4

依赖: 本目录 venv (rosbags + opencv + numpy)
"""

import argparse
import os
import struct
import sys

import cv2
import numpy as np
from rosbags.rosbag1 import Reader


def parse_compressed_image(raw):
    """解析 sensor_msgs/msg/CompressedImage 的 CDR 序列化, 返回 (时间戳秒, 格式, JPEG字节)"""
    # 注: Orbbec 录制的封装头是 00 00 00 00 (非标准 CDR 的 00 01 00 00), 跳过前 4 字节即可
    off = 4
    sec, nsec = struct.unpack_from("<iI", raw, off)
    off += 8
    (flen,) = struct.unpack_from("<I", raw, off)
    off += 4 + flen  # frame_id
    (flen,) = struct.unpack_from("<I", raw, off)
    off += 4
    fmt = raw[off : off + flen].decode(errors="replace")
    off += flen
    (dlen,) = struct.unpack_from("<I", raw, off)
    off += 4
    return sec + nsec / 1e9, fmt, raw[off : off + dlen]


def main():
    ap = argparse.ArgumentParser(description="Orbbec bag -> mp4 / JPEG 抽取")
    ap.add_argument("bag", help=".bag 文件路径")
    ap.add_argument("-o", "--out", required=True, help="输出 mp4 文件或帧目录")
    ap.add_argument("--topic", help="topic 名, 默认自动选择彩色流")
    ap.add_argument("--fps", type=float, default=0, help="输出帧率, 默认按录制实测")
    ap.add_argument("--frames", type=int, default=0, help="抽帧模式: 每秒保存 N 张 JPEG")
    args = ap.parse_args()

    with Reader(args.bag) as r:
        conns = list(r.connections)
        topic = args.topic
        if not topic:
            for c in conns:  # 自动找彩色流
                if "color" in c.topic:
                    topic = c.topic
                    break
        if not topic:
            print("[错误] 没找到彩色流 topic, 可用:", [c.topic for c in conns])
            sys.exit(1)
        print(f"[解析] topic: {topic}")

        stamps, jpegs = [], []
        for conn, ts, raw in r.messages():
            if conn.topic != topic:
                continue
            t, fmt, data = parse_compressed_image(raw)
            stamps.append(t)
            jpegs.append(data)

    if not jpegs:
        print("[错误] topic 中没有数据")
        sys.exit(1)

    span = stamps[-1] - stamps[0]
    real_fps = len(jpegs) / span if span > 0 else 30.0
    fps = args.fps or round(real_fps)
    print(f"[解析] {len(jpegs)} 帧, 时间跨度 {span:.2f}s, 实测 {real_fps:.1f} fps, 输出按 {fps} fps")

    # 先解一帧拿分辨率
    first = cv2.imdecode(np.frombuffer(jpegs[0], np.uint8), cv2.IMREAD_COLOR)
    if first is None:
        print("[错误] 首帧解码失败")
        sys.exit(1)
    h, w = first.shape[:2]
    print(f"[解析] 分辨率 {w}x{h}")

    if args.frames > 0:
        os.makedirs(args.out, exist_ok=True)
        interval = 1.0 / args.frames
        n = 0
        next_t = stamps[0]
        for t, data in zip(stamps, jpegs):
            if t >= next_t:
                frame = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
                fname = os.path.join(args.out, f"f_{n:06d}_{t:.3f}.jpg")
                cv2.imwrite(fname, frame, [cv2.IMWRITE_JPEG_QUALITY, 95])
                n += 1
                next_t += interval
        print(f"[完成] 已保存 {n} 张 JPEG -> {os.path.abspath(args.out)}")
    else:
        writer = cv2.VideoWriter(args.out, cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
        if not writer.isOpened():
            print("[错误] 无法创建视频文件")
            sys.exit(1)
        for data in jpegs:
            frame = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
            if frame is not None:
                writer.write(frame)
        writer.release()
        print(f"[完成] 视频已保存 -> {os.path.abspath(args.out)}")


if __name__ == "__main__":
    main()
