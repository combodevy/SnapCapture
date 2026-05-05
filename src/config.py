"""
配置管理模块 - 读写应用配置（JSON格式）
"""
import json
import os
from pathlib import Path
from PyQt6.QtCore import QObject, pyqtSignal


class Config(QObject):
    """应用配置管理器，配置存储于 %APPDATA%/SnapCapture/config.json"""

    config_changed = pyqtSignal(str, object)  # (key, new_value)

    DEFAULT_HOTKEY = "ctrl+shift+a"
    DEFAULT_SAVE_PATH = str(Path.home() / "Pictures" / "SnapCapture")

    DEFAULTS = {
        "hotkey": DEFAULT_HOTKEY,
        "save_path": DEFAULT_SAVE_PATH,
        "auto_start": False,
    }

    def __init__(self):
        super().__init__()
        appdata = os.environ.get("APPDATA", str(Path.home()))
        self.config_dir = Path(appdata) / "SnapCapture"
        self.config_file = self.config_dir / "config.json"
        self._data = dict(self.DEFAULTS)
        self._load()

    def _load(self):
        """从文件加载配置"""
        try:
            if self.config_file.exists():
                with open(self.config_file, "r", encoding="utf-8") as f:
                    loaded = json.load(f)
                # 只更新已知的配置项
                for key in self.DEFAULTS:
                    if key in loaded:
                        self._data[key] = loaded[key]
        except (json.JSONDecodeError, IOError, OSError):
            pass

    def save(self):
        """保存配置到文件"""
        try:
            self.config_dir.mkdir(parents=True, exist_ok=True)
            with open(self.config_file, "w", encoding="utf-8") as f:
                json.dump(self._data, f, indent=2, ensure_ascii=False)
        except (IOError, OSError) as e:
            print(f"[SnapCapture] 配置保存失败: {e}")

    def get(self, key, default=None):
        """获取配置值"""
        fallback = default if default is not None else self.DEFAULTS.get(key)
        return self._data.get(key, fallback)

    def set(self, key, value):
        """设置配置值并自动保存"""
        old_value = self._data.get(key)
        self._data[key] = value
        self.save()
        if old_value != value:
            self.config_changed.emit(key, value)

    def reset_hotkey(self):
        """恢复默认快捷键"""
        self.set("hotkey", self.DEFAULT_HOTKEY)

    @property
    def hotkey(self):
        return self.get("hotkey")

    @property
    def save_path(self):
        return self.get("save_path")

    @property
    def auto_start(self):
        return self.get("auto_start")
