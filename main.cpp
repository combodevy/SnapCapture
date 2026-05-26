#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <dwmapi.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;
using namespace std;

// ========== 全局常量与标识符 ==========
#define WM_TRAY_MSG (WM_USER + 100)
#define WM_TRIGGER_CAPTURE (WM_USER + 101)
#define TRAY_ICON_ID 1

// 托盘右键菜单 ID
#define ID_TRAY_CAPTURE 2001
#define ID_TRAY_SETTINGS 2002
#define ID_TRAY_AUTOSTART 2003
#define ID_TRAY_QUIT 2004

// 选区控制点判定半径大小
const int HANDLE_HALF_WIDTH = 6;
const int MIN_SELECTION_SIZE = 5;

// ========== 全局变量 ==========
HINSTANCE g_hInstance = NULL;
HWND g_hWndMain = NULL;
HWND g_hWndOverlay = NULL;
HWND g_hWndSettings = NULL;

HHOOK g_hKeyHook = NULL;
HHOOK g_hMouseHook = NULL;

ULONG_PTR g_gdiplusToken = 0;
HBITMAP g_hScreenCapture = NULL;
HBITMAP g_hScreenCaptureMasked = NULL;

// 超高性能渲染缓存全局 GDI 句柄
HDC g_hOverlayDoubleBufferDC = NULL;
HBITMAP g_hOverlayDoubleBufferBmp = NULL;
HGDIOBJ g_hOldOverlayDoubleBufferBmp = NULL;
HDC g_hCaptureSourceDC = NULL;
HGDIOBJ g_hOldCaptureSourceBmp = NULL;
HDC g_hCaptureMaskedSourceDC = NULL;
HGDIOBJ g_hOldCaptureMaskedSourceBmp = NULL;

int g_screenX = 0;
int g_screenY = 0;
int g_screenWidth = 0;
int g_screenHeight = 0;

// ========== 配置信息数据结构 ==========
struct HotkeyConfig {
    wstring type = L"keyboard"; // keyboard or mouse
    vector<wstring> keys = { L"ctrl", L"shift", L"a" };
    wstring mouse_button = L""; // x1 or x2
    bool suppress = true;
};

struct AppConfig {
    HotkeyConfig hotkey;
    bool auto_start = false;
    bool save_to_clipboard = true;
    wstring save_directory = L"";
    bool notification = true;
    wstring capture_mode = L"region"; // region or window
};

AppConfig g_config;
wstring g_configPath = L"";

#define WM_HOTKEY_RECORDED (WM_USER + 102)
bool g_isRecordingHotkey = false;
HotkeyConfig g_recordedHotkey;

enum AnnotationMode {
    ANNOTATION_NONE = 0,
    ANNOTATION_ARROW,
    ANNOTATION_RECTANGLE,
    ANNOTATION_TEXT
};

enum ShapeType {
    SHAPE_ARROW = 0,
    SHAPE_RECTANGLE,
    SHAPE_TEXT
};

struct DrawingShape {
    ShapeType type;
    POINT start;
    POINT end;
    Color color;
    int thickness;
    wstring text;
};

AnnotationMode g_annotationMode = ANNOTATION_NONE;
vector<DrawingShape> g_shapes;
bool g_isDrawingShape = false;
DrawingShape g_tempShape;

// 当前属性选择状态
Color g_currentColor = Color(255, 231, 76, 60); // 默认红色
int g_currentThickness = 4; // 默认中号 4px

// 文字标注状态
bool g_isEditingText = false;
wstring g_editingText = L"";
POINT g_editTextPos = { 0, 0 };
int g_draggingTextIndex = -1;
POINT g_textDragOffset = { 0, 0 };
bool g_isDraggingText = false;

int ThicknessToFontSize(int thickness) {
    if (thickness <= 2) return 14;
    if (thickness <= 4) return 20;
    return 32;
}

// ========== 拖动调整大小手柄与交互状态 ==========
enum ResizeHandle {
    NONE_HANDLE = 0,
    TL, T, TR, R, BR, B, BL, L, M
};

struct OverlayState {
    RECT selection = { 0, 0, 0, 0 };
    bool selectionDone = false;
    bool isSelecting = false;
    bool isResizing = false;
    ResizeHandle activeHandle = NONE_HANDLE;
    POINT startPos = { 0, 0 };
    POINT dragStartPos = { 0, 0 };
    RECT dragStartRect = { 0, 0, 0, 0 };
    
    // 悬停高亮按钮索引 (1:✓ 确认, 2:💾 保存, 3:✗ 取消)
    int hoveredButton = 0; 
    
    // 智能窗口探测状态字段
    RECT detectedWindowRect = { 0, 0, 0, 0 };
    bool hasDetectedWindow = false;
    bool isPossibleClick = false;
};

OverlayState g_overlay;

// ========== 函数前置声明 ==========
void LoadConfig();
void SaveConfig();
void ApplyAutostart();
void StartHooks();
void StopHooks();
void TriggerCapture();
HICON CreateProgrammaticIcon();
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
bool CopyBitmapToClipboard(HBITMAP hBmp);
bool SaveBitmapToFile(HBITMAP hBmp, const wstring& filepath);
void UpdateTrayAutostartMenu(HMENU hMenu);

// ========== 兼容性 wstring 转换函数 ==========
wstring ToWString(int val) {
    wstringstream wss;
    wss << val;
    return wss.str();
}

