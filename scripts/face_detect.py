#!/usr/bin/env python3
"""
ESP32-S3 人脸检测客户端

调用设备上的 POST /detect 接口：
  - 请求体：raw JPEG bytes
  - 响应：{"count": N, "faces": [{"x","y","w","h","score"}], "image": "<base64 JPEG>"}

用法示例：
  # 单次抓取一帧 + 检测 + 显示
  python face_detect.py 192.168.110.47

  # 上传本地图片
  python face_detect.py 192.168.110.47 --image face.jpg

  # 保存检测结果到文件
  python face_detect.py 192.168.110.47 --save out.jpg

  # 循环检测，按 q 退出
  python face_detect.py 192.168.110.47 --loop

依赖：
  pip install requests opencv-python numpy
"""

import argparse
import base64
import os
import sys
import time

import cv2
import numpy as np
import requests


def grab_jpeg_from_stream(host: str, timeout: float = 5.0) -> bytes:
    """从 ESP32 的 /stream MJPEG 中抠出第一帧完整 JPEG。"""
    url = f"http://{host}/stream"
    resp = requests.get(url, stream=True, timeout=timeout)
    resp.raise_for_status()

    buf = bytearray()
    try:
        for chunk in resp.iter_content(chunk_size=4096):
            buf.extend(chunk)
            start = buf.find(b"\xff\xd8")  # JPEG SOI
            if start < 0:
                continue
            end = buf.find(b"\xff\xd9", start + 2)  # JPEG EOI
            if end < 0:
                continue
            return bytes(buf[start : end + 2])
    finally:
        resp.close()
    raise RuntimeError("Stream closed before a full JPEG frame was received")


def post_detect(host: str, jpeg: bytes, timeout: float = 30.0) -> dict:
    """POST 一张 JPEG 到 /detect，返回解析后的 JSON dict。"""
    url = f"http://{host}/detect"
    resp = requests.post(
        url,
        data=jpeg,
        headers={"Content-Type": "image/jpeg"},
        timeout=timeout,
    )
    resp.raise_for_status()
    return resp.json()


def decode_b64_jpeg(b64_str: str):
    """base64 字符串 -> OpenCV BGR ndarray，失败返回 None。"""
    try:
        raw = base64.b64decode(b64_str)
    except Exception:
        return None
    arr = np.frombuffer(raw, dtype=np.uint8)
    return cv2.imdecode(arr, cv2.IMREAD_COLOR)


def print_faces(result: dict, elapsed_ms: float) -> None:
    count = result.get("count", 0)
    print(f"Detected {count} face(s) in {elapsed_ms:.0f} ms")
    for i, f in enumerate(result.get("faces", [])):
        print(
            f"  [{i}] x={f['x']:>4} y={f['y']:>4} "
            f"w={f['w']:>4} h={f['h']:>4} score={f['score']:.2f}"
        )


def detect_once(host: str, image_path: str = None, save: str = None, show: bool = True) -> None:
    if image_path:
        with open(image_path, "rb") as fp:
            jpeg = fp.read()
        print(f"Loaded {len(jpeg)} bytes from {image_path}", file=sys.stderr)
    else:
        print(f"Grabbing one frame from http://{host}/stream ...", file=sys.stderr)
        jpeg = grab_jpeg_from_stream(host)
        print(f"Got {len(jpeg)} bytes", file=sys.stderr)

    t0 = time.time()
    result = post_detect(host, jpeg)
    elapsed = (time.time() - t0) * 1000.0
    print_faces(result, elapsed)

    img = decode_b64_jpeg(result.get("image", ""))
    if img is None:
        print("Could not decode the annotated image from response", file=sys.stderr)
        return

    if save:
        cv2.imwrite(save, img)
        print(f"Annotated image saved -> {save}")

    if show:
        cv2.imshow("face detect (press any key to close)", img)
        cv2.waitKey(0)
        cv2.destroyAllWindows()


def detect_loop(host: str, interval: float, save_dir: str = None) -> None:
    if save_dir:
        os.makedirs(save_dir, exist_ok=True)

    frame_idx = 0
    win_name = "face detect (loop, press q to quit)"

    while True:
        try:
            t0 = time.time()
            jpeg = grab_jpeg_from_stream(host)
            result = post_detect(host, jpeg)
            elapsed = (time.time() - t0) * 1000.0
        except Exception as exc:
            print(f"frame {frame_idx}: {exc}", file=sys.stderr)
            if cv2.waitKey(500) & 0xFF == ord("q"):
                break
            continue

        img = decode_b64_jpeg(result.get("image", ""))
        if img is not None:
            count = result.get("count", 0)
            cv2.putText(
                img,
                f"faces={count}  {elapsed:.0f}ms",
                (10, 22),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                (0, 255, 255),
                2,
            )
            cv2.imshow(win_name, img)
            if save_dir:
                cv2.imwrite(os.path.join(save_dir, f"frame_{frame_idx:05d}.jpg"), img)

        frame_idx += 1
        wait_ms = max(1, int(interval * 1000))
        if cv2.waitKey(wait_ms) & 0xFF == ord("q"):
            break

    cv2.destroyAllWindows()


def main() -> None:
    ap = argparse.ArgumentParser(description="ESP32-S3 face detect client")
    ap.add_argument("host", help="设备 IP，例如 192.168.110.47")
    ap.add_argument("--image", help="上传本地 JPEG，而不是从 /stream 抓帧")
    ap.add_argument("--save", help="把检测结果图保存到这个文件")
    ap.add_argument("--no-show", action="store_true", help="不弹窗显示（适合服务器/CI）")
    ap.add_argument("--loop", action="store_true", help="循环抓帧 + 检测，按 q 退出")
    ap.add_argument("--interval", type=float, default=0.1, help="循环模式的帧间隔秒数（默认 0.1）")
    ap.add_argument("--save-dir", help="循环模式下，把每一帧带框图保存到这个目录")
    args = ap.parse_args()

    if args.loop:
        detect_loop(args.host, args.interval, args.save_dir)
    else:
        detect_once(args.host, args.image, args.save, show=not args.no_show)


if __name__ == "__main__":
    try:
        main()
    except requests.exceptions.RequestException as exc:
        print(f"HTTP error: {exc}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        sys.exit(130)
