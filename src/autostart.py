"""
开机自启动管理模块 - 通过 Windows 注册表控制自启动
"""
import sys
import winreg

APP_NAME = "SnapCapture"
REG_PATH = r"Software\Microsoft\Windows\CurrentVersion\Run"


def is_autostart_enabled() -> bool:
    """检查是否已设置开机自启动"""
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_READ
        )
        try:
            winreg.QueryValueEx(key, APP_NAME)
            return True
        except FileNotFoundError:
            return False
        finally:
            winreg.CloseKey(key)
    except OSError:
        return False


def set_autostart(enable: bool) -> bool:
    """设置或取消开机自启动，返回操作是否成功"""
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_SET_VALUE
        )
        try:
            if enable:
                if getattr(sys, "frozen", False):
                    exe_path = f'"{sys.executable}"'
                else:
                    exe_path = f'"{sys.executable}" "{sys.argv[0]}"'
                winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, exe_path)
            else:
                try:
                    winreg.DeleteValue(key, APP_NAME)
                except FileNotFoundError:
                    pass
            return True
        finally:
            winreg.CloseKey(key)
    except OSError as e:
        print(f"[SnapCapture] 自启动设置失败: {e}")
        return False