// ========== DPI 感知初始化 ==========
void InitDpiAwareness() {
    HMODULE hShcore = LoadLibrary(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(int);
        PFN_SetProcessDpiAwareness pfn = (PFN_SetProcessDpiAwareness)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (pfn) {
            pfn(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(hShcore);
    } else {
        HMODULE hUser32 = GetModuleHandle(L"user32.dll");
        if (hUser32) {
            typedef BOOL(WINAPI* PFN_SetProcessDPIAware)();
            PFN_SetProcessDPIAware pfn = (PFN_SetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
            if (pfn) pfn();
        }
    }
}

// ========== 辅助函数: 轻量级配置文件解析 ==========
wstring Trim(const wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n\"");
    if (start == wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n\"");
    return s.substr(start, end - start + 1);
}

void LoadConfig() {
    // 默认配置文件路径
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath);
    g_configPath = wstring(exePath) + L"\\config.json";
    
    FILE* file = _wfopen(g_configPath.c_str(), L"rt, ccs=UTF-8");
    if (!file) {
        SaveConfig();
        return;
    }
    
    // 轻量级简易 JSON 解析（适配无外部 JSON 库环境）
    wchar_t buf[2048];
    while (fgetws(buf, 2048, file)) {
        wstring line(buf);
        size_t colon = line.find(L':');
        if (colon == wstring::npos) continue;
        
        wstring key = Trim(line.substr(0, colon));
        wstring val = line.substr(colon + 1);
        size_t comma = val.find_last_of(L',');
        if (comma != wstring::npos) {
            val = val.substr(0, comma);
        }
        val = Trim(val);
        
        if (key == L"type") g_config.hotkey.type = val;
        else if (key == L"mouse_button") g_config.hotkey.mouse_button = val;
        else if (key == L"suppress") g_config.hotkey.suppress = (val == L"true");
        else if (key == L"auto_start") g_config.auto_start = (val == L"true");
        else if (key == L"save_to_clipboard") g_config.save_to_clipboard = (val == L"true");
        else if (key == L"save_directory") g_config.save_directory = val;
        else if (key == L"notification") g_config.notification = (val == L"true");
        else if (key == L"capture_mode") g_config.capture_mode = val;
        else if (key == L"keys") {
            // 解析快捷键数组，形如 ["ctrl", "shift", "a"]
            g_config.hotkey.keys.clear();
            size_t start = val.find(L'[');
            size_t end = val.find(L']');
            if (start != wstring::npos && end != wstring::npos && end > start + 1) {
                wstring keys_str = val.substr(start + 1, end - start - 1);
                wstringstream ss(keys_str);
                wstring token;
                while (getline(ss, token, L',')) {
                    g_config.hotkey.keys.push_back(Trim(token));
                }
            }
        }
    }
    fclose(file);
}

void SaveConfig() {
    FILE* file = _wfopen(g_configPath.c_str(), L"wt, ccs=UTF-8");
    if (!file) return;
    
    fwprintf(file, L"{\n");
    fwprintf(file, L"  \"hotkey\": {\n");
    fwprintf(file, L"    \"type\": \"%ls\",\n", g_config.hotkey.type.c_str());
    fwprintf(file, L"    \"keys\": [");
    for (size_t i = 0; i < g_config.hotkey.keys.size(); ++i) {
        fwprintf(file, L"\"%ls\"", g_config.hotkey.keys[i].c_str());
        if (i < g_config.hotkey.keys.size() - 1) fwprintf(file, L", ");
    }
    fwprintf(file, L"],\n");
    fwprintf(file, L"    \"mouse_button\": \"%ls\",\n", g_config.hotkey.mouse_button.c_str());
    fwprintf(file, L"    \"suppress\": %ls\n", g_config.hotkey.suppress ? L"true" : L"false");
    fwprintf(file, L"  },\n");
    fwprintf(file, L"  \"auto_start\": %ls,\n", g_config.auto_start ? L"true" : L"false");
    fwprintf(file, L"  \"save_to_clipboard\": %ls,\n", g_config.save_to_clipboard ? L"true" : L"false");
    
    // 处理路径斜杠转义
    wstring escaped_path = L"";
    for (wchar_t c : g_config.save_directory) {
        if (c == L'\\') escaped_path += L"\\\\";
        else escaped_path += c;
    }
    fwprintf(file, L"  \"save_directory\": \"%ls\",\n", escaped_path.c_str());
    fwprintf(file, L"  \"notification\": %ls,\n", g_config.notification ? L"true" : L"false");
    fwprintf(file, L"  \"capture_mode\": \"%ls\"\n", g_config.capture_mode.c_str());
    fwprintf(file, L"}\n");
    
    fclose(file);
}

// ========== 注册表开机自启动控制 ==========
void ApplyAutostart() {
    HKEY hKey;
    LPCWSTR regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyEx(HKEY_CURRENT_USER, regPath, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (g_config.auto_start) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileName(NULL, exePath, MAX_PATH);
            wstring val = L"\"" + wstring(exePath) + L"\"";
            RegSetValueEx(hKey, L"CaptureTool", 0, REG_SZ, (const BYTE*)val.c_str(), (val.length() + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValue(hKey, L"CaptureTool");
        }
        RegCloseKey(hKey);
    }
}

// ========== 底层内核钩子 (Low-Level Hooks) ==========
bool IsModifierPressed(const wstring& mod) {
    if (mod == L"ctrl") return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (mod == L"shift") return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (mod == L"alt") return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    return false;
}

wstring VkToCharName(DWORD vk) {
    if (vk >= 0x41 && vk <= 0x5A) {
        wchar_t c = (wchar_t)vk;
        wstring s = L"";
        s += towlower(c);
        return s;
    }
    if (vk >= 0x30 && vk <= 0x39) {
        wchar_t c = (wchar_t)vk;
        wstring s = L"";
        s += c;
        return s;
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return L"f" + ToWString(vk - VK_F1 + 1);
    }
    switch (vk) {
        case VK_SNAPSHOT: return L"print_screen";
        case VK_ESCAPE: return L"esc";
        case VK_SPACE: return L"space";
        case VK_RETURN: return L"enter";
        case VK_TAB: return L"tab";
        case VK_BACK: return L"backspace";
        case VK_DELETE: return L"delete";
        case VK_HOME: return L"home";
        case VK_END: return L"end";
        case VK_PRIOR: return L"page_up";
        case VK_NEXT: return L"page_down";
        case VK_UP: return L"up";
        case VK_DOWN: return L"down";
        case VK_LEFT: return L"left";
        case VK_RIGHT: return L"right";
        case VK_INSERT: return L"insert";
        case VK_CAPITAL: return L"caps_lock";
        case VK_NUMLOCK: return L"num_lock";
        case VK_SCROLL: return L"scroll_lock";
        case VK_PAUSE: return L"pause";
    }
    return L"";
}

bool IsModifierVk(DWORD vk) {
    return (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
            vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
            vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU);
}

wstring FormatHotkeyConfig(const HotkeyConfig& config) {
    wstring s = L"";
    if (config.type == L"mouse") {
        for (const auto& k : config.keys) {
            if (k == L"ctrl") s += L"Ctrl + ";
            else if (k == L"shift") s += L"Shift + ";
            else if (k == L"alt") s += L"Alt + ";
        }
        s += (config.mouse_button == L"x1") ? L"Mouse4" : L"Mouse5";
    } else {
        for (size_t i = 0; i < config.keys.size(); ++i) {
            wstring k = config.keys[i];
            if (k == L"ctrl") s += L"Ctrl";
            else if (k == L"shift") s += L"Shift";
            else if (k == L"alt") s += L"Alt";
            else {
                if (k.length() == 1) {
                    s += towupper(k[0]);
                } else if (k == L"print_screen") {
                    s += L"PrintScreen";
                } else {
                    wstring cap = k;
                    cap[0] = towupper(cap[0]);
                    s += cap;
                }
            }
            if (i < config.keys.size() - 1) s += L" + ";
        }
    }
    if (s.empty()) return L"无";
    return s;
}

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*)lParam;
        if (g_isRecordingHotkey) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                SendMessage(g_hWndSettings, WM_HOTKEY_RECORDED, wParam, kbd->vkCode);
            }
            return 1; // Intercept all keyboard events during recording
        }
        if (g_config.hotkey.type == L"keyboard") {
            wstring trigger_key = L"";
            vector<wstring> config_mods;
            for (const auto& k : g_config.hotkey.keys) {
                if (k == L"ctrl" || k == L"shift" || k == L"alt") {
                    config_mods.push_back(k);
                } else {
                    trigger_key = k;
                }
            }
            
            wstring pressed_name = VkToCharName(kbd->vkCode);
            if (pressed_name == trigger_key) {
                // 校验所有修饰键是否精准吻合
                bool mods_match = true;
                vector<wstring> all_mods = { L"ctrl", L"shift", L"alt" };
                for (const auto& m : all_mods) {
                    bool should_be = false;
                    for (const auto& cm : config_mods) {
                        if (cm == m) { should_be = true; break; }
                    }
                    if (IsModifierPressed(m) != should_be) {
                        mods_match = false;
                        break;
                    }
                }
                
                if (mods_match) {
                    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                        PostMessage(g_hWndMain, WM_TRIGGER_CAPTURE, 0, 0);
                    }
                    if (g_config.hotkey.suppress) {
                        return 1; // 吞噬事件
                    }
                }
            }
        }
    }
    return CallNextHookEx(g_hKeyHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lParam;
        if (g_isRecordingHotkey) {
            if (wParam == WM_XBUTTONDOWN) {
                int xbutton = GET_XBUTTON_WPARAM(ms->mouseData);
                SendMessage(g_hWndSettings, WM_HOTKEY_RECORDED, wParam, xbutton);
                return 1;
            }
            if (wParam == WM_XBUTTONUP) {
                return 1;
            }
        }
        if (g_config.hotkey.type == L"mouse" && (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP)) {
            int xbutton = GET_XBUTTON_WPARAM(ms->mouseData);
            wstring button_name = (xbutton == 1) ? L"x1" : (xbutton == 2 ? L"x2" : L"");
            
            if (button_name == g_config.hotkey.mouse_button) {
                // 校验修饰键是否精准吻合
                bool mods_match = true;
                vector<wstring> all_mods = { L"ctrl", L"shift", L"alt" };
                for (const auto& m : all_mods) {
                    bool should_be = false;
                    for (const auto& cm : g_config.hotkey.keys) {
                        if (cm == m) { should_be = true; break; }
                    }
                    if (IsModifierPressed(m) != should_be) {
                        mods_match = false;
                        break;
                    }
                }
                
                if (mods_match) {
                    if (wParam == WM_XBUTTONDOWN) {
                        PostMessage(g_hWndMain, WM_TRIGGER_CAPTURE, 0, 0);
                    }
                    if (g_config.hotkey.suppress) {
                        return 1; // 吞噬事件
                    }
                }
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

void StartHooks() {
    g_hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, g_hInstance, 0);
    g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
}

void StopHooks() {
    if (g_hKeyHook) UnhookWindowsHookEx(g_hKeyHook);
    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);
    g_hKeyHook = NULL;
    g_hMouseHook = NULL;
}

// ========== 矢量相机托盘图标程序化生成 ==========
HICON CreateProgrammaticIcon() {
    int size = 32;
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreenDC, size, size);
    HGDIOBJ hOld = SelectObject(hMemDC, hBmp);
    
    // 初始化画布背景为黑色，前景画刷
    HBRUSH hBg = CreateSolidBrush(RGB(0, 0, 0));
    RECT r = { 0, 0, size, size };
    FillRect(hMemDC, &r, hBg);
    DeleteObject(hBg);
    
    // GDI 矢量图标绘制 (相机造型)
    HBRUSH hBlue = CreateSolidBrush(RGB(0, 174, 255)); // 品牌蓝色 #00AEFF
    
    // 取景器
    RECT rView = { 10, 4, 22, 10 };
    FillRect(hMemDC, &rView, hBlue);
    
    // 机身
    RECT rBody = { 2, 8, 30, 28 };
    HGDIOBJ hOldPen = SelectObject(hMemDC, GetStockObject(NULL_PEN));
    HGDIOBJ hOldBrush = SelectObject(hMemDC, hBlue);
    RoundRect(hMemDC, rBody.left, rBody.top, rBody.right, rBody.bottom, 6, 6);
    
    // 镜头外圈 (白色)
    SelectObject(hMemDC, GetStockObject(WHITE_BRUSH));
    Ellipse(hMemDC, 9, 11, 23, 25);
    
    // 镜头内圈 (深蓝色)
    HBRUSH hDarkBlue = CreateSolidBrush(RGB(0, 120, 200));
    SelectObject(hMemDC, hDarkBlue);
    Ellipse(hMemDC, 12, 14, 20, 22);
    
    // 镜头反光点 (白色圆点)
    SelectObject(hMemDC, GetStockObject(WHITE_BRUSH));
    Ellipse(hMemDC, 13, 15, 16, 18);
    
    // 闪光灯
    HBRUSH hYellow = CreateSolidBrush(RGB(255, 220, 100));
    SelectObject(hMemDC, hYellow);
    Ellipse(hMemDC, 24, 11, 27, 14);
    
    SelectObject(hMemDC, hOldPen);
    SelectObject(hMemDC, hOldBrush);
    SelectObject(hMemDC, hOld);
    
    DeleteObject(hBlue);
    DeleteObject(hDarkBlue);
    DeleteObject(hYellow);
    ReleaseDC(NULL, hScreenDC);
    
    // 创建 Icon 结构信息
    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hBmp;  // 简化掩码
    ii.hbmColor = hBmp;
    
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hBmp);
    DeleteObject(hMemDC);
    return hIcon;
}

// ========== 浮动工具栏绘图与点击交互设计 ==========
struct ToolbarButton {
    RECT rect;
    wstring text;
    Color color;
    Color hoverColor;
    Color clickColor;
};

vector<ToolbarButton> GetToolbarButtons(const RECT& sel, int width, int height) {
    vector<ToolbarButton> btns;
    // 按钮总体尺寸 (精致扁平无 Emoji)
    int btn_w = 68;
    int btn_h = 30;
    int spacing = 6;
    int total_w = btn_w * 7 + spacing * 6 + 16; // 加上 padding (7个按钮)
    int total_h = btn_h + 12;
    if (g_annotationMode != ANNOTATION_NONE) {
        total_h += 40; // 预留属性子工具栏的空间
    }
    
    // 计算工具栏左上角坐标 (优先选区右下角外侧，边界避让)
    int tx = sel.right - total_w;
    int ty = sel.bottom + 8;
    
    if (ty + total_h > height) {
        ty = sel.top - total_h - 8;
    }
    if (ty < 0) {
        ty = sel.bottom - total_h - 8;
    }
    if (tx < 0) {
        tx = sel.left;
    }
    if (tx + total_w > width) {
        tx = width - total_w - 4;
    }
    
    int start_x = tx + 8;
    int start_y = ty + 6;
    
    // 1. 矩形
    ToolbarButton bRect;
    bRect.rect = { start_x, start_y, start_x + btn_w, start_y + btn_h };
    bRect.text = L"矩形";
    if (g_annotationMode == ANNOTATION_RECTANGLE) {
        bRect.color = Color(255, 0, 174, 255);       // 激活主题蓝 #00AEFF
        bRect.hoverColor = Color(255, 0, 150, 220);
        bRect.clickColor = Color(255, 0, 120, 190);
    } else {
        bRect.color = Color(255, 52, 73, 94);        // 未激活深蓝灰
        bRect.hoverColor = Color(255, 44, 62, 80);
        bRect.clickColor = Color(255, 30, 40, 50);
    }
    btns.push_back(bRect);
    
    // 2. 箭头
    ToolbarButton bArrow;
    bArrow.rect = { start_x + btn_w + spacing, start_y, start_x + btn_w * 2 + spacing, start_y + btn_h };
    bArrow.text = L"箭头";
    if (g_annotationMode == ANNOTATION_ARROW) {
        bArrow.color = Color(255, 0, 174, 255);      // 激活主题蓝
        bArrow.hoverColor = Color(255, 0, 150, 220);
        bArrow.clickColor = Color(255, 0, 120, 190);
    } else {
        bArrow.color = Color(255, 52, 73, 94);       // 未激活深蓝灰
        bArrow.hoverColor = Color(255, 44, 62, 80);
        bArrow.clickColor = Color(255, 30, 40, 50);
    }
    btns.push_back(bArrow);
    
    // 3. 文字
    ToolbarButton bText;
    bText.rect = { start_x + (btn_w + spacing) * 2, start_y, start_x + btn_w * 3 + spacing * 2, start_y + btn_h };
    bText.text = L"文字";
    if (g_annotationMode == ANNOTATION_TEXT) {
        bText.color = Color(255, 0, 174, 255);
        bText.hoverColor = Color(255, 0, 150, 220);
        bText.clickColor = Color(255, 0, 120, 190);
    } else {
        bText.color = Color(255, 52, 73, 94);
        bText.hoverColor = Color(255, 44, 62, 80);
        bText.clickColor = Color(255, 30, 40, 50);
    }
    btns.push_back(bText);
    
    // 4. 撤销
    ToolbarButton bUndo;
    bUndo.rect = { start_x + (btn_w + spacing) * 3, start_y, start_x + btn_w * 4 + spacing * 3, start_y + btn_h };
    bUndo.text = L"撤销";
    bUndo.color = Color(255, 70, 70, 70);
    bUndo.hoverColor = Color(255, 90, 90, 90);
    bUndo.clickColor = Color(255, 50, 50, 50);
    btns.push_back(bUndo);
    
    // 5. 确定
    ToolbarButton b1;
    b1.rect = { start_x + (btn_w + spacing) * 4, start_y, start_x + btn_w * 5 + spacing * 4, start_y + btn_h };
    b1.text = L"确定";
    b1.color = Color(255, 39, 174, 96);
    b1.hoverColor = Color(255, 46, 204, 113);
    b1.clickColor = Color(255, 30, 132, 73);
    btns.push_back(b1);
    
    // 6. 保存
    ToolbarButton b2;
    b2.rect = { start_x + (btn_w + spacing) * 5, start_y, start_x + btn_w * 6 + spacing * 5, start_y + btn_h };
    b2.text = L"保存";
    b2.color = Color(255, 41, 128, 185);
    b2.hoverColor = Color(255, 52, 152, 219);
    b2.clickColor = Color(255, 31, 111, 165);
    btns.push_back(b2);
    
    // 7. 取消
    ToolbarButton b3;
    b3.rect = { start_x + (btn_w + spacing) * 6, start_y, start_x + btn_w * 7 + spacing * 6, start_y + btn_h };
    b3.text = L"取消";
    b3.color = Color(255, 120, 120, 120);
    b3.hoverColor = Color(255, 231, 76, 60);
    b3.clickColor = Color(255, 192, 57, 43);
    btns.push_back(b3);
    
    return btns;
}

