#!/usr/bin/env python3
# Copyright (c) 2026 加一公司. All rights reserved.
# 文件: run_idf.py  作者: 屈海鹏  时间: 2026-11-11
# 描述: Git Bash/MSYS2 下的 idf.py 包装脚本。
#
# Git Bash / MSYS2 会向每个子进程强制注入 MSYSTEM 环境变量，idf.py (v6.1)
# 检测到后会打印 "MSys/Mingw is no longer supported" 并直接跳过 main()。
# 此脚本在调用 idf.py 前移除该变量，从而正常构建。
#
# 用法：
#   export IDF_PATH=<你的 ESP-IDF 路径>
#   python run_idf.py build
#   python run_idf.py -p COM3 flash

import os
import sys

for key in ('MSYSTEM', 'MSYSTEM_CARCH', 'MSYSTEM_PREFIX', 'MSYSTEM_CHOST'):
    os.environ.pop(key, None)

idf_path = os.environ.get('IDF_PATH')
if not idf_path:
    sys.exit('错误：未设置 IDF_PATH 环境变量（例如 export IDF_PATH=D:/esp/.espressif/v6.1/esp-idf）')

IDF_PY = os.path.join(idf_path, 'tools', 'idf.py')
if not os.path.isfile(IDF_PY):
    sys.exit(f'错误：找不到 {IDF_PY}')

# idf.py 会从 tools/ 目录导入同级的模块（python_version_checker 等），
# 因此把它放到 sys.path 首位，模拟直接运行脚本的环境。
sys.path.insert(0, os.path.dirname(IDF_PY))

# argv[0] 需形如 idf.py 供 click 解析，argv[1:] 为子命令及参数。
sys.argv[0] = IDF_PY

with open(IDF_PY, encoding='utf-8') as f:
    code = compile(f.read(), IDF_PY, 'exec')

# 执行 idf.py 的 __main__ 分支。
exec(code, {'__name__': '__main__', '__file__': IDF_PY})
