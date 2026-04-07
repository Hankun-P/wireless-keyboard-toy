import os
import sys

def resource_path(path):
    """
    获取资源文件的绝对路径
    支持开发环境和 PyInstaller 打包后的环境
    """
    if hasattr(sys, "_MEIPASS"):
        # PyInstaller 打包后的临时目录
        base = sys._MEIPASS
        return os.path.join(base, path)
    else:
        # 开发环境：从 utils 目录向上到 controller 目录
        base = os.path.dirname(os.path.dirname(__file__))
        return os.path.join(base, path)