// ========== Win32 选区控制手柄边界判定函数 ==========
ResizeHandle GetHandleAtPoint(const RECT& rect, POINT pt) {
    if (!g_overlay.selectionDone) return NONE_HANDLE;
    
    int hw = HANDLE_HALF_WIDTH;
    
    // 4个角
    RECT rTL = { rect.left - hw, rect.top - hw, rect.left + hw, rect.top + hw };
    if (PtInRect(&rTL, pt)) return TL;
    
    RECT rTR = { rect.right - hw, rect.top - hw, rect.right + hw, rect.top + hw };
    if (PtInRect(&rTR, pt)) return TR;
    
    RECT rBR = { rect.right - hw, rect.bottom - hw, rect.right + hw, rect.bottom + hw };
    if (PtInRect(&rBR, pt)) return BR;
    
    RECT rBL = { rect.left - hw, rect.bottom - hw, rect.left + hw, rect.bottom + hw };
    if (PtInRect(&rBL, pt)) return BL;
    
    // 4条边中点
    int cx = rect.left + (rect.right - rect.left) / 2;
    int cy = rect.top + (rect.bottom - rect.top) / 2;
    
    RECT rT = { cx - hw, rect.top - hw, cx + hw, rect.top + hw };
    if (PtInRect(&rT, pt)) return T;
    
    RECT rR = { rect.right - hw, cy - hw, rect.right + hw, cy + hw };
    if (PtInRect(&rR, pt)) return R;
    
    RECT rB = { cx - hw, rect.bottom - hw, cx + hw, rect.bottom + hw };
    if (PtInRect(&rB, pt)) return B;
    
    RECT rL = { rect.left - hw, cy - hw, rect.left + hw, cy + hw };
    if (PtInRect(&rL, pt)) return L;
    
    // 整个选区内部 (用于平移选区)
    if (PtInRect(&rect, pt)) return M;
    
    return NONE_HANDLE;
}

void UpdateOverlayCursor(HWND hWnd, POINT pt) {
    if (g_overlay.isSelecting) {
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return;
    }
    
    ResizeHandle h = GetHandleAtPoint(g_overlay.selection, pt);
    switch (h) {
        case TL:
        case BR:
            SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
            break;
        case TR:
        case BL:
            SetCursor(LoadCursor(NULL, IDC_SIZENESW));
            break;
        case T:
        case B:
            SetCursor(LoadCursor(NULL, IDC_SIZENS));
            break;
        case L:
        case R:
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            break;
        case M:
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            break;
        default:
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            break;
    }
}

// ========== GDI+ 图像裁剪与剪贴板 CF_DIB + PNG 双写入核心算法 ==========
HBITMAP CropScreenCapture(const RECT& sel) {
    // 限制物理裁剪区域
    int x1 = max((LONG)0, sel.left);
    int y1 = max((LONG)0, sel.top);
    int x2 = min((LONG)g_screenWidth, sel.right);
    int y2 = min((LONG)g_screenHeight, sel.bottom);
    
    int w = x2 - x1;
    int h = y2 - y1;
    if (w <= 0 || h <= 0) return NULL;
    
    HDC hScreenDC = GetDC(NULL);
    HDC hDstDC = CreateCompatibleDC(hScreenDC);
    
    HBITMAP hDstBmp = CreateCompatibleBitmap(hScreenDC, w, h);
    HGDIOBJ hOldDst = SelectObject(hDstDC, hDstBmp);
    
    // 直接使用预分配的全局背景源 DC，省去临时 DC 构造与 Bitmap 选入开销
    BitBlt(hDstDC, 0, 0, w, h, g_hCaptureSourceDC, x1, y1, SRCCOPY);
    
    // 把已画的标注图形（包括箭头和矩形框）渲染到输出的位图 DC 上
    {
        Graphics g(hDstDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        for (const auto& shp : g_shapes) {
            int startX = shp.start.x - x1;
            int startY = shp.start.y - y1;
            int endX = shp.end.x - x1;
            int endY = shp.end.y - y1;
            
            Pen pen(shp.color, (REAL)shp.thickness);
            if (shp.type == SHAPE_ARROW) {
                AdjustableArrowCap arrowCap(3.5f, 4.0f, TRUE);
                pen.SetCustomEndCap(&arrowCap);
                g.DrawLine(&pen, (REAL)startX, (REAL)startY, (REAL)endX, (REAL)endY);
            } else if (shp.type == SHAPE_RECTANGLE) {
                int rx = min(startX, endX);
                int ry = min(startY, endY);
                int rw = abs(startX - endX);
                int rh = abs(startY - endY);
                g.DrawRectangle(&pen, rx, ry, rw, rh);
            } else if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                int fontSize = ThicknessToFontSize(shp.thickness);
                Font txtFont(L"Microsoft YaHei", (REAL)fontSize, FontStyleBold);
                SolidBrush txtBrush(shp.color);
                g.SetTextRenderingHint(TextRenderingHintAntiAlias);
                g.DrawString(shp.text.c_str(), -1, &txtFont, PointF((REAL)startX, (REAL)startY), &txtBrush);
            }
        }
    }
    
    SelectObject(hDstDC, hOldDst);
    
    DeleteDC(hDstDC);
    ReleaseDC(NULL, hScreenDC);
    
    return hDstBmp;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// 剪贴板 DIB 格式写入支持（兼容传统 Office/画图）
HGLOBAL BitmapToDIB(HBITMAP hBitmap) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    
    BITMAPINFOHEADER bih = { 0 };
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = bmp.bmWidth;
    bih.biHeight = bmp.bmHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 32; // 32位像素
    bih.biCompression = BI_RGB;
    
    HDC hDC = GetDC(NULL);
    DWORD dwBmpSize = ((bmp.bmWidth * 32 + 31) / 32) * 4 * bmp.bmHeight;
    HGLOBAL hDIB = GlobalAlloc(GHND, sizeof(BITMAPINFOHEADER) + dwBmpSize);
    if (!hDIB) {
        ReleaseDC(NULL, hDC);
        return NULL;
    }
    
    char* lpbi = (char*)GlobalLock(hDIB);
    memcpy(lpbi, &bih, sizeof(BITMAPINFOHEADER));
    
    GetDIBits(hDC, hBitmap, 0, bmp.bmHeight, lpbi + sizeof(BITMAPINFOHEADER), (BITMAPINFO*)lpbi, DIB_RGB_COLORS);
    
    GlobalUnlock(hDIB);
    ReleaseDC(NULL, hDC);
    return hDIB;
}

bool CopyBitmapToClipboard(HBITMAP hBmp) {
    if (!hBmp) return false;
    
    // GDI+ 转换为 PNG 内存流 (保留 Alpha 现代透明通道，兼容 Discord)
    Gdiplus::Bitmap gdiBmp(hBmp, NULL);
    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) != S_OK) return false;
    
    CLSID clsidPng;
    if (GetEncoderClsid(L"image/png", &clsidPng) == -1) {
        pStream->Release();
        return false;
    }
    
    if (gdiBmp.Save(pStream, &clsidPng, NULL) != Ok) {
        pStream->Release();
        return false;
    }
    
    HGLOBAL hPngGlobal = NULL;
    GetHGlobalFromStream(pStream, &hPngGlobal);
    if (!hPngGlobal) {
        pStream->Release();
        return false;
    }
    
    // 拷贝一份全局内存给剪贴板管理
    SIZE_T pngSize = GlobalSize(hPngGlobal);
    HGLOBAL hPngCopy = GlobalAlloc(GMEM_MOVEABLE, pngSize);
    if (hPngCopy) {
        void* dest = GlobalLock(hPngCopy);
        void* src = GlobalLock(hPngGlobal);
        memcpy(dest, src, pngSize);
        GlobalUnlock(hPngCopy);
        GlobalUnlock(hPngGlobal);
    }
    pStream->Release();
    
    // 制作 DIB 数据
    HGLOBAL hDIB = BitmapToDIB(hBmp);
    
    // 注册自定义 PNG 格式
    UINT cfPng = RegisterClipboardFormat(L"PNG");
    
    if (OpenClipboard(g_hWndMain)) {
        EmptyClipboard();
        if (hPngCopy) SetClipboardData(cfPng, hPngCopy);
        if (hDIB) SetClipboardData(CF_DIB, hDIB);
        CloseClipboard();
        return true;
    }
    
    if (hPngCopy) GlobalFree(hPngCopy);
    if (hDIB) GlobalFree(hDIB);
    return false;
}

bool SaveBitmapToFile(HBITMAP hBmp, const wstring& filepath) {
    Gdiplus::Bitmap gdiBmp(hBmp, NULL);
    CLSID clsidPng;
    if (GetEncoderClsid(L"image/png", &clsidPng) == -1) return false;
    return gdiBmp.Save(filepath.c_str(), &clsidPng, NULL) == Ok;
}

