#!/usr/bin/env python3
"""POST 一张 JPEG 到 ESP32 的 /detect 接口，获取人脸检测结果"""

import sys
import requests

HOST = "192.168.110.47"  # 改成你的设备 IP
IMAGE_FILE = "test.jpg"   # 本地 JPEG 图片路径


def main():
    # 1. 读取本地 JPEG 文件
    with open(IMAGE_FILE, "rb") as f:
        jpeg_data = f.read()
    print(f"读取文件: {IMAGE_FILE} ({len(jpeg_data)} bytes)")

    # 2. POST 到 /detect
    url = f"http://{HOST}/detect"
    resp = requests.post(url, data=jpeg_data, headers={"Content-Type": "image/jpeg"}, timeout=30)
    resp.raise_for_status()

    # 3. 解析 JSON 返回
    result = resp.json()
    count = result.get("count", 0)
    print(f"检测到 {count} 个人脸")

    for i, face in enumerate(result.get("faces", [])):
        print(f"  [{i}] x={face['x']} y={face['y']} w={face['w']} h={face['h']} score={face['score']}")

    print("\n返回的完整 JSON:")
    print(result)


if __name__ == "__main__":
    main()