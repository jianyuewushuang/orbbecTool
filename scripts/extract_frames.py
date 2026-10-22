#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把当前目录下的所有视频按「每 N 帧抽 1 帧」抽成图片，
输出到与视频同名的文件夹中（如 demo.mp4 -> ./demo/000001.jpg）。

用法：
    python extract_frames.py                  # 默认每 15 帧抽 1 帧
    python extract_frames.py -n 30            # 每 30 帧抽 1 帧
    python extract_frames.py -d ./videos      # 指定视频所在目录
    python extract_frames.py -f png           # 输出 png
    python extract_frames.py -r               # 递归处理子目录

后端：优先使用 ffmpeg（快、兼容性好），不可用时自动回退到 OpenCV。
"""

import argparse
import os
import shutil
import subprocess
import sys

VIDEO_EXTS = {
    ".mp4", ".mov", ".mkv", ".avi", ".flv", ".wmv", ".webm",
    ".m4v", ".mpg", ".mpeg", ".3gp", ".ts", ".mts", ".m2ts", ".rmvb",
}


def find_videos(directory, recursive):
    """找出目录下所有视频文件（跳过已输出文件夹里的产物）。"""
    videos = []
    if recursive:
        for root, dirs, files in os.walk(directory):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for name in files:
                if os.path.splitext(name)[1].lower() in VIDEO_EXTS:
                    videos.append(os.path.join(root, name))
    else:
        for name in sorted(os.listdir(directory)):
            path = os.path.join(directory, name)
            if os.path.isfile(path) and os.path.splitext(name)[1].lower() in VIDEO_EXTS:
                videos.append(path)
    return videos


def extract_with_ffmpeg(video, out_dir, interval, img_fmt):
    """用 ffmpeg 的 select 滤镜精确按帧序号抽帧：第 0、interval、2*interval ... 帧。"""
    pattern = os.path.join(out_dir, f"%06d.{img_fmt}")
    cmd = [
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin",
        "-i", video,
        "-vf", rf"select='not(mod(n\,{interval}))'",
        "-vsync", "0",           # 禁止按时间戳补帧/丢帧，保证一帧不差
        "-frame_pts", "0",       # 文件名按输出顺序编号，而不是原始帧号
        "-q:v", "2",             # jpg 质量（2 ≈ 高质量）；png 时忽略
        pattern,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip().splitlines()[-1] if proc.stderr else "ffmpeg 失败")
    return len([f for f in os.listdir(out_dir) if f.lower().endswith(f".{img_fmt}")])


def extract_with_cv2(video, out_dir, interval, img_fmt):
    """OpenCV 回退方案：逐帧读取，按计数抽帧。"""
    import cv2

    cap = cv2.VideoCapture(video)
    if not cap.isOpened():
        raise RuntimeError("无法打开视频")

    count = 0
    saved = 0
    ok, frame = cap.read()
    while ok:
        if count % interval == 0:
            saved += 1
            cv2.imwrite(os.path.join(out_dir, f"{saved:06d}.{img_fmt}"), frame)
        count += 1
        ok, frame = cap.read()
    cap.release()
    return saved


def main():
    ap = argparse.ArgumentParser(description="视频按固定间隔抽帧为图片")
    ap.add_argument("-d", "--dir", default=".", help="视频所在目录，默认当前目录")
    ap.add_argument("-n", "--interval", type=int, default=15,
                    help="每隔多少帧抽 1 帧，默认 15")
    ap.add_argument("-f", "--format", default="jpg", help="图片格式，默认 jpg（也支持 png）")
    ap.add_argument("-r", "--recursive", action="store_true", help="递归处理子目录")
    ap.add_argument("--backend", choices=["auto", "ffmpeg", "cv2"], default="auto",
                    help="抽帧后端，默认自动选择")
    args = ap.parse_args()

    if args.interval < 1:
        sys.exit("错误：抽帧间隔必须 >= 1")

    directory = os.path.abspath(args.dir)
    if not os.path.isdir(directory):
        sys.exit(f"错误：目录不存在 -> {directory}")

    img_fmt = args.format.lower().lstrip(".")
    videos = find_videos(directory, args.recursive)
    if not videos:
        print(f"未在 {directory} 找到视频文件（支持后缀：{', '.join(sorted(VIDEO_EXTS))}）")
        return

    use_ffmpeg = (args.backend == "ffmpeg") or (
        args.backend == "auto" and shutil.which("ffmpeg") is not None)

    print(f"共发现 {len(videos)} 个视频，每 {args.interval} 帧抽 1 帧，"
          f"后端 = {'ffmpeg' if use_ffmpeg else 'OpenCV'}\n")

    total = 0
    failed = []
    for idx, video in enumerate(videos, 1):
        name = os.path.splitext(os.path.basename(video))[0]
        out_dir = os.path.join(os.path.dirname(video), name)
        os.makedirs(out_dir, exist_ok=True)

        print(f"[{idx}/{len(videos)}] {os.path.basename(video)} -> {name}/", end=" ", flush=True)
        try:
            if use_ffmpeg:
                saved = extract_with_ffmpeg(video, out_dir, args.interval, img_fmt)
            else:
                saved = extract_with_cv2(video, out_dir, args.interval, img_fmt)
            total += saved
            print(f"抽出 {saved} 张")
        except Exception as e:
            failed.append((os.path.basename(video), str(e)))
            print(f"失败：{e}")

    print(f"\n完成：共抽出 {total} 张图片"
          + (f"，{len(failed)} 个视频失败" if failed else ""))
    for n, err in failed:
        print(f"  - {n}: {err}")


if __name__ == "__main__":
    main()