// ========== GDI+ 覆盖层双缓冲重绘 WM_PAINT 过程 ==========
void DrawOverlay(HWND hWnd, HDC hdc) {
    // 获取窗口实际尺寸（物理坐标）
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    
    // 直接使用预分配的内存双缓冲 DC (超低时延，零抖动)
    HDC hMemDC = g_hOverlayDoubleBufferDC;
    
    // 1. 极速拷贝预渲染的半透明遮罩背景图 (Direct VRAM/Kernel BitBlt)
    BitBlt(hMemDC, 0, 0, w, h, g_hCaptureMaskedSourceDC, 0, 0, SRCCOPY);
    
    Graphics g(hMemDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    
    RECT sel = g_overlay.selection;
    bool hasSelection = (sel.right - sel.left > 0 && sel.bottom - sel.top > 0);
    
    if (hasSelection) {
        // 2. 抠图：在选区部分以 Direct BitBlt 恢复清晰原背景 (零 CPU Alpha Blending 开销)
        BitBlt(hMemDC, sel.left, sel.top, sel.right - sel.left, sel.bottom - sel.top, g_hCaptureSourceDC, sel.left, sel.top, SRCCOPY);
        
        // 绘制所有已画好的标注图形 (包括箭头、矩形框、文字)
        for (const auto& shp : g_shapes) {
            Pen pen(shp.color, (REAL)shp.thickness);
            if (shp.type == SHAPE_ARROW) {
                AdjustableArrowCap arrowCap(3.5f, 4.0f, TRUE);
                pen.SetCustomEndCap(&arrowCap);
                g.DrawLine(&pen, (REAL)shp.start.x, (REAL)shp.start.y, (REAL)shp.end.x, (REAL)shp.end.y);
            } else if (shp.type == SHAPE_RECTANGLE) {
                int rx = min(shp.start.x, shp.end.x);
                int ry = min(shp.start.y, shp.end.y);
                int rw = abs(shp.start.x - shp.end.x);
                int rh = abs(shp.start.y - shp.end.y);
                g.DrawRectangle(&pen, rx, ry, rw, rh);
            } else if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                int fontSize = ThicknessToFontSize(shp.thickness);
                Font txtFont(L"Microsoft YaHei", (REAL)fontSize, FontStyleBold);
                SolidBrush txtBrush(shp.color);
                g.SetTextRenderingHint(TextRenderingHintAntiAlias);
                g.DrawString(shp.text.c_str(), -1, &txtFont, PointF((REAL)shp.start.x, (REAL)shp.start.y), &txtBrush);
            }
        }
        
        // 绘制正在编辑中的文字（带光标）
        if (g_isEditingText) {
            int fontSize = ThicknessToFontSize(g_currentThickness);
            Font txtFont(L"Microsoft YaHei", (REAL)fontSize, FontStyleBold);
            SolidBrush txtBrush(g_currentColor);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);
            
            wstring displayText = g_editingText;
            if (displayText.empty()) displayText = L" ";
            g.DrawString(g_editingText.c_str(), -1, &txtFont, PointF((REAL)g_editTextPos.x, (REAL)g_editTextPos.y), &txtBrush);
            
            // 计算光标位置
            RectF cursorBound;
            if (!g_editingText.empty()) {
                g.MeasureString(g_editingText.c_str(), -1, &txtFont, PointF((REAL)g_editTextPos.x, (REAL)g_editTextPos.y), &cursorBound);
            } else {
                cursorBound = RectF((REAL)g_editTextPos.x, (REAL)g_editTextPos.y, 0, (REAL)(fontSize + 8));
            }
            
            // 绘制闪烁光标竖线
            DWORD tick = GetTickCount();
            if ((tick / 500) % 2 == 0) {
                Pen cursorPen(g_currentColor, 2.0f);
                float cx = cursorBound.X + cursorBound.Width;
                float cy = cursorBound.Y + 2;
                float ch = cursorBound.Height - 4;
                g.DrawLine(&cursorPen, cx, cy, cx, cy + ch);
            }
            
            // 绘制输入框虚线边框
            Pen inputBorderPen(Color(150, 0, 174, 255), 1.0f);
            inputBorderPen.SetDashStyle(DashStyleDash);
            float bx = cursorBound.X - 4;
            float by = cursorBound.Y - 2;
            float bw = max(cursorBound.Width + 12.0f, 40.0f);
            float bh = cursorBound.Height + 4;
            g.DrawRectangle(&inputBorderPen, bx, by, bw, bh);
        }
        
        // 绘制正在拖拽绘制中的临时图形
        if (g_isDrawingShape) {
            Pen pen(g_tempShape.color, (REAL)g_tempShape.thickness);
            if (g_tempShape.type == SHAPE_ARROW) {
                AdjustableArrowCap arrowCap(3.5f, 4.0f, TRUE);
                pen.SetCustomEndCap(&arrowCap);
                g.DrawLine(&pen, (REAL)g_tempShape.start.x, (REAL)g_tempShape.start.y, (REAL)g_tempShape.end.x, (REAL)g_tempShape.end.y);
            } else if (g_tempShape.type == SHAPE_RECTANGLE) {
                int rx = min(g_tempShape.start.x, g_tempShape.end.x);
                int ry = min(g_tempShape.start.y, g_tempShape.end.y);
                int rw = abs(g_tempShape.start.x - g_tempShape.end.x);
                int rh = abs(g_tempShape.start.y - g_tempShape.end.y);
                g.DrawRectangle(&pen, rx, ry, rw, rh);
            }
        }
        
        // 3. 绘制选区亮蓝边框 (品牌蓝色 #00AEFF, 2px)
        Pen borderPen(Color(255, 0, 174, 255), 2);
        g.DrawRectangle(&borderPen, (int)sel.left, (int)sel.top, (int)(sel.right - sel.left), (int)(sel.bottom - sel.top));
        
        // 4. 绘制 8 方向蓝色调节手柄
        if (g_overlay.selectionDone) {
            SolidBrush handleBrush(Color(255, 0, 174, 255));
            Pen whitePen(Color(255, 255, 255, 255), 1.0f);
            
            int size = 6;
            int hs = size / 2;
            int cx = sel.left + (sel.right - sel.left) / 2;
            int cy = sel.top + (sel.bottom - sel.top) / 2;
            
            POINT points[] = {
                { sel.left, sel.top }, { cx, sel.top }, { sel.right, sel.top },
                { sel.right, cy }, { sel.right, sel.bottom }, { cx, sel.bottom },
                { sel.left, sel.bottom }, { sel.left, cy }
            };
            
            for (int i = 0; i < 8; ++i) {
                g.FillRectangle(&handleBrush, (int)(points[i].x - hs), (int)(points[i].y - hs), size, size);
                g.DrawRectangle(&whitePen, (int)(points[i].x - hs), (int)(points[i].y - hs), size, size);
            }
        }
        
        // 5. 绘制尺寸标签
        wstring dimText = ToWString(sel.right - sel.left) + L" \u00d7 " + ToWString(sel.bottom - sel.top);
        Font font(L"Microsoft YaHei", 9, FontStyleBold);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        
        RectF textBounding;
        g.MeasureString(dimText.c_str(), -1, &font, PointF(0, 0), &textBounding);
        
        int label_w = (int)textBounding.Width + 12;
        int label_h = (int)textBounding.Height + 6;
        int label_x = sel.left;
        int label_y = sel.top - label_h - 6;
        if (label_y < 0) label_y = sel.bottom + 6;
        if (label_x + label_w > w) label_x = w - label_w - 4;
        
        SolidBrush textBgBrush(Color(180, 0, 0, 0));
        g.FillRectangle(&textBgBrush, label_x, label_y, label_w, label_h);
        
        SolidBrush textBrush(Color(255, 255, 255, 255));
        g.DrawString(dimText.c_str(), -1, &font, RectF((REAL)label_x, (REAL)label_y, (REAL)label_w, (REAL)label_h), &format, &textBrush);
        
        // 6. 绘制悬浮工具栏 (口 矩形, ↗ 箭头, ↶ 撤销, ✓ 确认, 💾 保存, ✗ 取消)
        if (g_overlay.selectionDone && !g_overlay.isResizing) {
            auto buttons = GetToolbarButtons(sel, w, h);
            
            // 计算工具栏大背景盒 (深黑圆角底盒 #1E1E1E)
            int min_x = buttons[0].rect.left - 8;
            int max_x = buttons.back().rect.right + 8;
            int min_y = buttons[0].rect.top - 6;
            int max_y = buttons[0].rect.bottom + 6;
            
            SolidBrush barBrush(Color(220, 30, 30, 30));
            g.FillRectangle(&barBrush, min_x, min_y, max_x - min_x, max_y - min_y);
            
            // 绘制 1px 细边框，符合方正简约气质
            Pen barBorderPen(Color(255, 60, 60, 60), 1.0f);
            g.DrawRectangle(&barBorderPen, min_x, min_y, max_x - min_x, max_y - min_y);
            
            Font btnFont(L"Microsoft YaHei", 9, FontStyleBold);
            for (size_t i = 0; i < buttons.size(); ++i) {
                Color c = buttons[i].color;
                if (g_overlay.hoveredButton == (int)(i + 1)) {
                    c = buttons[i].hoverColor;
                }
                
                SolidBrush bBrush(c);
                RECT br = buttons[i].rect;
                g.FillRectangle(&bBrush, (int)br.left, (int)br.top, (int)(br.right - br.left), (int)(br.bottom - br.top));
                
                g.DrawString(buttons[i].text.c_str(), -1, &btnFont, RectF((REAL)br.left, (REAL)br.top, (REAL)(br.right - br.left), (REAL)(br.bottom - br.top)), &format, &textBrush);
            }
            
            // 7. 绘制属性子工具栏 (仅在激活标注模式时显示)
            if (g_annotationMode != ANNOTATION_NONE) {
                int st_x = min_x;
                int st_y = max_y + 4;
                int st_w = max_x - min_x;
                int st_h = 36;
                
                g.FillRectangle(&barBrush, st_x, st_y, st_w, st_h);
                g.DrawRectangle(&barBorderPen, st_x, st_y, st_w, st_h);
                
                // --- 绘制粗细选择器 ---
                int thicks[] = { 2, 4, 8 };
                int dot_sizes[] = { 6, 10, 14 };
                for (int i = 0; i < 3; ++i) {
                    int bx = st_x + 12 + i * 30;
                    int by = st_y + 6;
                    int bw = 24;
                    int bh = 24;
                    
                    // 如果被选中，绘制浅灰色圆角高亮边框
                    if (g_currentThickness == thicks[i]) {
                        Pen selPen(Color(200, 255, 255, 255), 1.5f);
                        g.DrawRectangle(&selPen, bx, by, bw, bh);
                    }
                    
                    // 绘制中间的白色圆点
                    int ds = dot_sizes[i];
                    int dx = bx + (bw - ds) / 2;
                    int dy = by + (bh - ds) / 2;
                    g.FillEllipse(&textBrush, dx, dy, ds, ds);
                }
                
                // --- 绘制颜色选择器 ---
                Color colors[] = {
                    Color(255, 231, 76, 60),      // 红
                    Color(255, 52, 152, 219),     // 蓝
                    Color(255, 46, 204, 113),     // 绿
                    Color(255, 241, 196, 15),     // 黄
                    Color(255, 127, 140, 141),    // 灰
                    Color(255, 255, 255, 255),    // 白
                    Color(255, 44, 62, 80)        // 黑
                };
                
                for (int i = 0; i < 7; ++i) {
                    int cx = st_x + 120 + i * 28;
                    int cy = st_y + 8;
                    int cw = 20;
                    int ch = 20;
                    
                    SolidBrush cBrush(colors[i]);
                    g.FillRectangle(&cBrush, cx, cy, cw, ch);
                    
                    // 如果被选中，绘制一个带有黑/白内缩外框的高亮框
                    if (g_currentColor.GetValue() == colors[i].GetValue()) {
                        Pen whitePen(Color(255, 255, 255, 255), 2.0f);
                        g.DrawRectangle(&whitePen, cx - 2, cy - 2, cw + 4, ch + 4);
                        
                        Pen blackPen(Color(255, 0, 0, 0), 1.0f);
                        g.DrawRectangle(&blackPen, cx - 1, cy - 1, cw + 2, ch + 2);
                    }
                }
            }
        }
    } else {
        // 智能窗口悬停高亮绘制逻辑
        if (g_overlay.hasDetectedWindow) {
            RECT rWin = g_overlay.detectedWindowRect;
            int winW = rWin.right - rWin.left;
            int winH = rWin.bottom - rWin.top;
            if (winW > 0 && winH > 0) {
                // A. 抠图：极速将探测出的悬停窗口部分复原为无遮罩原截屏背景 (零 CPU 混合开销)
                BitBlt(hMemDC, rWin.left, rWin.top, winW, winH, g_hCaptureSourceDC, rWin.left, rWin.top, SRCCOPY);
                
                // B. 绘制悬停蓝光方正边框 (1.5px 品牌蓝 #00AEFF)
                Pen hoverPen(Color(255, 0, 174, 255), 1.5f);
                g.DrawRectangle(&hoverPen, rWin.left, rWin.top, winW, winH);
            }
        } else {
            // 绘制初始化指引提示文字
            wstring hint = L"拖拽鼠标框选截图区域    按 Esc 取消";
            Font font(L"Microsoft YaHei", 12, FontStyleRegular);
            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);
            
            RectF textBounding;
            g.MeasureString(hint.c_str(), -1, &font, PointF(0, 0), &textBounding);
            
            int box_w = (int)textBounding.Width + 40;
            int box_h = (int)textBounding.Height + 20;
            int box_x = w / 2 - box_w / 2;
            int box_y = h / 2 - box_h / 2;
            
            SolidBrush textBgBrush(Color(140, 0, 0, 0));
            g.FillRectangle(&textBgBrush, box_x, box_y, box_w, box_h);
            
            SolidBrush textBrush(Color(180, 255, 255, 255));
            g.DrawString(hint.c_str(), -1, &font, RectF((REAL)box_x, (REAL)box_y, (REAL)box_w, (REAL)box_h), &format, &textBrush);
        }
    }
    
    // 8. 一次性将完整的双缓冲结果贴图到屏幕 DC (Buttery Smooth!)
    BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
}

// ========== 智能窗口框选探测与过滤 API ==========
BOOL IsValidWindow(HWND hWnd) {
    if (!hWnd || hWnd == g_hWndOverlay || hWnd == g_hWndMain || hWnd == g_hWndSettings || hWnd == GetShellWindow() || hWnd == GetDesktopWindow()) {
        return FALSE;
    }
    if (!IsWindowVisible(hWnd)) {
        return FALSE;
    }
    
    // 过滤掉无标题的子窗口或工具提示等
    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    
    if (exStyle & WS_EX_TOOLWINDOW) {
        return FALSE;
    }
    
    // 获取窗口标题，无标题且类名奇怪的过滤掉
    wchar_t title[256];
    GetWindowText(hWnd, title, 256);
    if (wcslen(title) == 0) {
        wchar_t className[256];
        GetClassName(hWnd, className, 256);
        if (wcscmp(className, L"Windows.UI.Core.CoreWindow") != 0 &&
            wcscmp(className, L"ApplicationFrameWindow") != 0) {
            return FALSE;
        }
    }
    
    // 过滤 Cloaked
    DWORD cloaked = 0;
    DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) {
        return FALSE;
    }
    
    // 过滤太小的隐藏窗口
    RECT r;
    GetWindowRect(hWnd, &r);
    if (r.right - r.left < 40 || r.bottom - r.top < 40) {
        return FALSE;
    }
    
    return TRUE;
}

// EnumWindows Z-order callback: finds the topmost valid window containing the point
struct FindWindowData {
    POINT ptScreen;
    HWND resultHwnd;
};

BOOL CALLBACK EnumWindowsFindAtPoint(HWND hWnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    
    if (!IsValidWindow(hWnd)) return TRUE;
    
    RECT rc;
    HRESULT hr = DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));
    if (FAILED(hr)) {
        GetWindowRect(hWnd, &rc);
    }
    
    if (data->ptScreen.x >= rc.left && data->ptScreen.x < rc.right &&
        data->ptScreen.y >= rc.top && data->ptScreen.y < rc.bottom) {
        data->resultHwnd = hWnd;
        return FALSE;
    }
    
    return TRUE;
}

BOOL GetWindowRectAtPoint(POINT pt, RECT* pRect) {
    FindWindowData data;
    data.ptScreen = pt;
    data.resultHwnd = NULL;
    
    EnumWindows(EnumWindowsFindAtPoint, (LPARAM)&data);
    
    if (!data.resultHwnd) return FALSE;
    
    RECT rc;
    HRESULT hr = DwmGetWindowAttribute(data.resultHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));
    if (FAILED(hr)) {
        GetWindowRect(data.resultHwnd, &rc);
    }
    
    pRect->left = rc.left - g_screenX;
    pRect->top = rc.top - g_screenY;
    pRect->right = rc.right - g_screenX;
    pRect->bottom = rc.bottom - g_screenY;
    
    return TRUE;
}

// ========== 全屏 Overlay 覆盖画布窗口过程 WndProc ==========
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            DrawOverlay(hWnd, hdc);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_MOUSEMOVE: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            
            if (g_isDraggingText && g_draggingTextIndex >= 0 && g_draggingTextIndex < (int)g_shapes.size()) {
                g_shapes[g_draggingTextIndex].start.x = pt.x - g_textDragOffset.x;
                g_shapes[g_draggingTextIndex].start.y = pt.y - g_textDragOffset.y;
                g_shapes[g_draggingTextIndex].end = g_shapes[g_draggingTextIndex].start;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDrawingShape) {
                g_tempShape.end = pt;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_overlay.isSelecting) {
                if (g_overlay.isPossibleClick) {
                    int dx = pt.x - g_overlay.startPos.x;
                    int dy = pt.y - g_overlay.startPos.y;
                    if (sqrt(dx * dx + dy * dy) > 5) {
                        g_overlay.isPossibleClick = false; // 鼠标拖拽距离超过 5 像素，判定为拖拽框选，取消单击判定
                    }
                }
                g_overlay.selection.right = pt.x;
                g_overlay.selection.bottom = pt.y;
                InvalidateRect(hWnd, NULL, FALSE);
            } 
            else if (g_overlay.isResizing) {
                int dx = pt.x - g_overlay.dragStartPos.x;
                int dy = pt.y - g_overlay.dragStartPos.y;
                RECT r = g_overlay.dragStartRect;
                
                if (g_overlay.activeHandle == M) {
                    // 移动选区
                    int w = r.right - r.left;
                    int h = r.bottom - r.top;
                    r.left += dx;
                    r.top += dy;
                    r.right = r.left + w;
                    r.bottom = r.top + h;
                    
                    // 越界保护
                    if (r.left < 0) { r.left = 0; r.right = w; }
                    if (r.top < 0) { r.top = 0; r.bottom = h; }
                    if (r.right > g_screenWidth) { r.right = g_screenWidth; r.left = g_screenWidth - w; }
                    if (r.bottom > g_screenHeight) { r.bottom = g_screenHeight; r.top = g_screenHeight - h; }
                } else {
                    // 拉伸边缘
                    int left = r.left;
                    int top = r.top;
                    int right = r.right;
                    int bottom = r.bottom;
                    ResizeHandle hHandle = g_overlay.activeHandle;
                    
                    if (hHandle == TL || hHandle == L || hHandle == BL) {
                        left = min(right - MIN_SELECTION_SIZE, left + dx);
                    }
                    if (hHandle == TR || hHandle == R || hHandle == BR) {
                        right = max(left + MIN_SELECTION_SIZE, right + dx);
                    }
                    if (hHandle == TL || hHandle == T || hHandle == TR) {
                        top = min(bottom - MIN_SELECTION_SIZE, top + dy);
                    }
                    if (hHandle == BL || hHandle == B || hHandle == BR) {
                        bottom = max(top + MIN_SELECTION_SIZE, bottom + dy);
                    }
                    
                    r = { left, top, right, bottom };
                }
                
                g_overlay.selection = r;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (!g_overlay.selectionDone) {
                // 如果用户没有框选完成，且正处于悬停探索状态下：自动进行智能窗口捕获
                POINT ptScreen = pt;
                ptScreen.x += g_screenX;
                ptScreen.y += g_screenY;
                
                RECT rcWin;
                if (GetWindowRectAtPoint(ptScreen, &rcWin)) {
                    if (!g_overlay.hasDetectedWindow || 
                        rcWin.left != g_overlay.detectedWindowRect.left ||
                        rcWin.top != g_overlay.detectedWindowRect.top ||
                        rcWin.right != g_overlay.detectedWindowRect.right ||
                        rcWin.bottom != g_overlay.detectedWindowRect.bottom) {
                        
                        g_overlay.detectedWindowRect = rcWin;
                        g_overlay.hasDetectedWindow = true;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else {
                    if (g_overlay.hasDetectedWindow) {
                        g_overlay.hasDetectedWindow = false;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                SetCursor(LoadCursor(NULL, IDC_CROSS));
            }
            else {
                // 如果正处于标注模式，且光标在选区内，设置为十字光标，不要显示大小调整光标
                if (g_annotationMode != ANNOTATION_NONE && PtInRect(&g_overlay.selection, pt)) {
                    // 如果鼠标在工具栏或子工具栏按钮范围内，让它显示手形光标
                    bool onButton = false;
                    if (g_overlay.selectionDone) {
                        auto buttons = GetToolbarButtons(g_overlay.selection, g_screenWidth, g_screenHeight);
                        for (const auto& btn : buttons) {
                            if (PtInRect(&btn.rect, pt)) { onButton = true; break; }
                        }
                        
                        // 判定是否在属性子工具栏区域
                        int min_x = buttons[0].rect.left - 8;
                        int max_x = buttons.back().rect.right + 8;
                        int max_y = buttons[0].rect.bottom + 6;
                        RECT stRect = { min_x, max_y + 4, max_x, max_y + 40 };
                        if (PtInRect(&stRect, pt)) { onButton = true; }
                    }
                    if (onButton) {
                        SetCursor(LoadCursor(NULL, IDC_HAND));
                    } else {
                        SetCursor(LoadCursor(NULL, IDC_CROSS));
                    }
                } else {
                    // 更新鼠标形状与工具栏按钮高亮
                    UpdateOverlayCursor(hWnd, pt);
                }
                
                int hovered = 0;
                if (g_overlay.selectionDone) {
                    auto buttons = GetToolbarButtons(g_overlay.selection, g_screenWidth, g_screenHeight);
                    for (size_t i = 0; i < buttons.size(); ++i) {
                        if (PtInRect(&buttons[i].rect, pt)) {
                            hovered = (int)(i + 1);
                            break;
                        }
                    }
                }
                if (hovered != g_overlay.hoveredButton) {
                    g_overlay.hoveredButton = hovered;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            
            if (!g_overlay.selectionDone) {
                // 开始全新框选流程 (可能是单击选择窗体，也可能是长按拖拽选区)
                g_overlay.startPos = pt;
                g_overlay.selection = { pt.x, pt.y, pt.x, pt.y };
                g_overlay.isSelecting = true;
                g_overlay.isPossibleClick = true; // 先假设是点击，若鼠标产生拖拽位移则判定为长按拖拽
                InvalidateRect(hWnd, NULL, FALSE);
            } else {
                // 1. 判断是否点击在主工具栏或子工具栏的物理边界区域内 (防止误触导致选区重置)
                bool clickedOnToolbar = false;
                RECT toolbarBox = { 0, 0, 0, 0 };
                RECT subToolbarBox = { 0, 0, 0, 0 };
                
                auto buttons = GetToolbarButtons(g_overlay.selection, g_screenWidth, g_screenHeight);
                if (!buttons.empty()) {
                    int min_x = buttons[0].rect.left - 8;
                    int max_x = buttons.back().rect.right + 8;
                    int min_y = buttons[0].rect.top - 6;
                    int max_y = buttons[0].rect.bottom + 6;
                    
                    toolbarBox = { min_x, min_y, max_x, max_y };
                    if (PtInRect(&toolbarBox, pt)) {
                        clickedOnToolbar = true;
                    }
                    
                    if (g_annotationMode != ANNOTATION_NONE) {
                        subToolbarBox = { min_x, max_y + 4, max_x, max_y + 40 };
                        if (PtInRect(&subToolbarBox, pt)) {
                            clickedOnToolbar = true;
                        }
                    }
                }
                
                // 2. 如果点击在属性子工具栏区域，处理粗细和颜色选择
                if (g_annotationMode != ANNOTATION_NONE && !buttons.empty() && PtInRect(&subToolbarBox, pt)) {
                    int min_x = buttons[0].rect.left - 8;
                    int max_y = buttons[0].rect.bottom + 6;
                    int st_x = min_x;
                    int st_y = max_y + 4;
                    
                    // A. 粗细项点击
                    int thicks[] = { 2, 4, 8 };
                    for (int i = 0; i < 3; ++i) {
                        int bx = st_x + 12 + i * 30;
                        int by = st_y + 6;
                        RECT dotRect = { bx, by, bx + 24, by + 24 };
                        if (PtInRect(&dotRect, pt)) {
                            g_currentThickness = thicks[i];
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                    }
                    // B. 颜色项点击
                    Color colors[] = {
                        Color(255, 231, 76, 60),      // 红
                        Color(255, 52, 152, 219),     // 蓝
                        Color(255, 46, 204, 113),     // 绿
                        Color(255, 241, 196, 15),     // 黄
                        Color(255, 127, 140, 141),    // 灰
                        Color(255, 255, 255, 255),    // 白
                        Color(255, 44, 62, 80)        // 黑
                    };
                    for (int i = 0; i < 7; ++i) {
                        int cx = st_x + 120 + i * 28;
                        int cy = st_y + 8;
                        RECT colRect = { cx, cy, cx + 20, cy + 20 };
                        if (PtInRect(&colRect, pt)) {
                            g_currentColor = colors[i];
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                    }
                    break; // 拦截消息，不往下处理
                }
                
                // 3. 判断是否精准点击在主工具栏按钮上
                int clickedBtn = 0;
                for (size_t i = 0; i < buttons.size(); ++i) {
                    if (PtInRect(&buttons[i].rect, pt)) {
                        clickedBtn = (int)(i + 1);
                        break;
                    }
                }
                
                if (clickedBtn > 0) {
                    int btn = clickedBtn;
                    g_overlay.hoveredButton = clickedBtn; // 同步高亮状态
                    
                    // 切换标注模式时，先提交正在编辑的文字
                    auto finalizeEditingText = [&]() {
                        if (g_isEditingText && !g_editingText.empty()) {
                            DrawingShape textShape;
                            textShape.type = SHAPE_TEXT;
                            textShape.start = g_editTextPos;
                            textShape.end = g_editTextPos;
                            textShape.color = g_currentColor;
                            textShape.thickness = g_currentThickness;
                            textShape.text = g_editingText;
                            g_shapes.push_back(textShape);
                        }
                        g_isEditingText = false;
                        g_editingText = L"";
                    };
                    
                    if (btn == 1) { // 矩形
                        finalizeEditingText();
                        if (g_annotationMode == ANNOTATION_RECTANGLE) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_RECTANGLE;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 2) { // 箭头
                        finalizeEditingText();
                        if (g_annotationMode == ANNOTATION_ARROW) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_ARROW;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 3) { // 文字
                        finalizeEditingText();
                        if (g_annotationMode == ANNOTATION_TEXT) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_TEXT;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 4) { // 撤销
                        if (g_isEditingText) {
                            g_isEditingText = false;
                            g_editingText = L"";
                        } else if (!g_shapes.empty()) {
                            g_shapes.pop_back();
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 5) { // 确定
                        finalizeEditingText();
                        HBITMAP hBmp = CropScreenCapture(g_overlay.selection);
                        if (hBmp) {
                            CopyBitmapToClipboard(hBmp);
                            DeleteObject(hBmp);
                        }
                        SendMessage(hWnd, WM_CLOSE, 0, 0);
                    } 
                    else if (btn == 6) { // 保存
                        finalizeEditingText();
                        HBITMAP hBmp = CropScreenCapture(g_overlay.selection);
                        if (hBmp) {
                            CopyBitmapToClipboard(hBmp);
                            
                            OPENFILENAME ofn = { 0 };
                            wchar_t fileSz[MAX_PATH] = L"screenshot.png";
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = hWnd;
                            ofn.lpstrFilter = L"PNG Image\0*.png\0";
                            ofn.lpstrFile = fileSz;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                            ofn.lpstrDefExt = L"png";
                            
                            if (GetSaveFileName(&ofn)) {
                                SaveBitmapToFile(hBmp, ofn.lpstrFile);
                            }
                            DeleteObject(hBmp);
                        }
                        SendMessage(hWnd, WM_CLOSE, 0, 0);
                    }
                    else if (btn == 7) { // 取消
                        SendMessage(hWnd, WM_CLOSE, 0, 0);
                    }
                    break; // 拦截消息，不往下处理
                }
                
                // 4. 如果点击在工具栏的空白/边框区域（非按钮），直接拦截，防止误触导致选区重置
                if (clickedOnToolbar) {
                    break;
                }
                
                // 5. 如果处于标注模式，且在选区内
                if (g_annotationMode != ANNOTATION_NONE && PtInRect(&g_overlay.selection, pt)) {
                    if (g_annotationMode == ANNOTATION_TEXT) {
                        // 文字模式：检查是否点击在已有文字上（拖动）
                        bool hitExistingText = false;
                        HDC tmpDC = GetDC(hWnd);
                        Graphics tmpG(tmpDC);
                        for (int i = (int)g_shapes.size() - 1; i >= 0; --i) {
                            if (g_shapes[i].type == SHAPE_TEXT && !g_shapes[i].text.empty()) {
                                int fs = ThicknessToFontSize(g_shapes[i].thickness);
                                Font tmpFont(L"Microsoft YaHei", (REAL)fs, FontStyleBold);
                                RectF bounds;
                                tmpG.MeasureString(g_shapes[i].text.c_str(), -1, &tmpFont, PointF((REAL)g_shapes[i].start.x, (REAL)g_shapes[i].start.y), &bounds);
                                RECT textRect = { (int)bounds.X - 4, (int)bounds.Y - 4, (int)(bounds.X + bounds.Width) + 4, (int)(bounds.Y + bounds.Height) + 4 };
                                if (PtInRect(&textRect, pt)) {
                                    g_isDraggingText = true;
                                    g_draggingTextIndex = i;
                                    g_textDragOffset.x = pt.x - g_shapes[i].start.x;
                                    g_textDragOffset.y = pt.y - g_shapes[i].start.y;
                                    SetCapture(hWnd);
                                    hitExistingText = true;
                                    break;
                                }
                            }
                        }
                        ReleaseDC(hWnd, tmpDC);
                        
                        if (!hitExistingText) {
                            // 提交上一次编辑中的文字
                            if (g_isEditingText && !g_editingText.empty()) {
                                DrawingShape textShape;
                                textShape.type = SHAPE_TEXT;
                                textShape.start = g_editTextPos;
                                textShape.end = g_editTextPos;
                                textShape.color = g_currentColor;
                                textShape.thickness = g_currentThickness;
                                textShape.text = g_editingText;
                                g_shapes.push_back(textShape);
                            }
                            // 在点击位置开始新的文字输入
                            g_isEditingText = true;
                            g_editingText = L"";
                            g_editTextPos = pt;
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    } else {
                        // 箭头/矩形模式
                        g_isDrawingShape = true;
                        g_tempShape.type = (g_annotationMode == ANNOTATION_ARROW) ? SHAPE_ARROW : SHAPE_RECTANGLE;
                        g_tempShape.start = pt;
                        g_tempShape.end = pt;
                        g_tempShape.color = g_currentColor;
                        g_tempShape.thickness = g_currentThickness;
                        SetCapture(hWnd);
                    }
                } else {
                    // 检查是否点在控制手柄上以缩放或平移
                    ResizeHandle h = GetHandleAtPoint(g_overlay.selection, pt);
                    if (h != NONE_HANDLE) {
                        g_overlay.isResizing = true;
                        g_overlay.activeHandle = h;
                        g_overlay.dragStartPos = pt;
                        g_overlay.dragStartRect = g_overlay.selection;
                    } else {
                        // 点在外面，取消当前选区并起步重画
                        g_overlay.selectionDone = false;
                        g_shapes.clear();
                        g_annotationMode = ANNOTATION_NONE;
                        g_overlay.startPos = pt;
                        g_overlay.selection = { pt.x, pt.y, pt.x, pt.y };
                        g_overlay.isSelecting = true;
                        g_overlay.isPossibleClick = true; // 开始全新悬停/拖拉判定
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (g_isDraggingText) {
                g_isDraggingText = false;
                g_draggingTextIndex = -1;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDrawingShape) {
                g_isDrawingShape = false;
                ReleaseCapture();
                
                int dx = g_tempShape.end.x - g_tempShape.start.x;
                int dy = g_tempShape.end.y - g_tempShape.start.y;
                if (sqrt(dx * dx + dy * dy) > 5) {
                    g_shapes.push_back(g_tempShape);
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_overlay.isSelecting) {
                g_overlay.isSelecting = false;
                
                if (g_overlay.isPossibleClick) {
                    // 这是一次短促的单击选择窗口事件！
                    if (g_overlay.hasDetectedWindow) {
                        g_overlay.selection = g_overlay.detectedWindowRect;
                        g_overlay.selectionDone = true;
                        g_overlay.hasDetectedWindow = false; // 选定后关闭悬停指示
                    } else {
                        g_overlay.selection = { 0, 0, 0, 0 };
                        g_overlay.selectionDone = false;
                    }
                } else {
                    // 这是一次长按鼠标左键拖拽框选的流程！
                    g_overlay.selection.left = min(g_overlay.startPos.x, (LONG)LOWORD(lParam));
                    g_overlay.selection.top = min(g_overlay.startPos.y, (LONG)HIWORD(lParam));
                    g_overlay.selection.right = max(g_overlay.startPos.x, (LONG)LOWORD(lParam));
                    g_overlay.selection.bottom = max(g_overlay.startPos.y, (LONG)HIWORD(lParam));
                    
                    int w = g_overlay.selection.right - g_overlay.selection.left;
                    int h = g_overlay.selection.bottom - g_overlay.selection.top;
                    
                    if (w > MIN_SELECTION_SIZE && h > MIN_SELECTION_SIZE) {
                        g_overlay.selectionDone = true;
                    } else {
                        g_overlay.selection = { 0, 0, 0, 0 };
                        g_overlay.selectionDone = false;
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
            } 
            else if (g_overlay.isResizing) {
                g_overlay.isResizing = false;
                g_overlay.activeHandle = NONE_HANDLE;
                
                // 限制最小边界
                int w = g_overlay.selection.right - g_overlay.selection.left;
                int h = g_overlay.selection.bottom - g_overlay.selection.top;
                if (w <= MIN_SELECTION_SIZE || h <= MIN_SELECTION_SIZE) {
                    g_overlay.selection = { 0, 0, 0, 0 };
                    g_overlay.selectionDone = false;
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_RBUTTONDOWN: {
            break;
        }
        case WM_RBUTTONUP: {
            SendMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }
        case WM_KEYDOWN: {
            if (g_isEditingText) {
                if (wParam == VK_BACK) {
                    if (!g_editingText.empty()) {
                        g_editingText.pop_back();
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (wParam == VK_RETURN) {
                    // 回车确认文字
                    if (!g_editingText.empty()) {
                        DrawingShape textShape;
                        textShape.type = SHAPE_TEXT;
                        textShape.start = g_editTextPos;
                        textShape.end = g_editTextPos;
                        textShape.color = g_currentColor;
                        textShape.thickness = g_currentThickness;
                        textShape.text = g_editingText;
                        g_shapes.push_back(textShape);
                    }
                    g_isEditingText = false;
                    g_editingText = L"";
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wParam == VK_ESCAPE) {
                    g_isEditingText = false;
                    g_editingText = L"";
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                break;
            }
            if (wParam == VK_ESCAPE) {
                SendMessage(hWnd, WM_CLOSE, 0, 0);
            }
            break;
        }
        case WM_CHAR: {
            if (g_isEditingText) {
                wchar_t ch = (wchar_t)wParam;
                if (ch >= 32 && ch != 127) {
                    g_editingText += ch;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == 1 && g_isEditingText) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_CLOSE: {
            g_overlay.selectionDone = false;
            g_overlay.selection = { 0, 0, 0, 0 };
            g_overlay.hasDetectedWindow = false;
            g_overlay.isPossibleClick = false;
            g_overlay.isSelecting = false;
            g_overlay.isResizing = false;
            g_isDrawingShape = false;
            g_isEditingText = false;
            g_editingText = L"";
            g_isDraggingText = false;
            g_draggingTextIndex = -1;
            g_shapes.clear();
            g_annotationMode = ANNOTATION_NONE;
            KillTimer(hWnd, 1);
            DestroyWindow(hWnd);
            g_hWndOverlay = NULL;
            
            // 释放超高性能渲染双缓冲与源缓存 DCs 和 Bitmaps
            if (g_hOverlayDoubleBufferDC) {
                SelectObject(g_hOverlayDoubleBufferDC, g_hOldOverlayDoubleBufferBmp);
                DeleteDC(g_hOverlayDoubleBufferDC);
                g_hOverlayDoubleBufferDC = NULL;
            }
            if (g_hOverlayDoubleBufferBmp) {
                DeleteObject(g_hOverlayDoubleBufferBmp);
                g_hOverlayDoubleBufferBmp = NULL;
            }
            if (g_hCaptureSourceDC) {
                SelectObject(g_hCaptureSourceDC, g_hOldCaptureSourceBmp);
                DeleteDC(g_hCaptureSourceDC);
                g_hCaptureSourceDC = NULL;
            }
            if (g_hCaptureMaskedSourceDC) {
                SelectObject(g_hCaptureMaskedSourceDC, g_hOldCaptureMaskedSourceBmp);
                DeleteDC(g_hCaptureMaskedSourceDC);
                g_hCaptureMaskedSourceDC = NULL;
            }
            
            // 释放大屏幕截图内存
            if (g_hScreenCapture) {
                DeleteObject(g_hScreenCapture);
                g_hScreenCapture = NULL;
            }
            if (g_hScreenCaptureMasked) {
                DeleteObject(g_hScreenCaptureMasked);
                g_hScreenCaptureMasked = NULL;
            }
            
            // 截图恢复后重新运行全局快捷键
            StartHooks();
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ========== 触发截图流程 ==========
void TriggerCapture() {
    if (g_hWndOverlay) return; // 正在截图，跳过
    
    // 截图期间暂停钩子响应，防止二次冲突
    StopHooks();
    
    // 1. 获取屏幕总宽度 and 高度 (包括多显示器)
    g_screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    
    // 2. GDI 截取虚拟屏幕，生成背景 HBITMAP
    HDC hScreenDC = GetDC(NULL);
    
    // A. 截取原始屏幕画面
    HDC hScrMemDC = CreateCompatibleDC(hScreenDC);
    g_hScreenCapture = CreateCompatibleBitmap(hScreenDC, g_screenWidth, g_screenHeight);
    HGDIOBJ hOldScr = SelectObject(hScrMemDC, g_hScreenCapture);
    BitBlt(hScrMemDC, 0, 0, g_screenWidth, g_screenHeight, hScreenDC, g_screenX, g_screenY, SRCCOPY);
    SelectObject(hScrMemDC, hOldScr);
    DeleteDC(hScrMemDC);
    
    // B. 创建带有预置遮罩的背景图缓存 (单次生成遮罩，完全规避实时 Alpha Blending Math)
    g_hScreenCaptureMasked = CreateCompatibleBitmap(hScreenDC, g_screenWidth, g_screenHeight);
    HDC hMaskMemDC = CreateCompatibleDC(hScreenDC);
    HGDIOBJ hOldMasked = SelectObject(hMaskMemDC, g_hScreenCaptureMasked);
    
    HDC hCopySrcDC = CreateCompatibleDC(hScreenDC);
    HGDIOBJ hOldCopy = SelectObject(hCopySrcDC, g_hScreenCapture);
    BitBlt(hMaskMemDC, 0, 0, g_screenWidth, g_screenHeight, hCopySrcDC, 0, 0, SRCCOPY);
    SelectObject(hCopySrcDC, hOldCopy);
    DeleteDC(hCopySrcDC);
    
    {
        Graphics g(hMaskMemDC);
        SolidBrush maskBrush(Color(100, 0, 0, 0));
        g.FillRectangle(&maskBrush, 0, 0, g_screenWidth, g_screenHeight);
    }
    SelectObject(hMaskMemDC, hOldMasked);
    DeleteDC(hMaskMemDC);
    
    // C. 预先构建双缓冲 DC 和 Bitmap (省去 WM_PAINT 中每一次高频分配/销毁 20MB+ 内存的开销)
    g_hOverlayDoubleBufferDC = CreateCompatibleDC(hScreenDC);
    g_hOverlayDoubleBufferBmp = CreateCompatibleBitmap(hScreenDC, g_screenWidth, g_screenHeight);
    g_hOldOverlayDoubleBufferBmp = SelectObject(g_hOverlayDoubleBufferDC, g_hOverlayDoubleBufferBmp);
    
    // D. 预先将背景与遮罩位图选入各自的静态 DC
    g_hCaptureSourceDC = CreateCompatibleDC(hScreenDC);
    g_hOldCaptureSourceBmp = SelectObject(g_hCaptureSourceDC, g_hScreenCapture);
    
    g_hCaptureMaskedSourceDC = CreateCompatibleDC(hScreenDC);
    g_hOldCaptureMaskedSourceBmp = SelectObject(g_hCaptureMaskedSourceDC, g_hScreenCaptureMasked);
    
    ReleaseDC(NULL, hScreenDC);
    
    // 3. 展现 Overlay 无边框置顶覆盖层
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = OverlayWndProc;
    wcex.hInstance = g_hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
    wcex.lpszClassName = L"CaptureToolOverlayClass";
    RegisterClassEx(&wcex);
    
    g_overlay.selection = { 0, 0, 0, 0 };
    g_overlay.selectionDone = false;
    g_overlay.isSelecting = false;
    g_overlay.isResizing = false;
    g_overlay.hasDetectedWindow = false;
    g_overlay.isPossibleClick = false;
    g_isDrawingShape = false;
    
    g_hWndOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"CaptureToolOverlayClass",
        L"CaptureToolOverlay",
        WS_POPUP | WS_VISIBLE,
        g_screenX, g_screenY, g_screenWidth, g_screenHeight,
        NULL, NULL, g_hInstance, NULL
    );
    
    ShowWindow(g_hWndOverlay, SW_SHOW);
    UpdateWindow(g_hWndOverlay);
    SetForegroundWindow(g_hWndOverlay);
    SetTimer(g_hWndOverlay, 1, 500, NULL); // 500ms cursor blink timer
}

// ========== 托盘自启动右键菜单刷新函数 ==========
void UpdateTrayAutostartMenu(HMENU hMenu) {
    if (g_config.auto_start) {
        CheckMenuItem(hMenu, ID_TRAY_AUTOSTART, MF_CHECKED | MF_BYCOMMAND);
    } else {
        CheckMenuItem(hMenu, ID_TRAY_AUTOSTART, MF_UNCHECKED | MF_BYCOMMAND);
    }
}

// ========== 按钮扁平化子类化处理辅助 (Hover Invalidation) ==========
LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WNDPROC oldProc = (WNDPROC)GetProp(hWnd, L"OLDPROC");
    switch (message) {
        case WM_MOUSEMOVE: {
            if (!GetProp(hWnd, L"HOVERED")) {
                SetProp(hWnd, L"HOVERED", (HANDLE)1);
                TRACKMOUSEEVENT tme = { sizeof(tme) };
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            RemoveProp(hWnd, L"HOVERED");
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_DESTROY: {
            RemoveProp(hWnd, L"HOVERED");
            WNDPROC proc = (WNDPROC)RemoveProp(hWnd, L"OLDPROC");
            if (proc) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)proc);
            break;
        }
    }
    return CallWindowProc(oldProc ? oldProc : DefWindowProc, hWnd, message, wParam, lParam);
}

void MakeButtonFlat(HWND hBtn) {
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetProp(hBtn, L"OLDPROC", (HANDLE)oldProc);
}

// ========== 简易现代风格“设置对话框” WndProc 过程 ==========
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hCheckSuppress = NULL;
    static HWND hCheckAutostart = NULL;
    static HWND hBtnSave = NULL;
    static HWND hStaticText = NULL;
    static HWND hBtnRecord = NULL;
    
    static bool s_suppress = false;
    static bool s_autostart = false;
    
    switch (message) {
        case WM_CREATE: {
            s_suppress = g_config.hotkey.suppress;
            s_autostart = g_config.auto_start;
            
            // 背景与基本 UI 控件创建 (极简现代暗黑风，去除任何丑陋的 Win32 控件背景)
            HBRUSH hGray = CreateSolidBrush(RGB(30, 30, 30));
            SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)hGray);
            
            HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            HFONT hTextFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            // 标题 (无 Emoji，精致大方)
            HWND hTitle = CreateWindow(L"STATIC", L"CaptureTool 设置", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 424, 30, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            // 提示区
            g_recordedHotkey = g_config.hotkey; // 暂存热键
            wstring hotkeyText = L"当前快捷键: " + FormatHotkeyConfig(g_config.hotkey);
            hStaticText = CreateWindow(L"STATIC", hotkeyText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 65, 424, 25, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hStaticText, WM_SETFONT, (WPARAM)hTextFont, TRUE);
            
            // 录入新快捷键按钮 (Owner Draw，扁平风格，无 Emoji)
            hBtnRecord = CreateWindow(L"BUTTON", L"录入新快捷键", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 132, 105, 200, 38, hWnd, (HMENU)1002, g_hInstance, NULL);
            MakeButtonFlat(hBtnRecord);
            
            // 复选框 - 吞噬 (Owner Draw 绘制成极致扁平暗黑复选框)
            hCheckSuppress = CreateWindow(L"BUTTON", L"阻止其他应用接收此按键", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 165, 320, 28, hWnd, (HMENU)1003, g_hInstance, NULL);
            MakeButtonFlat(hCheckSuppress);
            
            // 复选框 - 自启动
            hCheckAutostart = CreateWindow(L"BUTTON", L"开机自启动", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 205, 320, 28, hWnd, (HMENU)1004, g_hInstance, NULL);
            MakeButtonFlat(hCheckAutostart);
            
            // 保存按钮 (Owner Draw，扁平风格，无 Emoji)
            hBtnSave = CreateWindow(L"BUTTON", L"保存设置", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 132, 265, 200, 44, hWnd, (HMENU)1001, g_hInstance, NULL);
            MakeButtonFlat(hBtnSave);
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(220, 220, 220)); // 优雅浅灰
            SetBkColor(hdc, RGB(30, 30, 30));      // 极致深灰
            static HBRUSH hBkBrush = CreateSolidBrush(RGB(30, 30, 30));
            return (INT_PTR)hBkBrush;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
            HDC hdc = pDIS->hDC;
            RECT r = pDIS->rcItem;
            UINT id = pDIS->CtlID;
            
            bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
            bool isHovered = GetProp(pDIS->hwndItem, L"HOVERED") != NULL;
            
            Graphics g(hdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            
            // 背景填充
            SolidBrush bgBrush(Color(255, 30, 30, 30));
            g.FillRectangle(&bgBrush, (int)r.left, (int)r.top, (int)(r.right - r.left), (int)(r.bottom - r.top));
            
            if (id == 1001 || id == 1002) { // 按钮绘制：保存按钮 (1001) 或 录入按钮 (1002)
                Color btnColor;
                if (id == 1001) { // 保存设置：现代高保真扁平绿
                    if (isPressed) btnColor = Color(255, 30, 130, 70);
                    else if (isHovered) btnColor = Color(255, 46, 204, 113);
                    else btnColor = Color(255, 39, 174, 96);
                } else { // 录入快捷键：现代扁平深灰/蓝
                    if (isPressed) btnColor = Color(255, 30, 40, 50);
                    else if (isHovered) btnColor = Color(255, 52, 152, 219);
                    else btnColor = Color(255, 52, 73, 94);
                }
                
                // 绘制完全方正无导角的精致扁平按钮
                SolidBrush bBrush(btnColor);
                g.FillRectangle(&bBrush, (int)r.left, (int)r.top, (int)(r.right - r.left), (int)(r.bottom - r.top));
                
                // 绘制按钮文字
                wchar_t text[128];
                GetWindowText(pDIS->hwndItem, text, 128);
                
                Font font(L"Microsoft YaHei", (id == 1001) ? 10 : 9, FontStyleBold);
                StringFormat format;
                format.SetAlignment(StringAlignmentCenter);
                format.SetLineAlignment(StringAlignmentCenter);
                SolidBrush textBrush(Color(255, 255, 255, 255));
                g.DrawString(text, -1, &font, RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top)), &format, &textBrush);
            }
            else if (id == 1003 || id == 1004) { // 复选框绘制：阻止接收 (1003) 或 开机自启 (1004)
                bool isChecked = (id == 1003) ? s_suppress : s_autostart;
                
                // 绘制自制方正精致暗黑复选框
                int boxSize = 16;
                int boxX = r.left;
                int boxY = r.top + (r.bottom - r.top - boxSize) / 2;
                
                if (isChecked) {
                    SolidBrush checkedBrush(Color(255, 0, 174, 255)); // 炫酷品牌蓝 #00AEFF
                    g.FillRectangle(&checkedBrush, boxX, boxY, boxSize, boxSize);
                    
                    // 绘制纯白高清对号
                    Pen whitePen(Color(255, 255, 255, 255), 2.0f);
                    g.DrawLine(&whitePen, boxX + 4, boxY + 8, boxX + 7, boxY + 11);
                    g.DrawLine(&whitePen, boxX + 7, boxY + 11, boxX + 12, boxY + 4);
                } else {
                    Pen borderPen(isHovered ? Color(255, 180, 180, 180) : Color(255, 100, 100, 100), 1.5f);
                    g.DrawRectangle(&borderPen, boxX, boxY, boxSize, boxSize);
                }
                
                // 绘制复选框标签文本
                wchar_t text[128];
                GetWindowText(pDIS->hwndItem, text, 128);
                
                Font font(L"Microsoft YaHei", 9, FontStyleRegular);
                StringFormat format;
                format.SetAlignment(StringAlignmentNear);
                format.SetLineAlignment(StringAlignmentCenter);
                SolidBrush textBrush(isHovered ? Color(255, 255, 255, 255) : Color(255, 200, 200, 200));
                
                int textX = boxX + boxSize + 10;
                g.DrawString(text, -1, &font, RectF((REAL)textX, (REAL)r.top, (REAL)(r.right - textX), (REAL)(r.bottom - r.top)), &format, &textBrush);
            }
            return TRUE;
        }
        case WM_HOTKEY_RECORDED: {
            if (!g_isRecordingHotkey) break;
            
            if (message == WM_HOTKEY_RECORDED) { // 冗余安全检查
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    DWORD vk = (DWORD)lParam;
                    
                    // 按 Esc 退出录入模式且不保存
                    if (vk == VK_ESCAPE) {
                        g_isRecordingHotkey = false;
                        wstring hotkeyText = L"当前快捷键: " + FormatHotkeyConfig(g_recordedHotkey);
                        SetWindowText(hStaticText, hotkeyText.c_str());
                        SetWindowText(hBtnRecord, L"录入新快捷键");
                        EnableWindow(hBtnRecord, TRUE);
                        break;
                    }
                    
                    if (IsModifierVk(vk)) {
                        // 如果是修饰键，更新静态文本，提示当前录入修饰状态
                        wstring modStr = L"";
                        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) modStr += L"Ctrl + ";
                        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) modStr += L"Shift + ";
                        if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) modStr += L"Alt + ";
                        if (modStr.empty()) modStr = L"... + ";
                        SetWindowText(hStaticText, (L"当前录入: " + modStr).c_str());
                    } else {
                        // 非修饰主键，结束录入
                        vector<wstring> keys;
                        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) keys.push_back(L"ctrl");
                        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) keys.push_back(L"shift");
                        if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) keys.push_back(L"alt");
                        
                        wstring keyName = VkToCharName(vk);
                        if (!keyName.empty()) {
                            keys.push_back(keyName);
                            g_recordedHotkey.type = L"keyboard";
                            g_recordedHotkey.keys = keys;
                            g_recordedHotkey.mouse_button = L"";
                            
                            g_isRecordingHotkey = false;
                            wstring resultStr = L"已录入新快捷键: " + FormatHotkeyConfig(g_recordedHotkey);
                            SetWindowText(hStaticText, resultStr.c_str());
                            SetWindowText(hBtnRecord, L"录入新快捷键");
                            EnableWindow(hBtnRecord, TRUE);
                        }
                    }
                } else if (wParam == WM_XBUTTONDOWN) {
                    int xbutton = (int)lParam;
                    
                    vector<wstring> keys;
                    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) keys.push_back(L"ctrl");
                    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) keys.push_back(L"shift");
                    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) keys.push_back(L"alt");
                    
                    g_recordedHotkey.type = L"mouse";
                    g_recordedHotkey.keys = keys;
                    g_recordedHotkey.mouse_button = (xbutton == 1) ? L"x1" : L"x2";
                    
                    g_isRecordingHotkey = false;
                    wstring resultStr = L"已录入新快捷键: " + FormatHotkeyConfig(g_recordedHotkey);
                    SetWindowText(hStaticText, resultStr.c_str());
                    SetWindowText(hBtnRecord, L"录入新快捷键");
                    EnableWindow(hBtnRecord, TRUE);
                }
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1001) {
                // 保存设置
                g_config.hotkey = g_recordedHotkey;
                g_config.hotkey.suppress = s_suppress;
                g_config.auto_start = s_autostart;
                
                SaveConfig();
                ApplyAutostart();
                
                // 重启 hook 生效新热键
                StopHooks();
                StartHooks();
                
                MessageBox(hWnd, L"设置已成功保存", L"CaptureTool", MB_OK | MB_ICONINFORMATION);
                SendMessage(hWnd, WM_CLOSE, 0, 0);
            }
            else if (LOWORD(wParam) == 1002) {
                // 开启录入状态
                g_isRecordingHotkey = true;
                SetWindowText(hStaticText, L"请按下快捷键或鼠标侧键... (Esc 取消)");
                SetWindowText(hBtnRecord, L"监听中...");
                EnableWindow(hBtnRecord, FALSE);
            }
            else if (LOWORD(wParam) == 1003) {
                s_suppress = !s_suppress;
                InvalidateRect(hCheckSuppress, NULL, FALSE);
            }
            else if (LOWORD(wParam) == 1004) {
                s_autostart = !s_autostart;
                InvalidateRect(hCheckAutostart, NULL, FALSE);
            }
            break;
        }
        case WM_CLOSE: {
            g_isRecordingHotkey = false; // 强置关闭
            DestroyWindow(hWnd);
            g_hWndSettings = NULL;
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ========== 主运行控制核心 WndProc 窗口过程 ==========
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TRIGGER_CAPTURE: {
            TriggerCapture();
            break;
        }
        case WM_TRAY_MSG: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_CAPTURE, L"区域截图");
                AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置中心");
                AppendMenu(hMenu, MF_STRING, ID_TRAY_AUTOSTART, L"开机自启动");
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_QUIT, L"退出工具");
                
                UpdateTrayAutostartMenu(hMenu);
                
                // 保证托盘右键菜单能正常失焦关闭
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);
            } 
            else if (lParam == WM_LBUTTONDBLCLK) {
                TriggerCapture();
            }
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_TRAY_CAPTURE:
                    TriggerCapture();
                    break;
                case ID_TRAY_SETTINGS: {
                    if (g_hWndSettings) {
                        SetForegroundWindow(g_hWndSettings);
                        break;
                    }
                    WNDCLASSEX wcexS = { 0 };
                    wcexS.cbSize = sizeof(WNDCLASSEX);
                    wcexS.lpfnWndProc = SettingsWndProc;
                    wcexS.hInstance = g_hInstance;
                    wcexS.hCursor = LoadCursor(NULL, IDC_ARROW);
                    wcexS.lpszClassName = L"CaptureToolSettingsClass";
                    RegisterClassEx(&wcexS);
                    
                    int sw = GetSystemMetrics(SM_CXSCREEN);
                    int sh = GetSystemMetrics(SM_CYSCREEN);
                    int wWidth = 480;
                    int wHeight = 380;
                    int wx = (sw - wWidth) / 2;
                    int wy = (sh - wHeight) / 2;
                    
                    g_hWndSettings = CreateWindowEx(
                        WS_EX_TOPMOST,
                        L"CaptureToolSettingsClass",
                        L"CaptureTool 设置",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                        wx, wy, wWidth, wHeight,
                        NULL, NULL, g_hInstance, NULL
                    );
                    
                    ShowWindow(g_hWndSettings, SW_SHOW);
                    UpdateWindow(g_hWndSettings);
                    break;
                }
                case ID_TRAY_AUTOSTART:
                    g_config.auto_start = !g_config.auto_start;
                    SaveConfig();
                    ApplyAutostart();
                    break;
                case ID_TRAY_QUIT:
                    SendMessage(hWnd, WM_DESTROY, 0, 0);
                    break;
            }
            break;
        }
        case WM_DESTROY: {
            // 卸载托盘图标
            NOTIFYICONDATA nid = { sizeof(nid) };
            nid.hWnd = hWnd;
            nid.uID = TRAY_ICON_ID;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            
            StopHooks();
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ========== 程序 WinMain 主入口函数 ==========
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    
    // 1. 初始化 DPI 适配层
    InitDpiAwareness();
    
    // 2. 单实例检测，防止多实例重复注册底层 Hook 锁死系统
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"CaptureTool_Mutex_Lock_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, L"CaptureTool 已在运行中！\n请检查右下角系统托盘区域。", L"CaptureTool", MB_OK | MB_ICONWARNING);
        return 0;
    }
    
    // 3. 加载配置文件，同步注册表启动项
    LoadConfig();
    ApplyAutostart();
    
    // 4. 初始化 GDI+ 环境
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    
    // 5. 注册并创建主窗口 (作为隐藏的窗口处理消息泵)
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"CaptureToolMainClass";
    RegisterClassEx(&wcex);
    
    g_hWndMain = CreateWindowEx(
        0, L"CaptureToolMainClass", L"CaptureToolMain",
        0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL
    );
    
    if (!g_hWndMain) {
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(hMutex);
        return 0;
    }
    
    // 6. 添加系统托盘图标
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = g_hWndMain;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_MSG;
    nid.hIcon = CreateProgrammaticIcon();
    wcscpy_s(nid.szTip, L"CaptureTool - 截图工具");
    Shell_NotifyIcon(NIM_ADD, &nid);
    DestroyIcon(nid.hIcon); // 创建完已复制，可安全释放句柄
    
    // 7. 安装全局底层热键与侧键 Hook
    StartHooks();
    
    // 8. 消息泵循环运行
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 9. 清除环境并退出
    GdiplusShutdown(g_gdiplusToken);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
