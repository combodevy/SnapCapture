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
#include <utility>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <dwmapi.h>
#include <commdlg.h>
#include <imm.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "imm32.lib")

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

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
#define ID_TRAY_RELOADHOOKS 2005

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
    Color custom_colors[4] = {
        Color(255, 231, 76, 60),      // 红
        Color(255, 52, 152, 219),     // 蓝
        Color(255, 46, 204, 113),     // 绿
        Color(255, 155, 89, 182)      // 紫
    };
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
    ANNOTATION_CIRCLE,
    ANNOTATION_PENCIL,
    ANNOTATION_MOSAIC,
    ANNOTATION_TEXT
};

enum ShapeType {
    SHAPE_ARROW = 0,
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_PENCIL,
    SHAPE_MOSAIC,
    SHAPE_MOSAIC_RECT,
    SHAPE_TEXT
};

struct DrawingShape {
    ShapeType type;
    POINT start;
    POINT end;
    Color color;
    int thickness;
    wstring text;
    float angle = 0.0f;
    float scale = 1.0f;
    float origW = 0.0f;
    float origH = 0.0f;
    float origX = 0.0f;
    float origY = 0.0f;
    int fontSize = 24;
    wstring fontFamily = L"Microsoft YaHei";
    int fontStyle = 0; // 字体样式: Regular=0, Bold=1, Italic=2, BoldItalic=3
    int roundRadius = 0; // 圆角半径: 0 为直角，大于 0 为圆角
    vector<POINT> points; // 画笔自由手绘的点集
};

AnnotationMode g_annotationMode = ANNOTATION_NONE;
vector<DrawingShape> g_shapes;
vector<DrawingShape> g_redoStack; // 撤销后的标注暂存，用于重做
bool g_isDrawingShape = false;
DrawingShape g_tempShape;

// 当前属性选择状态
Color g_currentColor = Color(255, 231, 76, 60); // 默认红色
int g_currentThickness = 4; // 默认中号 4px
int g_activeCustomSlotIndex = 0; // 当前选中的自定义色槽索引 (0-3)
int g_currentRoundRadius = 0; // 当前默认圆角半径 (0-30)

// 马赛克工具状态
const int MOSAIC_BRUSH_MIN = 6, MOSAIC_BRUSH_MAX = 150;
const int MOSAIC_BLOCK_MIN = 4, MOSAIC_BLOCK_MAX = 40;
const int MOSAIC_REGEN_INTERVAL_MS = 200; // 拖动程度滑条时图层重建的节流间隔
const int TOOLBAR_ANIM_MS = 160;          // 工具栏滑入动画时长
int g_mosaicBlock = 12;    // 程度：色块边长 px，无级可调，持久化到 config
int g_mosaicBrush = 24;    // 涂抹模式笔刷直径 px，无级可调，持久化到 config
int g_mosaicShapeMode = 0; // 0=涂抹 1=矩形
int g_mosaicStyle = 0;     // 0=像素化 1=模糊，持久化到 config
int g_mosaicSliderDrag = 0;      // 0=无 1=正在拖粗细滑条 2=正在拖程度滑条
int g_sliderTrackX = 0;          // 拖动中滑条的轨道起点（按下时缓存，移动中免重算布局）
bool g_showMosaicCursor = false; // 涂抹马赛克时隐藏十字，改画笔刷覆盖圈
POINT g_mosaicCursorPt = { 0, 0 };
bool g_circleOnBuffer = false;   // 双缓冲中当前是否留有笔刷圈（用于擦除）
RECT g_circleLastRect = { 0, 0, 0, 0 };  // 缓冲中圈所占矩形
RECT g_paintRect = { 0, 0, 0, 0 };       // 本帧需要重绘的矩形（局部合成）
RECT g_sliderStripRect = { 0, 0, 0, 0 }; // 拖动中的滑条区域（泵式局部重绘用）
RECT g_longViewRect = { 0, 0, 0, 0 };    // 长图在覆盖层中的框内显示区域
Bitmap* g_mosaicBitmap = NULL;     // 惰性生成的马赛克图层（与冻结截图同尺寸、屏幕对齐）
TextureBrush* g_mosaicTexBrush = NULL; // 绑定图层的纹理画刷：涂抹/矩形直接按屏幕坐标取样填充
static int s_mosaicGenBlock = -1;  // 生成图层时使用的程度
static int s_mosaicGenStyle = -1;  // 生成图层时使用的风格
static DWORD s_mosaicLastRegenTick = 0; // 上次图层重建时刻（节流用）
DWORD g_toolbarAnimStart = 0;      // 工具栏滑入动画起始 tick（0=未在动画中）

// 长截屏（滚动截屏）状态
bool g_longModeActive = false;     // 正在滚动捕获（覆盖层隐藏，悬浮面板工作）
bool g_longPreviewActive = false;  // 捕获完成，长图已放进截图框内预览
HWND g_hWndLongPanel = NULL;       // 悬浮控制面板
RECT g_longRegion = { 0 };         // 屏幕坐标的捕获区域
RECT g_longRegionLocal = { 0 };    // 覆盖层坐标选区（进入时保存）
int g_longRegionWidth = 0, g_longRegionHeight = 0;
std::vector<Gdiplus::Bitmap*> g_longStrips;   // 拼接条带池（每条分配高度固定 LONG_STRIP_H）
std::vector<int> g_longStripHeights;          // 各条带已填充行数（末条可不满）
int g_longTotalH = 0;              // 已拼接总高
const int LONG_STRIP_H = 256;      // 条带池单条分配高度
int g_longStallCount = 0;          // 连续无新增计数（到底提示）
Gdiplus::Bitmap* g_longPrevCap = NULL; // 上一轮捕获（中继路径的对齐参考）
bool g_longPrevValid = false;      // 画布尾部是否与 g_longPrevCap 逐行对齐
int g_longDiagExt = -1, g_longDiagRun = -1, g_longDiagD = 0, g_longDiagApp = -1, g_longDiagPath = 0;
int g_longDiagSum = -1;
int g_longPollMs = 120;            // 当前轮询周期（中继续拼时自适应提速）
int g_longPanelW = 0, g_longPanelH = 0, g_longPreviewH = 0;
float g_longPanelScale = -1.0f;    // 面板预览当前缩放（动画插值用，-1=未初始化）
Gdiplus::Bitmap* g_longFinalBmp = NULL;  // 全分辨率成品（输出用）
Gdiplus::Bitmap* g_longViewBmp = NULL;   // 框内预览用的缩放版本
HBITMAP g_longFinalHbmp = NULL;    // 成品的 HBITMAP 视图（剪贴板/保存用）
int g_longFinalW = 0, g_longFinalH = 0; // 成品尺寸（条带释放后标签/缩放仍需使用）

// 新增：高保真交互控制状态
bool g_isHoveringRoundRadius = false;
wstring g_editingRoundRadiusStr = L"";
int g_currentFontStyle = 0; // 字体样式预设
int g_editingCaretIndex = 0; // 正在编辑文字的光标索引

// 输入法 (IME) 组合输入状态：用于内联显示拼音组合串与定位候选窗口
bool g_imeComposing = false;
wstring g_imeComposition = L"";
int g_imeCaretInComposition = 0;
float g_editCaretScreenX = 0.0f; // 当前光标在屏幕坐标系中的位置
float g_editCaretScreenY = 0.0f;

// 文字标注状态
bool g_isEditingText = false;
wstring g_editingText = L"";
POINT g_editTextPos = { 0, 0 };
int g_draggingTextIndex = -1;
POINT g_textDragOffset = { 0, 0 };
bool g_isDraggingText = false;
int g_selectedTextIndex = -1;
float g_editAngle = 0.0f;
float g_editScale = 1.0f;
int g_currentFontSize = 24;
wstring g_currentFontFamily = L"Microsoft YaHei";

bool g_isRotatingText = false;
int g_rotatingTextIndex = -1;
float g_initialMouseAngle = 0.0f;
float g_initialTextAngle = 0.0f;

bool g_isResizingText = false;
int g_resizingTextIndex = -1;
float g_initialMouseDist = 0.0f;
float g_initialTextScale = 1.0f;

// 新增：文本拖拽、缩放、颜色 Dialog 全局状态
bool g_isDraggingEditingText = false;
POINT g_editingTextDragOffset = { 0, 0 };
int g_resizeTextHandle = 0; // 0: TL, 1: TR, 2: BL
float g_fixedOppositeCornerX = 0.0f;
float g_fixedOppositeCornerY = 0.0f;
float g_initialScreenDirX = 0.0f;
float g_initialScreenDirY = 0.0f;
float g_diagonalLocalLength = 0.0f;

COLORREF g_customDialogColors[16] = {
    RGB(255, 255, 255), RGB(240, 240, 240), RGB(220, 220, 220), RGB(200, 200, 200),
    RGB(180, 180, 180), RGB(150, 150, 150), RGB(120, 120, 120), RGB(100, 100, 100),
    RGB(80, 80, 80),    RGB(60, 60, 60),    RGB(40, 40, 40),    RGB(20, 20, 20),
    RGB(0, 0, 0),       RGB(231, 76, 60),   RGB(52, 152, 219),  RGB(46, 204, 113)
};

// 后置声明 SaveConfig
void SaveConfig();

bool PromptForColor(HWND hWnd, Color& outColor) {
    CHOOSECOLOR cc = { 0 };
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hWnd;
    cc.lpCustColors = g_customDialogColors;
    cc.rgbResult = RGB(outColor.GetR(), outColor.GetG(), outColor.GetB());
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    
    if (ChooseColor(&cc) == TRUE) {
        outColor = Color(255, GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
        SaveConfig(); // 选择颜色后立即保存
        return true;
    }
    return false;
}

bool PromptForFont(HWND hWnd, wstring& outFamily, int& outSize, int& outStyle) {
    CHOOSEFONT cf = { 0 };
    LOGFONT lf = { 0 };
    HDC hdc = GetDC(hWnd);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hWnd, hdc);
    
    lf.lfHeight = -MulDiv(outSize, dpiY, 72);
    wcsncpy_s(lf.lfFaceName, LF_FACESIZE, outFamily.c_str(), _TRUNCATE);
    lf.lfCharSet = DEFAULT_CHARSET;
    
    // 填充以前保存的字形样式（加粗、斜体）
    lf.lfWeight = (outStyle & FontStyleBold) ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = (outStyle & FontStyleItalic) ? TRUE : FALSE;
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hWnd;
    cf.lpLogFont = &lf;
    cf.iPointSize = outSize * 10;
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
    
    if (ChooseFont(&cf) == TRUE) {
        outFamily = lf.lfFaceName;
        // 使用 ChooseFont 直接返回 of iPointSize，比 lfHeight 逆运算可靠得多
        outSize = cf.iPointSize / 10;
        if (outSize <= 0) outSize = 12;
        
        // 转换回 GDI+ 字体样式
        outStyle = FontStyleRegular;
        if (lf.lfWeight >= FW_BOLD) outStyle |= FontStyleBold;
        if (lf.lfItalic) outStyle |= FontStyleItalic;
        
        return true;
    }
    return false;
}

// 高可靠性 GDI+ 字体构造器：字体无效时依次降级，避免 0 尺寸或崩溃。
// 另外 GDI+ 每次 new Font 都要做字体匹配，而绘制是每帧进行的，
// 因此按 (字体名, 字号, 样式) 缓存实例，避免重复构造。
static vector<pair<wstring, Font*>> g_fontCache;

static wstring MakeFontCacheKey(const wchar_t* family, REAL size, FontStyle style) {
    return wstring(family) + L"|" + to_wstring((int)(size * 100.0f)) + L"|" + to_wstring((int)style);
}

Font* CreateSafeGdiplusFont(const wchar_t* family, REAL size, FontStyle style = FontStyleRegular) {
    wstring key = MakeFontCacheKey(family, size, style);
    for (size_t i = 0; i < g_fontCache.size(); ++i) {
        if (g_fontCache[i].first == key) return g_fontCache[i].second;
    }

    Font* font = new Font(family, size, style);
    if (font && font->GetLastStatus() == Ok) {
        g_fontCache.push_back(pair<wstring, Font*>(key, font));
        return font;
    }
    if (font) delete font;

    // 降级尝试1: 微软雅黑
    font = new Font(L"Microsoft YaHei", size, style);
    if (font && font->GetLastStatus() == Ok) {
        g_fontCache.push_back(pair<wstring, Font*>(key, font));
        return font;
    }
    if (font) delete font;

    // 降级尝试2: Arial
    font = new Font(L"Arial", size, style);
    if (font && font->GetLastStatus() == Ok) {
        g_fontCache.push_back(pair<wstring, Font*>(key, font));
        return font;
    }
    if (font) delete font;

    // 降级尝试3: 系统默认无衬线字体
    font = new Font(FontFamily::GenericSansSerif(), size, style);
    g_fontCache.push_back(pair<wstring, Font*>(key, font));
    return font;
}

// 确保文字标注带有测量缓存与有效包围盒。
// start==end 的退化框（刚提交、尚未绘制过）按 start 为左上角锚定补全，与编辑态显示位置一致；
// 已有包围盒但缓存被清（如改字号后）则保持中心不动、按缩放比例重建，避免旋转/缩放过的文字跳位。
// 覆盖层绘制、命中测试与最终裁剪输出必须共用此逻辑，否则两边对同一 shape 会算出不同的中心。
static void EnsureTextShapeMeasured(Graphics& g, DrawingShape& shp) {
    if (shp.type != SHAPE_TEXT || shp.text.empty() || shp.origW != 0.0f) return;

    Font* pFont = CreateSafeGdiplusFont(shp.fontFamily.c_str(), (REAL)shp.fontSize, (FontStyle)shp.fontStyle);
    RectF bounds;
    g.MeasureString(shp.text.c_str(), -1, pFont, PointF(0, 0), &bounds);
    shp.origW = bounds.Width;
    shp.origH = bounds.Height;
    shp.origX = bounds.X;
    shp.origY = bounds.Y;

    if (shp.end.x == shp.start.x && shp.end.y == shp.start.y) {
        shp.end.x = shp.start.x + (int)bounds.Width;
        shp.end.y = shp.start.y + (int)bounds.Height;
    } else {
        float cx = (shp.start.x + shp.end.x) / 2.0f;
        float cy = (shp.start.y + shp.end.y) / 2.0f;
        shp.start.x = (int)(cx - (bounds.Width * shp.scale) / 2.0f);
        shp.start.y = (int)(cy - (bounds.Height * shp.scale) / 2.0f);
        shp.end.x = (int)(cx + (bounds.Width * shp.scale) / 2.0f);
        shp.end.y = (int)(cy + (bounds.Height * shp.scale) / 2.0f);
    }
}

// 新增：判定鼠标点是否在当前编辑状态的文本包围盒内
bool IsPointInEditingText(HWND hWnd, POINT pt, float& outW, float& outH) {
    if (!g_isEditingText) return false;
    
    HDC hdc = GetDC(hWnd);
    Graphics g(hdc);
    
    Font* pFont = CreateSafeGdiplusFont(g_currentFontFamily.c_str(), (REAL)g_currentFontSize, (FontStyle)g_currentFontStyle);
    wstring text = g_editingText;
    if (text.empty()) text = L" ";
    
    RectF bounds;
    g.MeasureString(text.c_str(), -1, pFont, PointF(0, 0), &bounds);
    outW = bounds.Width;
    outH = bounds.Height;
    
    ReleaseDC(hWnd, hdc);
    
    float cx = g_editTextPos.x + outW / 2.0f;
    float cy = g_editTextPos.y + outH / 2.0f;
    
    float dx = pt.x - cx;
    float dy = pt.y - cy;
    float rad = -g_editAngle * 3.14159265f / 180.0f;
    float localX = (dx * cos(rad) - dy * sin(rad)) / g_editScale;
    float localY = (dx * sin(rad) + dy * cos(rad)) / g_editScale;
    
    return (localX >= -outW/2.0f - 10.0f && localX <= outW/2.0f + 10.0f &&
            localY >= -outH/2.0f - 8.0f && localY <= outH/2.0f + 8.0f);
}

// 新增：利用 GDI+ GraphicsPath 绘制高精平滑圆角矩形
void DrawRoundedRectangle(Graphics& g, Pen* pen, REAL x, REAL y, REAL width, REAL height, REAL radius) {
    if (radius <= 0.0f) {
        g.DrawRectangle(pen, x, y, width, height);
        return;
    }
    
    // 约束最大圆角半径不应超过宽高的一半
    REAL maxRadius = min(width, height) / 2.0f;
    if (radius > maxRadius) radius = maxRadius;
    
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90); // 左上
    path.AddArc(x + width - radius * 2, y, radius * 2, radius * 2, 270, 90); // 右上
    path.AddArc(x + width - radius * 2, y + height - radius * 2, radius * 2, radius * 2, 0, 90); // 右下
    path.AddArc(x, y + height - radius * 2, radius * 2, radius * 2, 90, 90); // 左下
    path.CloseFigure();
    
    g.DrawPath(pen, &path);
}

// 新增：利用 GDI+ GraphicsPath 填充高精平滑圆角矩形
void FillRoundedRectangle(Graphics& g, Brush* brush, REAL x, REAL y, REAL width, REAL height, REAL radius) {
    if (radius <= 0.0f) {
        g.FillRectangle(brush, x, y, width, height);
        return;
    }
    
    // 约束最大圆角半径不应超过宽高的一半
    REAL maxRadius = min(width, height) / 2.0f;
    if (radius > maxRadius) radius = maxRadius;
    
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90); // 左上
    path.AddArc(x + width - radius * 2, y, radius * 2, radius * 2, 270, 90); // 右上
    path.AddArc(x + width - radius * 2, y + height - radius * 2, radius * 2, radius * 2, 0, 90); // 右下
    path.AddArc(x, y + height - radius * 2, radius * 2, radius * 2, 90, 90); // 左下
    path.CloseFigure();

    g.FillPath(brush, &path);
}

// ========== 马赛克工具：子工具栏切换钮 + 预生成图层 + 绘制 ==========

wstring ToWString(int val); // 定义在下方"兼容性 wstring 转换函数"一节

// 笔刷覆盖圈所占的屏幕矩形（含描边与抗锯齿余量），用于局部失效。
// 余量必须与绘制笔迹时的胶囊余量一致（见涂抹分支），否则快速拖动会留下弧形残迹
static RECT MosaicCursorRect(POINT pt) {
    int r = g_mosaicBrush / 2 + 6;
    RECT rc = { pt.x - r, pt.y - r, pt.x + r, pt.y + r };
    return rc;
}

// 在属性子工具栏上绘制一枚小切换钮（激活态品牌蓝，非激活态深灰）
static void DrawSubToolbarChip(Graphics& g, REAL x, REAL y, REAL w, REAL h, bool active, const wchar_t* text) {
    SolidBrush bgBrush(active ? Color(255, 0, 174, 255) : Color(255, 60, 60, 60));
    FillRoundedRectangle(g, &bgBrush, x, y, w, h, 4.0f);

    Font* font = CreateSafeGdiplusFont(L"Microsoft YaHei", 8, FontStyleRegular);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    g.DrawString(text, -1, font, RectF(x, y, w, h), &sf, &textBrush);
}

// 把滑条上的横坐标换算成区间内的整数值（无级调节）
static int SliderValueFromX(int x, int trackX, int trackW, int minv, int maxv) {
    if (trackW <= 0) return minv;
    double t = (double)(x - trackX) / (double)trackW;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int v = minv + (int)(t * (maxv - minv) + 0.5);
    if (v < minv) v = minv;
    if (v > maxv) v = maxv;
    return v;
}

// 在属性子工具栏上绘制一枚小型滑条：灰槽 + 品牌蓝填充 + 圆形滑块 + 右侧数值
static void DrawSubToolbarSlider(Graphics& g, REAL labelX, REAL trackX, REAL trackW, REAL cy,
                                 const wchar_t* label, int value, int minv, int maxv, bool active) {
    Font* font = CreateSafeGdiplusFont(L"Microsoft YaHei", 8, FontStyleRegular);
    StringFormat sfLeft;
    sfLeft.SetAlignment(StringAlignmentNear);
    sfLeft.SetLineAlignment(StringAlignmentCenter);
    SolidBrush labelBrush(Color(255, 172, 172, 178));
    g.DrawString(label, -1, font, RectF(labelX, cy - 10.0f, 28.0f, 20.0f), &sfLeft, &labelBrush);

    SolidBrush trackBrush(Color(255, 72, 72, 78));
    FillRoundedRectangle(g, &trackBrush, trackX, cy - 2.0f, trackW, 4.0f, 2.0f);

    REAL t = (maxv > minv) ? (REAL)(value - minv) / (REAL)(maxv - minv) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    REAL fillW = trackW * t;
    if (fillW < 4.0f) fillW = 4.0f;
    SolidBrush fillBrush(Color(255, 0, 174, 255));
    FillRoundedRectangle(g, &fillBrush, trackX, cy - 2.0f, fillW, 4.0f, 2.0f);

    REAL thumbR = active ? 7.0f : 5.5f;
    REAL thumbX = trackX + fillW;
    SolidBrush thumbBrush(Color(255, 246, 246, 248));
    g.FillEllipse(&thumbBrush, thumbX - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    Pen thumbRing(Color(255, 0, 174, 255), 1.6f);
    g.DrawEllipse(&thumbRing, thumbX - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);

    Font* valueFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 8, FontStyleBold);
    SolidBrush valueBrush(Color(255, 238, 238, 242));
    wstring valueText = ToWString(value);
    g.DrawString(valueText.c_str(), -1, valueFont, RectF(trackX + trackW + 6.0f, cy - 10.0f, 26.0f, 20.0f), &sfLeft, &valueBrush);
}

// 确保马赛克图层存在且与当前程度/风格一致：把冻结截图缩小取样再放大，
// 像素模式用邻近插值出硬边色块，模糊模式用双线性。生成一次后整会话复用。
// force=false 时按 MOSAIC_REGEN_INTERVAL_MS 节流（拖动质量滑条期间保留旧图层平滑过渡），
// force=true 立即重建（工具激活、滑条按下/松开、风格切换）。
static void EnsureMosaicBitmap(bool force) {
    bool stale = (!g_mosaicBitmap) || (!g_mosaicTexBrush) ||
                 s_mosaicGenBlock != g_mosaicBlock || s_mosaicGenStyle != g_mosaicStyle;
    if (!stale) return;
    if (!force && g_mosaicBitmap) {
        DWORD now = GetTickCount();
        if (now - s_mosaicLastRegenTick < MOSAIC_REGEN_INTERVAL_MS) return;
    }
    s_mosaicLastRegenTick = GetTickCount();
    if (!g_hScreenCapture) return;

    Bitmap* fresh = NULL;
    Bitmap* src = Bitmap::FromHBITMAP(g_hScreenCapture, NULL);
    if (!src || src->GetLastStatus() != Ok) {
        delete src;
        return;
    }

    UINT W = src->GetWidth();
    UINT H = src->GetHeight();
    int block = max(4, g_mosaicBlock);
    int sw = (int)((W + block - 1) / block);
    int sh = (int)((H + block - 1) / block);
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;

    Bitmap* small = new Bitmap(sw, sh, PixelFormat32bppARGB);
    fresh = new Bitmap((INT)W, (INT)H, PixelFormat32bppARGB);
    if (!small || !fresh || small->GetLastStatus() != Ok || fresh->GetLastStatus() != Ok) {
        delete small;
        delete src;
        delete fresh;
        return;
    }

    {
        Graphics gs(small);
        gs.SetInterpolationMode(InterpolationModeBilinear);
        gs.SetPixelOffsetMode(PixelOffsetModeHalf);
        gs.DrawImage(src, Rect(0, 0, sw, sh), 0, 0, (INT)W, (INT)H, UnitPixel);
    }
    {
        Graphics gd(fresh);
        gd.SetInterpolationMode(g_mosaicStyle == 1 ? InterpolationModeBilinear : InterpolationModeNearestNeighbor);
        gd.SetPixelOffsetMode(PixelOffsetModeHalf);
        gd.DrawImage(small, Rect(0, 0, (INT)W, (INT)H), 0, 0, sw, sh, UnitPixel);
    }

    delete small;
    delete src;

    if (fresh->GetLastStatus() != Ok) {
        delete fresh;
        return;
    }

    // 只在成功后替换；节流跳过或生成失败期间保留旧图层继续显示
    // 画刷引用图层位图，必须先删画刷再删位图
    if (g_mosaicTexBrush) {
        delete g_mosaicTexBrush;
        g_mosaicTexBrush = NULL;
    }
    delete g_mosaicBitmap;
    g_mosaicBitmap = fresh;
    g_mosaicTexBrush = new TextureBrush(g_mosaicBitmap, WrapModeClamp);
    if (!g_mosaicTexBrush || g_mosaicTexBrush->GetLastStatus() != Ok) {
        delete g_mosaicTexBrush;
        g_mosaicTexBrush = NULL;
        delete g_mosaicBitmap;
        g_mosaicBitmap = NULL;
        s_mosaicGenBlock = -1;
        s_mosaicGenStyle = -1;
        return;
    }
    s_mosaicGenBlock = g_mosaicBlock;
    s_mosaicGenStyle = g_mosaicStyle;
}

// 把一个马赛克标注绘制到目标 Graphics 上。
// ox/oy 是绘制坐标系相对屏幕坐标的偏移：覆盖层为 (0,0)，裁剪输出为 (-x1,-y1)；
// 纹理画刷通过平移原点保证按屏幕坐标取样，预览与成图的色块完全一致。
// 涂抹用带纹理的圆头粗笔直接描折线：无 Widen/Region/裁剪，长笔迹也流畅不掉帧。
static void DrawMosaicShape(Graphics& g, const DrawingShape& shp, int ox, int oy) {
    EnsureMosaicBitmap(false);
    if (!g_mosaicBitmap || !g_mosaicTexBrush) return;

    // 纹理对齐：世界点 W 对应屏幕点 S = W - (ox,oy)，把纹理原点平移 (ox,oy) 后
    // 世界点 W 取样到的正是图层上的屏幕坐标 S
    g_mosaicTexBrush->ResetTransform();
    if (ox != 0 || oy != 0) {
        g_mosaicTexBrush->TranslateTransform((REAL)ox, (REAL)oy);
    }

    if (shp.type == SHAPE_MOSAIC) {
        if (shp.points.size() < 2) return;
        vector<PointF> pts(shp.points.size());
        for (size_t k = 0; k < shp.points.size(); ++k) {
            pts[k] = PointF((REAL)(shp.points[k].x + ox), (REAL)(shp.points[k].y + oy));
        }
        Pen texPen(g_mosaicTexBrush, (REAL)shp.thickness);
        texPen.SetStartCap(LineCapRound);
        texPen.SetEndCap(LineCapRound);
        texPen.SetLineJoin(LineJoinRound);
        g.DrawLines(&texPen, pts.data(), (INT)pts.size());
    } else {
        // 矩形：纹理填充（归一化，支持任意方向拖拽）
        int rx = min(shp.start.x, shp.end.x);
        int ry = min(shp.start.y, shp.end.y);
        int rw = abs(shp.end.x - shp.start.x);
        int rh = abs(shp.end.y - shp.start.y);
        if (rw > 0 && rh > 0) {
            g.FillRectangle(g_mosaicTexBrush, Rect(rx + ox, ry + oy, rw, rh));
        }
    }
    g_mosaicTexBrush->ResetTransform();
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
void ShowTrayNotification(const wstring& title, const wstring& message, DWORD infoFlags = NIIF_INFO);
bool IsWindowCaptureMode();
bool CommitEditingText();
void ResetEditingTextState();
void ResetOverlaySessionState();
bool SaveBitmapToConfiguredDirectory(HBITMAP hBmp, wstring& outPath);
bool FinalizeCaptureOutput(HWND hWnd, HBITMAP hBmp, bool showSaveDialog, wstring* outSavedPath = NULL);
HBITMAP CropScreenCapture(const RECT& sel);
void ClearRedoStack();
void UndoShape();
void RedoShape();
void DoCaptureConfirm(HWND hWnd);
void DoCaptureSaveAs(HWND hWnd);
void FreeLongStrips();

// ========== 兼容性 wstring 转换函数 ==========
wstring ToWString(int val) {
    wstringstream wss;
    wss << val;
    return wss.str();
}

wstring BuildTimestampedScreenshotName() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t filename[64];
    swprintf_s(filename, L"SnapCapture_%04d%02d%02d_%02d%02d%02d.png",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return filename;
}

wstring JoinPath(const wstring& dir, const wstring& name) {
    if (dir.empty()) return name;
    if (dir.back() == L'\\' || dir.back() == L'/') {
        return dir + name;
    }
    return dir + L"\\" + name;
}

void ShowTrayNotification(const wstring& title, const wstring& message, DWORD infoFlags) {
    if (!g_hWndMain) return;

    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = g_hWndMain;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = infoFlags;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

bool IsWindowCaptureMode() {
    return g_config.capture_mode == L"window";
}

void ResetEditingTextState() {
    g_isEditingText = false;
    g_editingText = L"";
    g_editingCaretIndex = 0;
    g_isDraggingEditingText = false;
    g_editingTextDragOffset = { 0, 0 };
    g_imeComposing = false;
    g_imeComposition = L"";
    g_imeCaretInComposition = 0;
}

// 读取输入法当前组合串与组合内光标位置（仅用于显示，最终上屏仍走 WM_CHAR）
void UpdateImeComposition(HWND hWnd, LPARAM lParam) {
    HIMC hIMC = ImmGetContext(hWnd);
    if (!hIMC) return;

    if (lParam & GCS_COMPSTR) {
        LONG bytes = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
        if (bytes > 0) {
            vector<wchar_t> buf((size_t)(bytes / sizeof(wchar_t)) + 2, L'\0');
            LONG copied = ImmGetCompositionString(hIMC, GCS_COMPSTR, buf.data(), bytes);
            if (copied > 0) {
                g_imeComposition.assign(buf.data(), (size_t)(copied / sizeof(wchar_t)));
            } else {
                g_imeComposition.clear();
            }
        } else {
            g_imeComposition.clear();
        }
    }

    if (lParam & GCS_CURSORPOS) {
        LONG pos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
        g_imeCaretInComposition = (pos > 0) ? (int)pos : 0;
        if (g_imeCaretInComposition > (int)g_imeComposition.length()) {
            g_imeCaretInComposition = (int)g_imeComposition.length();
        }
    }

    ImmReleaseContext(hWnd, hIMC);
}

// 把输入法候选/组合窗口挪到当前文字光标下方，避免它固定在屏幕左下角
void UpdateImeWindowPosition(HWND hWnd) {
    HIMC hIMC = ImmGetContext(hWnd);
    if (!hIMC) return;

    COMPOSITIONFORM cf;
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = (LONG)g_editCaretScreenX;
    cf.ptCurrentPos.y = (LONG)g_editCaretScreenY;
    ImmSetCompositionWindow(hIMC, &cf);
    ImmReleaseContext(hWnd, hIMC);
}

bool CommitEditingText() {
    if (!g_isEditingText || g_editingText.empty()) {
        ResetEditingTextState();
        return false;
    }

    DrawingShape textShape;
    textShape.type = SHAPE_TEXT;
    textShape.start = g_editTextPos;
    textShape.end = g_editTextPos;
    textShape.color = g_currentColor;
    textShape.thickness = g_currentThickness;
    textShape.text = g_editingText;
    textShape.angle = g_editAngle;
    textShape.scale = g_editScale;
    textShape.fontSize = g_currentFontSize;
    textShape.fontFamily = g_currentFontFamily;
    textShape.fontStyle = g_currentFontStyle;
    textShape.origW = 0.0f;
    textShape.origH = 0.0f;

    // 提交时立即补全测量缓存与包围盒：点确定/保存会在覆盖层重绘前直接裁剪输出，
    // 若此刻仍是 start==end 的退化框，成图会把整段文字画在锚点中心上，导致文字整体偏移。
    // 优先用覆盖层双缓冲 DC 测量，保证与预览渲染的测量结果完全一致。
    {
        HDC hBorrowedDC = NULL;
        HDC hMeasureDC = g_hOverlayDoubleBufferDC;
        if (!hMeasureDC) {
            hBorrowedDC = GetDC(NULL);
            hMeasureDC = hBorrowedDC;
        }
        {
            Graphics g(hMeasureDC);
            EnsureTextShapeMeasured(g, textShape);
        }
        if (hBorrowedDC) ReleaseDC(NULL, hBorrowedDC);
    }

    ClearRedoStack();
    g_shapes.push_back(textShape);

    ResetEditingTextState();
    return true;
}

bool SaveBitmapToConfiguredDirectory(HBITMAP hBmp, wstring& outPath) {
    if (!hBmp || g_config.save_directory.empty()) return false;

    int createResult = SHCreateDirectoryEx(NULL, g_config.save_directory.c_str(), NULL);
    if (createResult != ERROR_SUCCESS && createResult != ERROR_ALREADY_EXISTS && createResult != ERROR_FILE_EXISTS) {
        return false;
    }

    outPath = JoinPath(g_config.save_directory, BuildTimestampedScreenshotName());
    return SaveBitmapToFile(hBmp, outPath);
}

bool SaveBitmapWithDialog(HWND hWnd, HBITMAP hBmp, wstring& outPath) {
    if (!hBmp) return false;

    OPENFILENAME ofn = { 0 };
    wchar_t fileSz[MAX_PATH] = L"screenshot.png";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"PNG Image\0*.png\0";
    ofn.lpstrFile = fileSz;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"png";
    ofn.lpstrInitialDir = g_config.save_directory.empty() ? NULL : g_config.save_directory.c_str();

    if (!GetSaveFileName(&ofn)) return false;
    outPath = ofn.lpstrFile;
    return SaveBitmapToFile(hBmp, outPath);
}

bool FinalizeCaptureOutput(HWND hWnd, HBITMAP hBmp, bool showSaveDialog, wstring* outSavedPath) {
    if (!hBmp) return false;

    bool shouldCopy = g_config.save_to_clipboard || (!showSaveDialog && g_config.save_directory.empty());
    bool didCopy = false;
    bool didSave = false;
    wstring savedPath;

    if (shouldCopy) {
        didCopy = CopyBitmapToClipboard(hBmp);
    }

    if (showSaveDialog) {
        didSave = SaveBitmapWithDialog(hWnd, hBmp, savedPath);
    } else if (!g_config.save_directory.empty()) {
        didSave = SaveBitmapToConfiguredDirectory(hBmp, savedPath);
    }

    if (outSavedPath) {
        *outSavedPath = savedPath;
    }

    if (g_config.notification) {
        if (didCopy && didSave) {
            ShowTrayNotification(L"SnapCapture", L"截图已复制到剪贴板，并自动保存到目录。", NIIF_INFO);
        } else if (didSave) {
            ShowTrayNotification(L"SnapCapture", L"截图已保存。", NIIF_INFO);
        } else if (didCopy) {
            ShowTrayNotification(L"SnapCapture", L"截图已复制到剪贴板。", NIIF_INFO);
        } else {
            ShowTrayNotification(L"SnapCapture", L"截图输出失败，请检查保存目录或权限。", NIIF_WARNING);
        }
    }

    return didCopy || didSave;
}

// 释放长截屏的中间条带与对齐基准
void FreeLongStrips() {
    for (size_t i = 0; i < g_longStrips.size(); ++i) delete g_longStrips[i];
    g_longStrips.clear();
    g_longStripHeights.clear();
    if (g_longPrevCap) { delete g_longPrevCap; g_longPrevCap = NULL; }
    g_longPrevValid = false;
    g_longTotalH = 0;
    g_longStallCount = 0;
}

void ResetOverlaySessionState() {
    g_overlay.selectionDone = false;
    g_overlay.selection = { 0, 0, 0, 0 };
    g_overlay.hasDetectedWindow = false;
    g_overlay.isPossibleClick = false;
    g_overlay.isSelecting = false;
    g_overlay.isResizing = false;
    g_overlay.activeHandle = NONE_HANDLE;
    g_overlay.hoveredButton = 0;
    g_isDrawingShape = false;
    ResetEditingTextState();
    g_isDraggingText = false;
    g_draggingTextIndex = -1;
    g_selectedTextIndex = -1;
    g_isRotatingText = false;
    g_rotatingTextIndex = -1;
    g_isResizingText = false;
    g_resizingTextIndex = -1;
    g_isHoveringRoundRadius = false;
    g_editingRoundRadiusStr = L"";
    g_editAngle = 0.0f;
    g_editScale = 1.0f;
    g_currentFontSize = 24;
    g_currentFontFamily = L"Microsoft YaHei";
    g_currentFontStyle = 0;
    g_currentColor = Color(255, 231, 76, 60);
    g_currentThickness = 4;
    g_mosaicShapeMode = 0;
    g_mosaicSliderDrag = 0;
    g_showMosaicCursor = false;
    g_circleOnBuffer = false;
    g_toolbarAnimStart = 0;
    // 长截屏状态一并复位（面板/条带/成品全部释放）
    g_longModeActive = false;
    g_longPreviewActive = false;
    if (g_hWndLongPanel) { DestroyWindow(g_hWndLongPanel); g_hWndLongPanel = NULL; }
    FreeLongStrips();
    delete g_longFinalBmp; g_longFinalBmp = NULL;
    if (g_longFinalHbmp) { DeleteObject(g_longFinalHbmp); g_longFinalHbmp = NULL; }
    if (g_longViewBmp) { delete g_longViewBmp; g_longViewBmp = NULL; }
    g_longFinalW = 0;
    g_longFinalH = 0;
    g_shapes.clear();
    g_redoStack.clear();
    g_annotationMode = ANNOTATION_NONE;
}

// ========== 撤销 / 重做 ==========
void ClearRedoStack() {
    g_redoStack.clear();
}

void UndoShape() {
    if (!g_shapes.empty()) {
        g_redoStack.push_back(g_shapes.back());
        g_shapes.pop_back();
    }
}

void RedoShape() {
    if (!g_redoStack.empty()) {
        g_shapes.push_back(g_redoStack.back());
        g_redoStack.pop_back();
    }
}

// 确认输出：长图预览输出全分辨率成品；普通模式裁剪冻结画面，然后关闭覆盖层
void DoCaptureConfirm(HWND hWnd) {
    CommitEditingText();
    g_selectedTextIndex = -1;
    HBITMAP hBmp = NULL;
    if (g_longPreviewActive && g_longFinalHbmp) {
        hBmp = g_longFinalHbmp; // 长图成品归会话所有，不在此处释放
    } else {
        hBmp = CropScreenCapture(g_overlay.selection);
    }
    if (hBmp) {
        FinalizeCaptureOutput(hWnd, hBmp, false, NULL);
        if (hBmp != g_longFinalHbmp) DeleteObject(hBmp);
    }
    SendMessage(hWnd, WM_CLOSE, 0, 0);
}

// 另存为：弹出保存对话框，然后关闭覆盖层
void DoCaptureSaveAs(HWND hWnd) {
    CommitEditingText();
    g_selectedTextIndex = -1;
    HBITMAP hBmp = NULL;
    if (g_longPreviewActive && g_longFinalHbmp) {
        hBmp = g_longFinalHbmp; // 长图成品归会话所有，不在此处释放
    } else {
        hBmp = CropScreenCapture(g_overlay.selection);
    }
    if (hBmp) {
        FinalizeCaptureOutput(hWnd, hBmp, true, NULL);
        if (hBmp != g_longFinalHbmp) DeleteObject(hBmp);
    }
    SendMessage(hWnd, WM_CLOSE, 0, 0);
}

// ========== DPI 感知初始化 ==========
void InitDpiAwareness() {
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
        PFN_SetProcessDpiAwarenessContext pfnContext = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfnContext && pfnContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }

    HMODULE hShcore = LoadLibrary(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(int);
        PFN_SetProcessDpiAwareness pfn = (PFN_SetProcessDpiAwareness)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (pfn && SUCCEEDED(pfn(2))) {
            FreeLibrary(hShcore);
            return;
        }
        FreeLibrary(hShcore);
    }

    if (hUser32) {
        typedef BOOL(WINAPI* PFN_SetProcessDPIAware)();
        PFN_SetProcessDPIAware pfn = (PFN_SetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (pfn) pfn();
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
        else if (key == L"round_radius") g_currentRoundRadius = wcstol(val.c_str(), NULL, 10);
        else if (key == L"mosaic_block") {
            g_mosaicBlock = wcstol(val.c_str(), NULL, 10);
            if (g_mosaicBlock < MOSAIC_BLOCK_MIN) g_mosaicBlock = MOSAIC_BLOCK_MIN;
            if (g_mosaicBlock > MOSAIC_BLOCK_MAX) g_mosaicBlock = MOSAIC_BLOCK_MAX;
        }
        else if (key == L"mosaic_brush") {
            g_mosaicBrush = wcstol(val.c_str(), NULL, 10);
            if (g_mosaicBrush < MOSAIC_BRUSH_MIN) g_mosaicBrush = MOSAIC_BRUSH_MIN;
            if (g_mosaicBrush > MOSAIC_BRUSH_MAX) g_mosaicBrush = MOSAIC_BRUSH_MAX;
        }
        else if (key == L"mosaic_style") g_mosaicStyle = wcstol(val.c_str(), NULL, 10) ? 1 : 0;
        else if (key == L"custom_color_1") g_config.custom_colors[0] = Color((ARGB)wcstoul(val.c_str(), NULL, 10));
        else if (key == L"custom_color_2") g_config.custom_colors[1] = Color((ARGB)wcstoul(val.c_str(), NULL, 10));
        else if (key == L"custom_color_3") g_config.custom_colors[2] = Color((ARGB)wcstoul(val.c_str(), NULL, 10));
        else if (key == L"custom_color_4") g_config.custom_colors[3] = Color((ARGB)wcstoul(val.c_str(), NULL, 10));
        else if (key.find(L"dialog_color_") == 0) {
            int idx = wcstol(key.substr(13).c_str(), NULL, 10) - 1;
            if (idx >= 0 && idx < 16) {
                g_customDialogColors[idx] = (COLORREF)wcstoul(val.c_str(), NULL, 10);
            }
        }
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
    fwprintf(file, L"  \"custom_color_1\": %u,\n", g_config.custom_colors[0].GetValue());
    fwprintf(file, L"  \"custom_color_2\": %u,\n", g_config.custom_colors[1].GetValue());
    fwprintf(file, L"  \"custom_color_3\": %u,\n", g_config.custom_colors[2].GetValue());
    fwprintf(file, L"  \"custom_color_4\": %u,\n", g_config.custom_colors[3].GetValue());
    for (int i = 0; i < 16; ++i) {
        fwprintf(file, L"  \"dialog_color_%d\": %u,\n", i + 1, g_customDialogColors[i]);
    }
    fwprintf(file, L"  \"round_radius\": %d,\n", g_currentRoundRadius);
    fwprintf(file, L"  \"mosaic_block\": %d,\n", g_mosaicBlock);
    fwprintf(file, L"  \"mosaic_brush\": %d,\n", g_mosaicBrush);
    fwprintf(file, L"  \"mosaic_style\": %d,\n", g_mosaicStyle);
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
                // 侧键被鼠标驱动映射成"浏览器后退/前进"时，仍按鼠标侧键录入
                if (kbd->vkCode == VK_BROWSER_BACK) {
                    SendMessage(g_hWndSettings, WM_HOTKEY_RECORDED, WM_XBUTTONDOWN, (LPARAM)1);
                } else if (kbd->vkCode == VK_BROWSER_FORWARD) {
                    SendMessage(g_hWndSettings, WM_HOTKEY_RECORDED, WM_XBUTTONDOWN, (LPARAM)2);
                } else {
                    SendMessage(g_hWndSettings, WM_HOTKEY_RECORDED, wParam, kbd->vkCode);
                }
            }
            return 1; // Intercept all keyboard events during recording
        }
        // 部分鼠标驱动会把侧键映射成键盘的"浏览器后退/前进"而不是 XBUTTON 消息，
        // 这里折算回 X1 / X2，保证驱动走哪条路径都能触发截图。
        if (g_config.hotkey.type == L"mouse" && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            int sideButton = 0;
            if (kbd->vkCode == VK_BROWSER_BACK) sideButton = 1;
            else if (kbd->vkCode == VK_BROWSER_FORWARD) sideButton = 2;

            if (sideButton != 0) {
                wstring button_name = (sideButton == 1) ? L"x1" : L"x2";
                if (button_name == g_config.hotkey.mouse_button) {
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
                        PostMessage(g_hWndMain, WM_TRIGGER_CAPTURE, 0, 0);
                        if (g_config.hotkey.suppress) {
                            return 1; // 吞噬事件
                        }
                    }
                }
            }
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
    // 按钮总体尺寸 (精致矢量化大图标，自绘免 emoji/font 依赖)
    int btn_w = 44;
    int btn_h = 36;
    int spacing = 8;
    int sep_w = 16; // 分割线空间宽度
    
    // 长图预览模式下只有 保存/取消/确定 三个按钮
    if (g_longPreviewActive) {
        int total_w = btn_w * 3 + spacing * 2 + 20;
        int total_h = btn_h + 12;
        int tx = sel.right - total_w;
        int ty = sel.bottom + 8;
        if (ty + total_h > height) ty = sel.top - total_h - 8;
        if (ty < 0) ty = sel.bottom - total_h - 8;
        if (tx < 0) tx = sel.left;
        if (tx + total_w > width) tx = width - total_w - 4;
        int sx = tx + 10, sy = ty + 6;
        ToolbarButton bSave;
        bSave.rect = { sx, sy, sx + btn_w, sy + btn_h };
        bSave.text = L"save";
        bSave.color = Color(255, 200, 200, 200);
        btns.push_back(bSave);
        ToolbarButton bCancel;
        bCancel.rect = { sx + btn_w + spacing, sy, sx + btn_w * 2 + spacing, sy + btn_h };
        bCancel.text = L"cancel";
        bCancel.color = Color(255, 240, 92, 92);
        btns.push_back(bCancel);
        ToolbarButton bOk;
        bOk.rect = { sx + (btn_w + spacing) * 2, sy, sx + (btn_w + spacing) * 2 + btn_w, sy + btn_h };
        bOk.text = L"confirm";
        bOk.color = Color(255, 46, 204, 113);
        btns.push_back(bOk);
        return btns;
    }

    // 11个按钮，2个分割线区域，加上左右 padding
    int total_w = btn_w * 11 + spacing * 8 + sep_w * 2 + 20;
    int total_h = btn_h + 12;
    if (g_annotationMode != ANNOTATION_NONE) {
        total_h += 44; // 预留属性子工具栏的空间 (增大到 44px)
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
    
    int start_x = tx + 10;
    int start_y = ty + 6;
    
    // 计算每个按钮的 X 轴坐标
    // 组 1: 矩形, 圆形, 箭头, 画笔, 马赛克, 文字, 长截屏 (7个)
    int x0 = start_x;
    int x1 = x0 + btn_w + spacing;
    int x2 = x1 + btn_w + spacing;
    int x3 = x2 + btn_w + spacing;
    int x4 = x3 + btn_w + spacing;
    int x5 = x4 + btn_w + spacing;
    int x6 = x5 + btn_w + spacing;

    // 组 2: 撤销 (1个，在分割线 1 之后)
    int x7 = x6 + btn_w + sep_w;

    // 组 3: 保存, 取消, 确定 (3个，在分割线 2 之后)
    int x8 = x7 + btn_w + sep_w;
    int x9 = x8 + btn_w + spacing;
    int x10 = x9 + btn_w + spacing;
    
    // 1. 矩形
    ToolbarButton bRect;
    bRect.rect = { x0, start_y, x0 + btn_w, start_y + btn_h };
    bRect.text = L"rect";
    bRect.color = Color(255, 200, 200, 200); // 默认偏白灰
    btns.push_back(bRect);
    
    // 2. 圆形
    ToolbarButton bCircle;
    bCircle.rect = { x1, start_y, x1 + btn_w, start_y + btn_h };
    bCircle.text = L"circle";
    bCircle.color = Color(255, 200, 200, 200);
    btns.push_back(bCircle);
    
    // 3. 箭头
    ToolbarButton bArrow;
    bArrow.rect = { x2, start_y, x2 + btn_w, start_y + btn_h };
    bArrow.text = L"arrow";
    bArrow.color = Color(255, 200, 200, 200);
    btns.push_back(bArrow);
    
    // 4. 画笔
    ToolbarButton bPencil;
    bPencil.rect = { x3, start_y, x3 + btn_w, start_y + btn_h };
    bPencil.text = L"pencil";
    bPencil.color = Color(255, 200, 200, 200);
    btns.push_back(bPencil);
    
    // 5. 马赛克
    ToolbarButton bMosaic;
    bMosaic.rect = { x4, start_y, x4 + btn_w, start_y + btn_h };
    bMosaic.text = L"mosaic";
    bMosaic.color = Color(255, 200, 200, 200);
    btns.push_back(bMosaic);

    // 6. 文字
    ToolbarButton bText;
    bText.rect = { x5, start_y, x5 + btn_w, start_y + btn_h };
    bText.text = L"text";
    bText.color = Color(255, 200, 200, 200);
    btns.push_back(bText);

    // 7. 长截屏
    ToolbarButton bLong;
    bLong.rect = { x6, start_y, x6 + btn_w, start_y + btn_h };
    bLong.text = L"long";
    bLong.color = Color(255, 200, 200, 200);
    btns.push_back(bLong);

    // 8. 撤销
    ToolbarButton bUndo;
    bUndo.rect = { x7, start_y, x7 + btn_w, start_y + btn_h };
    bUndo.text = L"undo";
    bUndo.color = Color(255, 200, 200, 200);
    btns.push_back(bUndo);

    // 9. 保存
    ToolbarButton bSave;
    bSave.rect = { x8, start_y, x8 + btn_w, start_y + btn_h };
    bSave.text = L"save";
    bSave.color = Color(255, 200, 200, 200);
    btns.push_back(bSave);

    // 10. 取消 (红色)
    ToolbarButton bCancel;
    bCancel.rect = { x9, start_y, x9 + btn_w, start_y + btn_h };
    bCancel.text = L"cancel";
    bCancel.color = Color(255, 240, 92, 92); // 软红色
    btns.push_back(bCancel);

    // 11. 确定 (绿色)
    ToolbarButton bConfirm;
    bConfirm.rect = { x10, start_y, x10 + btn_w, start_y + btn_h };
    bConfirm.text = L"confirm";
    bConfirm.color = Color(255, 46, 204, 113); // 软绿色
    btns.push_back(bConfirm);
    
    return btns;
}

// ========== Win32 选区控制手柄边界判定函数 ==========
ResizeHandle GetHandleAtPoint(const RECT& rect, POINT pt) {
    if (!g_overlay.selectionDone) return NONE_HANDLE;
    if (g_longPreviewActive) return NONE_HANDLE; // 长图预览的框不可调整
    
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
        for (auto& shp : g_shapes) {
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
                DrawRoundedRectangle(g, &pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh, (REAL)shp.roundRadius);
            } else if (shp.type == SHAPE_CIRCLE) {
                int rx = min(startX, endX);
                int ry = min(startY, endY);
                int rw = abs(startX - endX);
                int rh = abs(startY - endY);
                g.DrawEllipse(&pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh);
            } else if (shp.type == SHAPE_PENCIL) {
                if (shp.points.size() > 1) {
                    pen.SetStartCap(LineCapRound);
                    pen.SetEndCap(LineCapRound);
                    pen.SetLineJoin(LineJoinRound);
                    vector<PointF> pts(shp.points.size());
                    for (size_t k = 0; k < shp.points.size(); ++k) {
                        pts[k] = PointF((REAL)(shp.points[k].x - x1), (REAL)(shp.points[k].y - y1));
                    }
                    g.DrawLines(&pen, pts.data(), (INT)pts.size());
                }
            } else if (shp.type == SHAPE_MOSAIC || shp.type == SHAPE_MOSAIC_RECT) {
                DrawMosaicShape(g, shp, -x1, -y1);
            } else if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                EnsureTextShapeMeasured(g, shp);
                Font* pFont = CreateSafeGdiplusFont(shp.fontFamily.c_str(), (REAL)shp.fontSize, (FontStyle)shp.fontStyle);

                SolidBrush txtBrush(shp.color);

                float cx = (shp.start.x + shp.end.x) / 2.0f - x1;
                float cy = (shp.start.y + shp.end.y) / 2.0f - y1;

                GraphicsState state = g.Save();
                g.SetTextRenderingHint(TextRenderingHintAntiAlias);
                g.TranslateTransform(cx, cy);
                g.RotateTransform(shp.angle);
                g.ScaleTransform(shp.scale, shp.scale);

                g.DrawString(shp.text.c_str(), -1, pFont, PointF(-shp.origW / 2.0f - shp.origX, -shp.origH / 2.0f - shp.origY), &txtBrush);

                g.Restore(state);
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

    // 局部合成：只重绘本帧失效矩形，其余区域复用双缓冲上一帧内容。
    // 双缓冲持久存在，上一帧画面仍然有效，因此可以按矩形增量更新。
    RECT pr = g_paintRect;
    if (pr.left < 0) pr.left = 0;
    if (pr.top < 0) pr.top = 0;
    if (pr.right > w) pr.right = w;
    if (pr.bottom > h) pr.bottom = h;

    // 1. 极速拷贝预渲染的半透明遮罩背景图 (Direct VRAM/Kernel BitBlt，仅更新区域)
    if (pr.right > pr.left && pr.bottom > pr.top) {
        BitBlt(hMemDC, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top,
               g_hCaptureMaskedSourceDC, pr.left, pr.top, SRCCOPY);
    }

    Graphics g(hMemDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetClip(Rect(pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top));
    
    RECT sel = g_overlay.selection;
    bool hasSelection = (sel.right - sel.left > 0 && sel.bottom - sel.top > 0);
    
    if (hasSelection) {
        // 2. 选区内容：长图预览模式绘制缩放后的长图；普通模式恢复清晰原背景（仅更新区域）
        RECT rcSel = { sel.left, sel.top, sel.right, sel.bottom };
        RECT rcClr;
        if (IntersectRect(&rcClr, &rcSel, &pr)) {
            if (g_longPreviewActive && g_longViewBmp) {
                g.DrawImage(g_longViewBmp, RectF((REAL)rcClr.left, (REAL)rcClr.top,
                            (REAL)(rcClr.right - rcClr.left), (REAL)(rcClr.bottom - rcClr.top)),
                            (REAL)(rcClr.left - sel.left), (REAL)(rcClr.top - sel.top),
                            (REAL)(rcClr.right - rcClr.left), (REAL)(rcClr.bottom - rcClr.top), UnitPixel);
            } else {
                BitBlt(hMemDC, rcClr.left, rcClr.top, rcClr.right - rcClr.left, rcClr.bottom - rcClr.top,
                       g_hCaptureSourceDC, rcClr.left, rcClr.top, SRCCOPY);
            }
        }
        
        // 绘制所有已画好的标注图形 (包括箭头、矩形框、文字)
        for (auto& shp : g_shapes) {
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
                DrawRoundedRectangle(g, &pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh, (REAL)shp.roundRadius);
            } else if (shp.type == SHAPE_CIRCLE) {
                int rx = min(shp.start.x, shp.end.x);
                int ry = min(shp.start.y, shp.end.y);
                int rw = abs(shp.start.x - shp.end.x);
                int rh = abs(shp.start.y - shp.end.y);
                g.DrawEllipse(&pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh);
            } else if (shp.type == SHAPE_PENCIL) {
                if (shp.points.size() > 1) {
                    pen.SetStartCap(LineCapRound);
                    pen.SetEndCap(LineCapRound);
                    pen.SetLineJoin(LineJoinRound);
                    vector<PointF> pts(shp.points.size());
                    for (size_t k = 0; k < shp.points.size(); ++k) {
                        pts[k] = PointF((REAL)shp.points[k].x, (REAL)shp.points[k].y);
                    }
                    g.DrawLines(&pen, pts.data(), (INT)pts.size());
                }
            } else if (shp.type == SHAPE_MOSAIC || shp.type == SHAPE_MOSAIC_RECT) {
                DrawMosaicShape(g, shp, 0, 0);
            } else if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                EnsureTextShapeMeasured(g, shp);
                Font* pFont = CreateSafeGdiplusFont(shp.fontFamily.c_str(), (REAL)shp.fontSize, (FontStyle)shp.fontStyle);

                SolidBrush txtBrush(shp.color);
                float cx = (shp.start.x + shp.end.x) / 2.0f;
                float cy = (shp.start.y + shp.end.y) / 2.0f;
                
                GraphicsState state = g.Save();
                g.SetTextRenderingHint(TextRenderingHintAntiAlias);
                g.TranslateTransform(cx, cy);
                g.RotateTransform(shp.angle);
                g.ScaleTransform(shp.scale, shp.scale);
                
                g.DrawString(shp.text.c_str(), -1, pFont, PointF(-shp.origW / 2.0f - shp.origX, -shp.origH / 2.0f - shp.origY), &txtBrush);
                
                g.Restore(state);
            }
        }
        
        // 绘制当前选中的文字的虚线外框与旋转/缩放手柄 (方正极简现代品牌蓝)
        if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
            const auto& shp = g_shapes[g_selectedTextIndex];
            if (shp.type == SHAPE_TEXT && !shp.text.empty() && shp.origW > 0.0f) {
                float cx = (shp.start.x + shp.end.x) / 2.0f;
                float cy = (shp.start.y + shp.end.y) / 2.0f;
                
                GraphicsState state = g.Save();
                g.TranslateTransform(cx, cy);
                g.RotateTransform(shp.angle);
                g.ScaleTransform(shp.scale, shp.scale);
                
                // 1. 绘制虚线选框
                Pen selectBorderPen(Color(255, 0, 174, 255), 1.0f / shp.scale);
                selectBorderPen.SetDashStyle(DashStyleDash);
                g.DrawRectangle(&selectBorderPen, -shp.origW / 2.0f - 4, -shp.origH / 2.0f - 2, shp.origW + 8, shp.origH + 4);
                
                // 2. 绘制 3 个缩放手柄 (TL, TR, BL)
                float handleSize = 6.0f / shp.scale;
                float hs = handleSize / 2.0f;
                SolidBrush handleBrush(Color(255, 0, 174, 255));
                Pen whitePen(Color(255, 255, 255), 1.0f / shp.scale);
                
                g.FillRectangle(&handleBrush, -shp.origW/2.0f - 4 - hs, -shp.origH/2.0f - 2 - hs, handleSize, handleSize);
                g.DrawRectangle(&whitePen, -shp.origW/2.0f - 4 - hs, -shp.origH/2.0f - 2 - hs, handleSize, handleSize);
                
                g.FillRectangle(&handleBrush, shp.origW/2.0f + 4 - hs, -shp.origH/2.0f - 2 - hs, handleSize, handleSize);
                g.DrawRectangle(&whitePen, shp.origW/2.0f + 4 - hs, -shp.origH/2.0f - 2 - hs, handleSize, handleSize);
                
                g.FillRectangle(&handleBrush, -shp.origW/2.0f - 4 - hs, shp.origH/2.0f + 2 - hs, handleSize, handleSize);
                g.DrawRectangle(&whitePen, -shp.origW/2.0f - 4 - hs, shp.origH/2.0f + 2 - hs, handleSize, handleSize);
                
                // 3. 绘制右下角 (BR) 旋转圈 handle (醒目大圆圈，方便点击)
                float circleRadius = 18.0f / shp.scale;
                float rx = shp.origW / 2.0f + 18.0f;
                float ry = shp.origH / 2.0f + 14.0f;
                
                // 连接线：从 BR 角到旋转圈
                Pen connPen(Color(200, 0, 174, 255), 1.5f / shp.scale);
                g.DrawLine(&connPen, shp.origW/2.0f + 4, shp.origH/2.0f + 2, rx, ry);
                
                // 旋转大圆圈
                SolidBrush circleBrush(Color(255, 0, 174, 255));
                g.FillEllipse(&circleBrush, rx - circleRadius, ry - circleRadius, circleRadius * 2, circleRadius * 2);
                Pen circleOutline(Color(255, 255, 255), 2.0f / shp.scale);
                g.DrawEllipse(&circleOutline, rx - circleRadius, ry - circleRadius, circleRadius * 2, circleRadius * 2);
                
                // 在圆圈中间画一个旋转箭头矢量图标 (↻)
                float iconR = circleRadius * 0.5f;
                Pen iconPen(Color(255, 255, 255), 1.5f / shp.scale);
                g.DrawArc(&iconPen, rx - iconR, ry - iconR, iconR * 2, iconR * 2, -60.0f, 240.0f);
                // 箭头尖
                float arrowTipX = rx + iconR * cos(-60.0f * 3.14159265f / 180.0f);
                float arrowTipY = ry + iconR * sin(-60.0f * 3.14159265f / 180.0f);
                float as = 3.0f / shp.scale;
                g.DrawLine(&iconPen, arrowTipX, arrowTipY, arrowTipX + as, arrowTipY + as * 1.5f);
                g.DrawLine(&iconPen, arrowTipX, arrowTipY, arrowTipX - as * 1.2f, arrowTipY + as * 0.5f);
                
                g.Restore(state);
            }
        }
        
        // 绘制正在编辑中的文字（带光标与输入法组合串）
        if (g_isEditingText) {
            Font* pFont = CreateSafeGdiplusFont(g_currentFontFamily.c_str(), (REAL)g_currentFontSize, (FontStyle)g_currentFontStyle);
            SolidBrush txtBrush(g_currentColor);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            // 输入法的组合串临时插在光标处：前缀 + 组合串 + 后缀
            wstring prefix = g_editingText.substr(0, g_editingCaretIndex);
            wstring comp = g_imeComposing ? g_imeComposition : wstring();
            wstring suffix = (g_editingCaretIndex < (int)g_editingText.length())
                ? g_editingText.substr(g_editingCaretIndex) : wstring();

            wstring displayText = prefix + comp + suffix;
            if (displayText.empty()) displayText = L" ";

            RectF cursorBound;
            g.MeasureString(displayText.c_str(), -1, pFont, PointF(0, 0), &cursorBound);
            float ew = cursorBound.Width;
            float eh = cursorBound.Height;
            float ex = cursorBound.X;
            float ey = cursorBound.Y;

            float cx = g_editTextPos.x + ew / 2.0f;
            float cy = g_editTextPos.y + eh / 2.0f;

            float left = -ew / 2.0f - ex;
            float top = -eh / 2.0f - ey;

            float prefixW = 0.0f;
            float compW = 0.0f;
            if (!prefix.empty()) {
                RectF b;
                g.MeasureString(prefix.c_str(), -1, pFont, PointF(0, 0), &b);
                prefixW = b.Width;
            }
            if (!comp.empty()) {
                RectF b;
                g.MeasureString(comp.c_str(), -1, pFont, PointF(0, 0), &b);
                compW = b.Width;
            }

            GraphicsState state = g.Save();
            g.TranslateTransform(cx, cy);
            g.RotateTransform(g_editAngle);
            g.ScaleTransform(g_editScale, g_editScale);

            float x = left;
            if (!prefix.empty()) {
                g.DrawString(prefix.c_str(), -1, pFont, PointF(x, top), &txtBrush);
            }
            x += prefixW;

            if (!comp.empty()) {
                g.DrawString(comp.c_str(), -1, pFont, PointF(x, top), &txtBrush);
                Pen underlinePen(g_currentColor, 1.0f / g_editScale);
                float uy = top + eh - 2.0f;
                g.DrawLine(&underlinePen, x, uy, x + compW, uy);
            }
            x += compW;

            if (!suffix.empty()) {
                g.DrawString(suffix.c_str(), -1, pFont, PointF(x, top), &txtBrush);
            }

            // 光标位置：组合输入中跟随组合内光标，否则跟随文本光标
            float caretX = left + prefixW;
            if (!comp.empty() && g_imeCaretInComposition > 0) {
                wstring compPrefix = comp.substr(0, g_imeCaretInComposition);
                RectF b;
                g.MeasureString(compPrefix.c_str(), -1, pFont, PointF(0, 0), &b);
                caretX += b.Width;
            }
            float caretY = -eh / 2.0f + 2.0f;
            float caretH = eh - 4.0f;

            // 记录光标屏幕坐标，供输入法候选窗口定位
            {
                float rad = g_editAngle * 3.14159265f / 180.0f;
                float sx = caretX * g_editScale;
                float sy = (caretY + caretH) * g_editScale;
                g_editCaretScreenX = cx + (sx * cos(rad) - sy * sin(rad));
                g_editCaretScreenY = cy + (sx * sin(rad) + sy * cos(rad));
            }

            // 绘制闪烁光标竖线
            DWORD tick = GetTickCount();
            if ((tick / 500) % 2 == 0) {
                Pen cursorPen(g_currentColor, 2.0f / g_editScale);
                g.DrawLine(&cursorPen, caretX, caretY, caretX, caretY + caretH);
            }

            // 绘制输入框虚线边框
            Pen inputBorderPen(Color(150, 0, 174, 255), 1.0f / g_editScale);
            inputBorderPen.SetDashStyle(DashStyleDash);
            float bx = -ew / 2.0f - 4;
            float by = -eh / 2.0f - 2;
            float bw = max(ew + 12.0f, 40.0f);
            float bh = eh + 4;
            g.DrawRectangle(&inputBorderPen, bx, by, bw, bh);

            g.Restore(state);
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
                DrawRoundedRectangle(g, &pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh, (REAL)g_tempShape.roundRadius);
            } else if (g_tempShape.type == SHAPE_CIRCLE) {
                int rx = min(g_tempShape.start.x, g_tempShape.end.x);
                int ry = min(g_tempShape.start.y, g_tempShape.end.y);
                int rw = abs(g_tempShape.start.x - g_tempShape.end.x);
                int rh = abs(g_tempShape.start.y - g_tempShape.end.y);
                g.DrawEllipse(&pen, (REAL)rx, (REAL)ry, (REAL)rw, (REAL)rh);
            } else if (g_tempShape.type == SHAPE_PENCIL) {
                if (g_tempShape.points.size() > 1) {
                    pen.SetStartCap(LineCapRound);
                    pen.SetEndCap(LineCapRound);
                    pen.SetLineJoin(LineJoinRound);
                    vector<PointF> pts(g_tempShape.points.size());
                    for (size_t k = 0; k < g_tempShape.points.size(); ++k) {
                        pts[k] = PointF((REAL)g_tempShape.points[k].x, (REAL)g_tempShape.points[k].y);
                    }
                    g.DrawLines(&pen, pts.data(), (INT)pts.size());
                }
            } else if (g_tempShape.type == SHAPE_MOSAIC || g_tempShape.type == SHAPE_MOSAIC_RECT) {
                DrawMosaicShape(g, g_tempShape, 0, 0);
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
        
        // 5. 绘制尺寸标签（长图预览显示成品全尺寸）
        wstring dimText;
        if (g_longPreviewActive) {
            dimText = L"长截屏 " + ToWString(g_longFinalW) + L" \u00d7 " + ToWString(g_longFinalH);
        } else {
            dimText = ToWString(sel.right - sel.left) + L" \u00d7 " + ToWString(sel.bottom - sel.top);
        }
        Font* font = CreateSafeGdiplusFont(L"Microsoft YaHei", 9, FontStyleBold);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        
        RectF textBounding;
        g.MeasureString(dimText.c_str(), -1, font, PointF(0, 0), &textBounding);
        
        int label_w = (int)textBounding.Width + 12;
        int label_h = (int)textBounding.Height + 6;
        int label_x = sel.left;
        int label_y = sel.top - label_h - 6;
        if (label_y < 0) label_y = sel.bottom + 6;
        if (label_x + label_w > w) label_x = w - label_w - 4;
        
        SolidBrush textBgBrush(Color(205, 15, 15, 17));
        FillRoundedRectangle(g, &textBgBrush, (REAL)label_x, (REAL)label_y, (REAL)label_w, (REAL)label_h, (REAL)label_h / 2.0f);
        
        SolidBrush textBrush(Color(255, 255, 255, 255));
        g.DrawString(dimText.c_str(), -1, font, RectF((REAL)label_x, (REAL)label_y, (REAL)label_w, (REAL)label_h), &format, &textBrush);
        
        // 6. 绘制悬浮工具栏 (口 矩形, ↗ 箭头, ↶ 撤销, ✓ 确认, 💾 保存, ✗ 取消)
        if (g_overlay.selectionDone && !g_overlay.isResizing) {
            auto buttons = GetToolbarButtons(sel, w, h);

            // 滑入动画：选定后工具栏从下方 14px 处缓出滑入（计时器驱动，结束自动归位）
            REAL slideDy = 0.0f;
            if (g_toolbarAnimStart != 0) {
                DWORD animElapsed = GetTickCount() - g_toolbarAnimStart;
                if (animElapsed < TOOLBAR_ANIM_MS) {
                    REAL tRemain = 1.0f - (REAL)animElapsed / (REAL)TOOLBAR_ANIM_MS;
                    slideDy = 14.0f * tRemain * tRemain;
                } else {
                    g_toolbarAnimStart = 0;
                }
            }
            GraphicsState toolbarAnimState = 0;
            if (slideDy > 0.0f) {
                toolbarAnimState = g.Save();
                g.TranslateTransform(0.0f, slideDy);
            }
            
            // 计算工具栏大背景盒 (深黑圆角底盒 + 柔和投影)
            int min_x = buttons[0].rect.left - 8;
            int max_x = buttons.back().rect.right + 8;
            int min_y = buttons[0].rect.top - 6;
            int max_y = buttons[0].rect.bottom + 6;

            SolidBrush barShadowBrush(Color(60, 0, 0, 0));
            FillRoundedRectangle(g, &barShadowBrush, (REAL)min_x + 2, (REAL)min_y + 3, (REAL)(max_x - min_x), (REAL)(max_y - min_y), 9.0f);

            SolidBrush barBrush(Color(242, 24, 24, 28));
            FillRoundedRectangle(g, &barBrush, (REAL)min_x, (REAL)min_y, (REAL)(max_x - min_x), (REAL)(max_y - min_y), 8.0f);

            Pen barBorderPen(Color(255, 62, 62, 68), 1.0f);
            DrawRoundedRectangle(g, &barBorderPen, (REAL)min_x, (REAL)min_y, (REAL)(max_x - min_x), (REAL)(max_y - min_y), 8.0f);
            
            for (size_t i = 0; i < buttons.size(); ++i) {
                RECT br = buttons[i].rect;
                REAL bx = (REAL)br.left;
                REAL by = (REAL)br.top;
                REAL bw = (REAL)(br.right - br.left);
                REAL bh = (REAL)(br.bottom - br.top);
                
                bool isHovered = (g_overlay.hoveredButton == (int)(i + 1));
                bool isActive = false;
                
                // 检查该按钮是否处于激活的标注模式
                if (i == 0 && g_annotationMode == ANNOTATION_RECTANGLE) isActive = true;
                else if (i == 1 && g_annotationMode == ANNOTATION_CIRCLE) isActive = true;
                else if (i == 2 && g_annotationMode == ANNOTATION_ARROW) isActive = true;
                else if (i == 3 && g_annotationMode == ANNOTATION_PENCIL) isActive = true;
                else if (i == 4 && g_annotationMode == ANNOTATION_MOSAIC) isActive = true;
                else if (i == 5 && g_annotationMode == ANNOTATION_TEXT) isActive = true;
                
                // A. 绘制按钮背景
                if (isActive) {
                    SolidBrush activeBgBrush(Color(255, 45, 55, 75)); // 微蓝灰
                    FillRoundedRectangle(g, &activeBgBrush, bx, by, bw, bh, 6.0f);

                    Pen activeBorderPen(Color(255, 0, 174, 255), 1.0f); // 品牌蓝
                    DrawRoundedRectangle(g, &activeBorderPen, bx, by, bw, bh, 6.0f);
                }
                else if (isHovered) {
                    SolidBrush hoverBgBrush(Color(255, 58, 58, 63));
                    FillRoundedRectangle(g, &hoverBgBrush, bx, by, bw, bh, 6.0f);
                }
                
                // B. 绘制矢量图形图标
                Color iconColor = buttons[i].color;
                if (isActive) {
                    iconColor = Color(255, 0, 174, 255); // 激活图标变成品牌蓝色
                } else if (isHovered) {
                    int hoverIdx = g_longPreviewActive ? ((i == 0) ? 9 : (i == 1) ? 10 : -1) : (int)i;
                    if (hoverIdx == 9) iconColor = Color(255, 255, 70, 70); // 悬停红更亮 (取消)
                    else if (hoverIdx == 10) iconColor = Color(255, 70, 230, 130); // 悬停绿更亮 (确定)
                    else iconColor = Color(255, 255, 255, 255); // 普通白亮
                }
                
                REAL cx = bx + bw / 2.0f;
                REAL cy = by + bh / 2.0f;
                
                Pen iconPen(iconColor, 1.6f);
                iconPen.SetStartCap(LineCapRound);
                iconPen.SetEndCap(LineCapRound);
                iconPen.SetLineJoin(LineJoinRound);

                // 悬停时图标轻微放大，增强操作预感
                GraphicsState iconGrowState = 0;
                if (isHovered) {
                    iconGrowState = g.Save();
                    g.TranslateTransform(cx, cy);
                    g.ScaleTransform(1.12f, 1.12f);
                    g.TranslateTransform(-cx, -cy);
                }

                // 长图预览模式只有 保存/取消/确定 三个按钮，映射到对应图标
                int drawIdx = i;
                if (g_longPreviewActive) {
                    drawIdx = (i == 0) ? 8 : (i == 1) ? 9 : 10;
                }

                switch (drawIdx) {
                    case 0: { // 矩形（横向圆角矩形，与整体圆角视觉语言呼应）
                        DrawRoundedRectangle(g, &iconPen, cx - 9.0f, cy - 6.5f, 18.0f, 13.0f, 3.0f);
                        break;
                    }
                    case 1: { // 圆形
                        g.DrawEllipse(&iconPen, cx - 7.5f, cy - 7.5f, 15.0f, 15.0f);
                        break;
                    }
                    case 2: { // 箭头（45° 主轴 + 对称回折的箭头头部）
                        g.DrawLine(&iconPen, cx - 8.5f, cy + 8.5f, cx + 7.5f, cy - 7.5f);
                        g.DrawLine(&iconPen, cx + 7.5f, cy - 7.5f, cx - 1.0f, cy - 7.5f);
                        g.DrawLine(&iconPen, cx + 7.5f, cy - 7.5f, cx + 7.5f, cy + 1.0f);
                        break;
                    }
                    case 3: { // 画笔（45° 笔杆 + 分界线 + 收拢的笔尖）
                        g.DrawLine(&iconPen, cx + 9.0f, cy - 5.0f, cx - 3.0f, cy + 7.0f);   // 笔杆上沿
                        g.DrawLine(&iconPen, cx + 5.0f, cy - 9.0f, cx - 7.0f, cy + 3.0f);   // 笔杆下沿
                        g.DrawLine(&iconPen, cx + 9.0f, cy - 5.0f, cx + 5.0f, cy - 9.0f);   // 笔杆尾端封口
                        g.DrawLine(&iconPen, cx - 3.0f, cy + 7.0f, cx - 7.0f, cy + 3.0f);   // 笔尖分界线
                        g.DrawLine(&iconPen, cx - 3.0f, cy + 7.0f, cx - 8.0f, cy + 8.0f);   // 笔尖左缘
                        g.DrawLine(&iconPen, cx - 7.0f, cy + 3.0f, cx - 8.0f, cy + 8.0f);   // 笔尖右缘
                        break;
                    }
                    case 4: { // 马赛克（3×3 棋盘格像素块）
                        SolidBrush mBrush(iconColor);
                        REAL ms = 5.0f;
                        REAL mx0 = cx - 7.5f, my0 = cy - 7.5f;
                        g.FillRectangle(&mBrush, mx0, my0, ms, ms);
                        g.FillRectangle(&mBrush, mx0 + 2 * ms, my0, ms, ms);
                        g.FillRectangle(&mBrush, mx0 + ms, my0 + ms, ms, ms);
                        g.FillRectangle(&mBrush, mx0, my0 + 2 * ms, ms, ms);
                        g.FillRectangle(&mBrush, mx0 + 2 * ms, my0 + 2 * ms, ms, ms);
                        break;
                    }
                    case 5: { // 文字（无衬线大写 T）
                        g.DrawLine(&iconPen, cx - 6.5f, cy - 5.5f, cx + 6.5f, cy - 5.5f);   // 顶横
                        g.DrawLine(&iconPen, cx, cy - 5.5f, cx, cy + 7.0f);                 // 竖笔
                        break;
                    }
                    case 6: { // 长截屏（页面 + 底部向下箭头）
                        DrawRoundedRectangle(g, &iconPen, cx - 6.5f, cy - 9.0f, 13.0f, 12.0f, 2.0f);
                        g.DrawLine(&iconPen, cx - 3.5f, cy - 5.5f, cx + 3.5f, cy - 5.5f);
                        g.DrawLine(&iconPen, cx - 3.5f, cy - 2.5f, cx + 3.5f, cy - 2.5f);
                        g.DrawLine(&iconPen, cx, cy + 4.0f, cx, cy + 9.0f);
                        g.DrawLine(&iconPen, cx - 3.5f, cy + 5.5f, cx, cy + 9.0f);
                        g.DrawLine(&iconPen, cx + 3.5f, cy + 5.5f, cx, cy + 9.0f);
                        break;
                    }
                    case 7: { // 撤销（逆时针 3/4 圆弧，箭头贴弧线终点指向行进方向）
                        Pen undoPen(iconColor, 2.0f);
                        undoPen.SetStartCap(LineCapRound);
                        undoPen.SetEndCap(LineCapRound);
                        undoPen.SetLineJoin(LineJoinRound);
                        g.DrawArc(&undoPen, cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 180.0f, -270.0f);
                        g.DrawLine(&undoPen, cx + 4.7f, cy - 9.4f, cx, cy - 7.5f);
                        g.DrawLine(&undoPen, cx, cy - 7.5f, cx + 4.7f, cy - 5.6f);
                        break;
                    }
                    case 8: { // 保存（下载托盘：向下的箭头 + 底部承托槽）
                        g.DrawLine(&iconPen, cx, cy - 8.0f, cx, cy + 2.5f);                 // 箭杆
                        g.DrawLine(&iconPen, cx, cy + 2.5f, cx - 4.5f, cy - 2.0f);          // 左翼
                        g.DrawLine(&iconPen, cx, cy + 2.5f, cx + 4.5f, cy - 2.0f);          // 右翼
                        g.DrawLine(&iconPen, cx - 8.0f, cy + 4.5f, cx - 8.0f, cy + 8.0f);   // 托盘左壁
                        g.DrawLine(&iconPen, cx - 8.0f, cy + 8.0f, cx + 8.0f, cy + 8.0f);   // 托盘底
                        g.DrawLine(&iconPen, cx + 8.0f, cy + 8.0f, cx + 8.0f, cy + 4.5f);   // 托盘右壁
                        break;
                    }
                    case 9: { // 取消
                        g.DrawLine(&iconPen, cx - 6.5f, cy - 6.5f, cx + 6.5f, cy + 6.5f);
                        g.DrawLine(&iconPen, cx + 6.5f, cy - 6.5f, cx - 6.5f, cy + 6.5f);
                        break;
                    }
                    case 10: { // 确定
                        g.DrawLine(&iconPen, cx - 7.5f, cy + 0.5f, cx - 2.5f, cy + 5.5f);
                        g.DrawLine(&iconPen, cx - 2.5f, cy + 5.5f, cx + 7.5f, cy - 5.0f);
                        break;
                    }
                }

                if (iconGrowState) g.Restore(iconGrowState);
            }

            // 绘制分割线
            if (buttons.size() >= 9) {
                Pen sepPen(Color(255, 55, 55, 55), 1.0f);

                // 分割线 1 (长截屏 | 撤销)
                int sep1_x = (buttons[6].rect.right + buttons[7].rect.left) / 2;
                g.DrawLine(&sepPen, (REAL)sep1_x, (REAL)(min_y + 8), (REAL)sep1_x, (REAL)(max_y - 8));

                // 分割线 2 (撤销 | 保存)
                int sep2_x = (buttons[7].rect.right + buttons[8].rect.left) / 2;
                g.DrawLine(&sepPen, (REAL)sep2_x, (REAL)(min_y + 8), (REAL)sep2_x, (REAL)(max_y - 8));
            }
            
            // 7. 绘制属性子工具栏 (仅在激活标注模式或选中文字时显示)
            if (g_annotationMode != ANNOTATION_NONE || g_selectedTextIndex >= 0) {
                int st_x = min_x;
                int st_y = max_y + 4;
                int st_w = max_x - min_x;
                int st_h = 36;
                
                FillRoundedRectangle(g, &barBrush, (REAL)st_x, (REAL)st_y, (REAL)st_w, (REAL)st_h, 8.0f);
                DrawRoundedRectangle(g, &barBorderPen, (REAL)st_x, (REAL)st_y, (REAL)st_w, (REAL)st_h, 8.0f);
                bool isTextMode = (g_annotationMode == ANNOTATION_TEXT || g_isEditingText);
                if (!isTextMode && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                    if (g_shapes[g_selectedTextIndex].type == SHAPE_TEXT) {
                        isTextMode = true;
                    }
                }
                
                bool isRectMode = (g_annotationMode == ANNOTATION_RECTANGLE);
                if (!isRectMode && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                    if (g_shapes[g_selectedTextIndex].type == SHAPE_RECTANGLE) {
                        isRectMode = true;
                    }
                }
                
                if (g_annotationMode == ANNOTATION_MOSAIC) {
                    // --- 马赛克子工具栏：模式/风格切换 + 笔刷粗细 + 程度（无颜色行）---
                    REAL chipY = (REAL)st_y + 7.0f;
                    DrawSubToolbarChip(g, (REAL)st_x + 12, chipY, 44.0f, 22.0f, g_mosaicShapeMode == 0, L"涂抹");
                    DrawSubToolbarChip(g, (REAL)st_x + 60, chipY, 44.0f, 22.0f, g_mosaicShapeMode == 1, L"矩形");
                    DrawSubToolbarChip(g, (REAL)st_x + 116, chipY, 44.0f, 22.0f, g_mosaicStyle == 0, L"像素");
                    DrawSubToolbarChip(g, (REAL)st_x + 164, chipY, 44.0f, 22.0f, g_mosaicStyle == 1, L"模糊");

                    // 笔刷粗细滑条（仅涂抹模式，6-80 无级可调，拖动即时生效）
                    if (g_mosaicShapeMode == 0) {
                        DrawSubToolbarSlider(g, (REAL)st_x + 218, (REAL)st_x + 250, 90.0f, (REAL)st_y + 17.0f,
                                             L"粗细", g_mosaicBrush, MOSAIC_BRUSH_MIN, MOSAIC_BRUSH_MAX,
                                             g_mosaicSliderDrag == 1);
                    }

                    // 程度滑条 (4-40 无级可调，拖动中节流重建图层，松手定稿)
                    DrawSubToolbarSlider(g, (REAL)st_x + 380, (REAL)st_x + 412, 90.0f, (REAL)st_y + 17.0f,
                                         L"质量", g_mosaicBlock, MOSAIC_BLOCK_MIN, MOSAIC_BLOCK_MAX,
                                         g_mosaicSliderDrag == 2);
                } else if (!isTextMode) {
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
                            DrawRoundedRectangle(g, &selPen, (REAL)bx, (REAL)by, (REAL)bw, (REAL)bh, 6.0f);
                        }
                        
                        // 绘制中间的白色圆点
                        int ds = dot_sizes[i];
                        int dx = bx + (bw - ds) / 2;
                        int dy = by + (bh - ds) / 2;
                        g.FillEllipse(&textBrush, dx, dy, ds, ds);
                    }
                    
                    // --- 新增：绘制圆角大小调节器 (只在矩形标注模式或选中矩形时显示) ---
                    if (isRectMode) {
                        int currentRound = g_currentRoundRadius;
                        if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                            currentRound = g_shapes[g_selectedTextIndex].roundRadius;
                        }
                        
                        SolidBrush btnBg(Color(255, 60, 60, 60));
                        Font* controlFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 9, FontStyleBold);
                        StringFormat sfCenter;
                        sfCenter.SetAlignment(StringAlignmentCenter);
                        sfCenter.SetLineAlignment(StringAlignmentCenter);
                        
                        // 1. [-] 减少圆角按钮
                        RECT decRect = { st_x + 100, st_y + 8, st_x + 120, st_y + 28 };
                        FillRoundedRectangle(g, &btnBg, (REAL)decRect.left, (REAL)decRect.top, (REAL)(decRect.right - decRect.left), (REAL)(decRect.bottom - decRect.top), 4.0f);
                        g.DrawString(L"-", -1, controlFont, RectF(decRect.left, decRect.top, decRect.right - decRect.left, decRect.bottom - decRect.top), &sfCenter, &textBrush);

                        // 2. 圆角数值文本标签
                        wstring roundText = L"圆角: " + ToWString(currentRound);
                        RECT roundRect = { st_x + 124, st_y + 8, st_x + 188, st_y + 28 };
                        Font* textFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 8, FontStyleRegular);
                        {
                            SolidBrush inputBgBrush(Color(255, 44, 44, 48));
                            FillRoundedRectangle(g, &inputBgBrush, (REAL)roundRect.left, (REAL)roundRect.top, (REAL)(roundRect.right - roundRect.left), (REAL)(roundRect.bottom - roundRect.top), 4.0f);
                        }
                        if (g_isHoveringRoundRadius) {
                            // 悬停时绘制精致的蓝色高亮边框和自定义输入的字符
                            Pen borderHighlight(Color(255, 0, 174, 255), 1.5f);
                            DrawRoundedRectangle(g, &borderHighlight, (REAL)roundRect.left, (REAL)roundRect.top, (REAL)(roundRect.right - roundRect.left), (REAL)(roundRect.bottom - roundRect.top), 4.0f);
                            wstring activeText = L"圆角: " + (g_editingRoundRadiusStr.empty() ? ToWString(currentRound) : g_editingRoundRadiusStr);
                            g.DrawString(activeText.c_str(), -1, textFont, RectF(roundRect.left, roundRect.top, roundRect.right - roundRect.left, roundRect.bottom - roundRect.top), &sfCenter, &textBrush);
                        } else {
                            g.DrawString(roundText.c_str(), -1, textFont, RectF(roundRect.left, roundRect.top, roundRect.right - roundRect.left, roundRect.bottom - roundRect.top), &sfCenter, &textBrush);
                        }

                        // 3. [+] 增加圆角按钮
                        RECT incRect = { st_x + 192, st_y + 8, st_x + 212, st_y + 28 };
                        FillRoundedRectangle(g, &btnBg, (REAL)incRect.left, (REAL)incRect.top, (REAL)(incRect.right - incRect.left), (REAL)(incRect.bottom - incRect.top), 4.0f);
                        g.DrawString(L"+", -1, controlFont, RectF(incRect.left, incRect.top, incRect.right - incRect.left, incRect.bottom - incRect.top), &sfCenter, &textBrush);
                    }
                } else {
                    // --- 绘制字号与字体选择器 ---
                    int currentSize = g_currentFontSize;
                    wstring currentFamily = g_currentFontFamily;
                    if (!g_isEditingText && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                        currentSize = g_shapes[g_selectedTextIndex].fontSize;
                        currentFamily = g_shapes[g_selectedTextIndex].fontFamily;
                    }
                    
                    // 1. Draw Decrease Button [-]
                    RECT decRect = { st_x + 12, st_y + 8, st_x + 32, st_y + 28 };
                    SolidBrush btnBg(Color(255, 60, 60, 60));
                    FillRoundedRectangle(g, &btnBg, (REAL)decRect.left, (REAL)decRect.top, (REAL)(decRect.right - decRect.left), (REAL)(decRect.bottom - decRect.top), 4.0f);
                    
                    Font* controlFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 9, FontStyleBold);
                    StringFormat sfCenter;
                    sfCenter.SetAlignment(StringAlignmentCenter);
                    sfCenter.SetLineAlignment(StringAlignmentCenter);
                    SolidBrush textBrush(Color(255, 255, 255, 255));
                    
                    g.DrawString(L"-", -1, controlFont, RectF(decRect.left, decRect.top, decRect.right - decRect.left, decRect.bottom - decRect.top), &sfCenter, &textBrush);
                    
                    // 2. Draw Font Size Text Label
                    wstring sizeText = ToWString(currentSize);
                    RECT sizeRect = { st_x + 36, st_y + 8, st_x + 64, st_y + 28 };
                    g.DrawString(sizeText.c_str(), -1, controlFont, RectF(sizeRect.left, sizeRect.top, sizeRect.right - sizeRect.left, sizeRect.bottom - sizeRect.top), &sfCenter, &textBrush);
                    
                    // 3. Draw Increase Button [+]
                    RECT incRect = { st_x + 68, st_y + 8, st_x + 88, st_y + 28 };
                    FillRoundedRectangle(g, &btnBg, (REAL)incRect.left, (REAL)incRect.top, (REAL)(incRect.right - incRect.left), (REAL)(incRect.bottom - incRect.top), 4.0f);
                    g.DrawString(L"+", -1, controlFont, RectF(incRect.left, incRect.top, incRect.right - incRect.left, incRect.bottom - incRect.top), &sfCenter, &textBrush);

                    // 4. Draw Font Family Button
                    RECT fontRect = { st_x + 96, st_y + 8, st_x + 220, st_y + 28 };
                    FillRoundedRectangle(g, &btnBg, (REAL)fontRect.left, (REAL)fontRect.top, (REAL)(fontRect.right - fontRect.left), (REAL)(fontRect.bottom - fontRect.top), 4.0f);
                    
                    // Truncate font name if too long to fit
                    wstring dispFamily = currentFamily;
                    if (dispFamily.length() > 8) {
                        dispFamily = dispFamily.substr(0, 7) + L"..";
                    }
                    
                    Font* familyFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 8, FontStyleRegular);
                    g.DrawString(dispFamily.c_str(), -1, familyFont, RectF(fontRect.left, fontRect.top, fontRect.right - fontRect.left, fontRect.bottom - fontRect.top), &sfCenter, &textBrush);
                }
                
                // --- 绘制颜色选择器 (7个默认颜色 + 4个自定义色槽 + 1个修改设置按钮；马赛克模式无颜色行) ---
                if (g_annotationMode != ANNOTATION_MOSAIC) {
                Color colors[] = {
                    Color(255, 231, 76, 60),      // 红
                    Color(255, 52, 152, 219),     // 蓝
                    Color(255, 46, 204, 113),     // 绿
                    Color(255, 241, 196, 15),     // 黄
                    Color(255, 127, 140, 141),    // 灰
                    Color(255, 255, 255, 255),    // 白
                    Color(255, 44, 62, 80)        // 黑
                };
                
                int colorStart = 120;
                if (isTextMode) {
                    colorStart = 232;
                } else if (isRectMode) {
                    colorStart = 224;
                }
                for (int i = 0; i < 12; ++i) {
                    int cx = st_x + colorStart + i * 20;
                    int cy = st_y + 9;
                    int cw = 16;
                    int ch = 16;
                    
                    if (i < 7) {
                        // 7个默认颜色
                        SolidBrush cBrush(colors[i]);
                        FillRoundedRectangle(g, &cBrush, (REAL)cx, (REAL)cy, (REAL)cw, (REAL)ch, 4.0f);

                        // 如果被选中，绘制一个带有黑/白内缩外框的高亮框
                        if (g_currentColor.GetValue() == colors[i].GetValue()) {
                            Pen whitePen(Color(255, 255, 255, 255), 2.0f);
                            DrawRoundedRectangle(g, &whitePen, (REAL)cx - 2, (REAL)cy - 2, (REAL)cw + 4, (REAL)ch + 4, 6.0f);

                            Pen blackPen(Color(255, 0, 0, 0), 1.0f);
                            DrawRoundedRectangle(g, &blackPen, (REAL)cx - 1, (REAL)cy - 1, (REAL)cw + 2, (REAL)ch + 2, 5.0f);
                        }
                    } else if (i >= 7 && i < 11) {
                        // 4个自定义色槽
                        int slotIdx = i - 7;
                        Color customCol = g_config.custom_colors[slotIdx];
                        SolidBrush cBrush(customCol);
                        FillRoundedRectangle(g, &cBrush, (REAL)cx, (REAL)cy, (REAL)cw, (REAL)ch, 4.0f);

                        // 如果当前选中的颜色恰好是这个色槽的颜色，且当前激活的是这个自定义槽位，绘制高亮选中框
                        if (g_currentColor.GetValue() == customCol.GetValue() && g_activeCustomSlotIndex == slotIdx) {
                            Pen whitePen(Color(255, 255, 255, 255), 2.0f);
                            DrawRoundedRectangle(g, &whitePen, (REAL)cx - 2, (REAL)cy - 2, (REAL)cw + 4, (REAL)ch + 4, 6.0f);

                            Pen blackPen(Color(255, 0, 0, 0), 1.0f);
                            DrawRoundedRectangle(g, &blackPen, (REAL)cx - 1, (REAL)cy - 1, (REAL)cw + 2, (REAL)ch + 2, 5.0f);
                        } else {
                            // 绘制 1px 细外边框
                            Pen borderPen(Color(255, 80, 80, 80), 1.0f);
                            DrawRoundedRectangle(g, &borderPen, (REAL)cx, (REAL)cy, (REAL)cw, (REAL)ch, 4.0f);

                            // 如果是当前选中的槽位（但没有高亮选中边框），绘制小圆点指示器
                            if (g_activeCustomSlotIndex == slotIdx) {
                                SolidBrush dotBrush(Color(180, 255, 255, 255));
                                g.FillEllipse(&dotBrush, cx + cw / 2 - 2, cy + ch / 2 - 2, 4, 4);
                            }
                        }
                    } else {
                        // 12. 自定义设置 "+" 按钮
                        SolidBrush grayBrush(Color(255, 60, 60, 60));
                        FillRoundedRectangle(g, &grayBrush, (REAL)cx, (REAL)cy, (REAL)cw, (REAL)ch, 4.0f);

                        Pen borderPen(Color(255, 80, 80, 80), 1.0f);
                        DrawRoundedRectangle(g, &borderPen, (REAL)cx, (REAL)cy, (REAL)cw, (REAL)ch, 4.0f);

                        // 绘制 "+" 矢量符号
                        Pen plusPen(Color(255, 220, 220, 220), 1.5f);
                        g.DrawLine(&plusPen, cx + 3, cy + 8, cx + cw - 3, cy + 8);
                        g.DrawLine(&plusPen, cx + 8, cy + 3, cx + 8, cy + ch - 3);
                    }
                }
                } // 结束非马赛克模式的颜色行
            }
            if (toolbarAnimState) g.Restore(toolbarAnimState);
        }
    } else {
        // 智能窗口悬停高亮绘制逻辑
        if (g_overlay.hasDetectedWindow) {
            RECT rWin = g_overlay.detectedWindowRect;
            int winW = rWin.right - rWin.left;
            int winH = rWin.bottom - rWin.top;
            if (winW > 0 && winH > 0) {
                // A. 抠图：极速将探测出的悬停窗口部分复原为无遮罩原截屏背景 (零 CPU 混合开销，仅更新区域)
                RECT rcWinFull = { rWin.left, rWin.top, rWin.right, rWin.bottom };
                RECT rcWin;
                if (IntersectRect(&rcWin, &rcWinFull, &pr)) {
                    BitBlt(hMemDC, rcWin.left, rcWin.top, rcWin.right - rcWin.left, rcWin.bottom - rcWin.top,
                           g_hCaptureSourceDC, rcWin.left, rcWin.top, SRCCOPY);
                }

                // B. 品牌蓝描边标记待捕获窗口
                Pen hoverPen(Color(255, 0, 174, 255), 2.0f);
                g.DrawRectangle(&hoverPen, rWin.left, rWin.top, winW, winH);
            }
        } else {
            // 绘制初始化指引提示文字
            wstring hint = IsWindowCaptureMode()
                ? L"单击窗口快速截图，或拖拽改为区域截图    按 Esc 取消"
                : L"拖拽鼠标框选截图区域    按 Esc 取消";
            Font* font = CreateSafeGdiplusFont(L"Microsoft YaHei", 12, FontStyleRegular);
            StringFormat format;
            format.SetAlignment(StringAlignmentCenter);
            format.SetLineAlignment(StringAlignmentCenter);
            
            RectF textBounding;
            g.MeasureString(hint.c_str(), -1, font, PointF(0, 0), &textBounding);
            
            int box_w = (int)textBounding.Width + 40;
            int box_h = (int)textBounding.Height + 20;
            int box_x = w / 2 - box_w / 2;
            int box_y = h / 2 - box_h / 2;
            
            SolidBrush textBgBrush(Color(170, 12, 12, 14));
            FillRoundedRectangle(g, &textBgBrush, (REAL)box_x, (REAL)box_y, (REAL)box_w, (REAL)box_h, 10.0f);
            
            SolidBrush textBrush(Color(180, 255, 255, 255));
            g.DrawString(hint.c_str(), -1, font, RectF((REAL)box_x, (REAL)box_y, (REAL)box_w, (REAL)box_h), &format, &textBrush);
        }
    }
    
    // 7.5 马赛克涂抹笔刷圈：替代十字光标，直观显示覆盖范围（仅屏幕提示，不进入成图）
    if (g_showMosaicCursor && g_annotationMode == ANNOTATION_MOSAIC && g_mosaicShapeMode == 0) {
        REAL br = (REAL)g_mosaicBrush / 2.0f;
        REAL mx = (REAL)g_mosaicCursorPt.x;
        REAL my = (REAL)g_mosaicCursorPt.y;
        Pen ringOutlinePen(Color(200, 0, 0, 0), 3.0f);   // 深色外描边，保证任意底色下可见
        g.DrawEllipse(&ringOutlinePen, mx - br - 1.5f, my - br - 1.5f, (br + 1.5f) * 2.0f, (br + 1.5f) * 2.0f);
        Pen ringPen(Color(235, 255, 255, 255), 1.5f);
        g.DrawEllipse(&ringPen, mx - br, my - br, br * 2.0f, br * 2.0f);
        SolidBrush centerBrush(Color(255, 255, 255, 255));
        g.FillEllipse(&centerBrush, mx - 1.0f, my - 1.0f, 2.0f, 2.0f);
    }

    // 8. 只把本帧更新的矩形贴回屏幕 DC（局部合成）
    BitBlt(hdc, pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top, hMemDC, pr.left, pr.top, SRCCOPY);
}

// ========== 智能窗口框选探测与过滤 API ==========
BOOL IsValidWindow(HWND hWnd) {
    if (!hWnd || hWnd == g_hWndOverlay || hWnd == g_hWndMain || hWnd == g_hWndSettings || hWnd == GetShellWindow() || hWnd == GetDesktopWindow()) {
        return FALSE;
    }
    if (!IsWindowVisible(hWnd)) {
        return FALSE;
    }
    
    // 过滤掉无标题的子窗口或工具提示等（GWL_STYLE 本身未参与判断，省掉一次系统调用）
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

// ========== 长截屏（滚动截屏）==========
// 交互：框选后进入长截屏，覆盖层隐藏、页面恢复可交互，由用户手动滚动页面；
// 程序只被动观察：定时抓取选区实时画面，把当前画面与"已拼接长图"直接做行校验，
// 求出视口在长图坐标系中的绝对位置，只把真正超出已拼内容的部分追加进去。
// 上滑（回看）时视口底部不超过已拼总高 → 数学上不可能产生重复内容。
// 不发送任何输入、不抢焦点、不移动光标。

static Bitmap* CaptureRegionBitmap(const RECT& region) {
    int w = region.right - region.left;
    int h = region.bottom - region.top;
    if (w <= 0 || h <= 0) return NULL;
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hb = CreateCompatibleBitmap(hScreen, w, h);
    HGDIOBJ old = SelectObject(hMem, hb);
    BitBlt(hMem, 0, 0, w, h, hScreen, region.left, region.top, SRCCOPY);
    SelectObject(hMem, old);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    Bitmap* bmp = Bitmap::FromHBITMAP(hb, NULL);
    DeleteObject(hb);
    return bmp;
}

struct LongLockedBmp {
    Bitmap* bmp;
    BitmapData data;
    bool ok;
    int stride;
    const BYTE* base;
};

static bool LockLongBitmap(Bitmap* bmp, LongLockedBmp& lb) {
    lb.bmp = bmp;
    lb.ok = false;
    if (!bmp) return false;
    Rect r(0, 0, (INT)bmp->GetWidth(), (INT)bmp->GetHeight());
    if (bmp->LockBits(&r, ImageLockModeRead, PixelFormat32bppARGB, &lb.data) != Ok) return false;
    lb.ok = true;
    lb.stride = lb.data.Stride;
    lb.base = (const BYTE*)lb.data.Scan0;
    return true;
}

static void UnlockLongBitmap(LongLockedBmp& lb) {
    if (lb.ok) {
        lb.bmp->UnlockBits(&lb.data);
        lb.ok = false;
    }
}

// 行对齐比较：把有效宽度分成 8 个列块，≥6 块逐字节相等即认为两行对齐。
// 容忍局部独立滚动区（如资源管理器左树+右列表双滚动区）、悬停高亮等局部变化——
// 这些只影响 1~2 个列块；而错误偏移会同时破坏内容列块，凑不齐 6 块，假对齐仍被杜绝。
static bool LongRowEqual(const BYTE* a, const BYTE* b, int w, int ignoreSide) {
    int usable = w - ignoreSide * 2;
    if (usable <= 0) return false;
    const int BLOCKS = 8;
    int bs = usable / BLOCKS;
    if (bs <= 0) bs = usable;
    int matched = 0;
    for (int k = 0; k < BLOCKS; ++k) {
        int x0 = ignoreSide + k * bs;
        int x1 = (k == BLOCKS - 1) ? (w - ignoreSide) : (x0 + bs);
        bool eq = true;
        for (int x = x0; x < x1; ++x) {
            const BYTE* pa = a + (size_t)x * 4;
            const BYTE* pb = b + (size_t)x * 4;
            if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2]) { eq = false; break; }
        }
        if (eq) ++matched;
    }
    return matched >= 6;
}

// 行是否"有内容"：与行首存在漂移（渐变/文字行）或存在局部对比度（文字边缘）。
// 纯空白/纯色行两种信号皆无，在任何偏移下都能匹配，不能作为对齐证据。
static bool LongRowInformative(const BYTE* row, int w, int ignoreSide) {
    if (!row) return false;
    const BYTE* p0 = row + (size_t)ignoreSide * 4;
    const BYTE* prev = p0;
    int drift = 0, contrast = 0;
    for (int x = ignoreSide + 2; x < w - ignoreSide; x += 2) {
        const BYTE* p = row + (size_t)x * 4;
        int dp = abs(p[0] - p0[0]) + abs(p[1] - p0[1]) + abs(p[2] - p0[2]);
        int dc = abs(p[0] - prev[0]) + abs(p[1] - prev[1]) + abs(p[2] - prev[2]);
        if (dp > 30) ++drift;
        if (dc > 30) ++contrast;
        if (drift >= 2 || contrast >= 3) return true;
        prev = p;
    }
    return false;
}

// 从条带池末尾修剪 count 行（ShareX：每轮先把画布底部"可疑区"剪掉再由当前帧重写）
static void TrimLongRows(int count) {
    while (count > 0 && !g_longStrips.empty()) {
        int last = (int)g_longStrips.size() - 1;
        int take = min(count, g_longStripHeights[last]);
        g_longStripHeights[last] -= take;
        count -= take;
        if (g_longStripHeights[last] == 0) {
            delete g_longStrips[last];
            g_longStrips.pop_back();
            g_longStripHeights.pop_back();
        }
    }
}

// 把 cur 的 [srcY0, srcY0+cnt) 行追加进条带池（末条未满则续写，满了开新条）
static bool AppendLongRows(Bitmap* cur, int srcY0, int cnt) {
    if (!cur || cnt <= 0) return false;
    while (cnt > 0) {
        int last = (int)g_longStrips.size() - 1;
        if (last < 0 || g_longStripHeights[last] >= LONG_STRIP_H) {
            Bitmap* s = new Bitmap(g_longRegionWidth, LONG_STRIP_H, PixelFormat32bppARGB);
            if (!s || s->GetLastStatus() != Ok) { delete s; return false; }
            g_longStrips.push_back(s);
            g_longStripHeights.push_back(0);
            last = (int)g_longStrips.size() - 1;
        }
        int filled = g_longStripHeights[last];
        int n = min(cnt, LONG_STRIP_H - filled);
        {
            Graphics gs(g_longStrips[last]);
            gs.DrawImage(cur, RectF(0.0f, (REAL)filled, (REAL)g_longRegionWidth, (REAL)n),
                         RectF(0.0f, (REAL)srcY0, (REAL)g_longRegionWidth, (REAL)n), UnitPixel);
        }
        g_longStripHeights[last] = filled + n;
        srcY0 += n;
        cnt -= n;
    }
    return true;
}

// 面板双缓冲（文件级静态，WM_DESTROY 释放）
static HDC g_longPanelMemDC = NULL;
static HBITMAP g_longPanelMemBmp = NULL;
static int g_longPanelMemW = 0, g_longPanelMemH = 0;

static RECT LongBtnDoneRect() {
    int by = g_longPanelH - 46;
    RECT rc = { g_longPanelW - 102, by, g_longPanelW - 12, by + 34 };
    return rc;
}

static RECT LongBtnCancelRect() {
    int by = g_longPanelH - 46;
    RECT rc = { g_longPanelW - 202, by, g_longPanelW - 112, by + 34 };
    return rc;
}

// 面板预览：整张长图按当前缩放等比绘制（顶对齐、水平居中），
// 缩放随长图增高平滑收敛到"刚好放得下"，即"慢慢变细长"的动画效果
static void DrawLongPreviewPanel(Graphics& g, const RECT& pv) {
    SolidBrush bgBrush(Color(255, 16, 16, 18));
    g.FillRectangle(&bgBrush, Rect(pv.left, pv.top, pv.right - pv.left, pv.bottom - pv.top));
    if (g_longTotalH <= 0 || g_longStrips.empty()) return;

    int pvW = pv.right - pv.left;
    int pvH = pv.bottom - pv.top;
    if (pvW <= 0 || pvH <= 0) return;
    float target = min((float)pvW / (float)g_longRegionWidth, (float)pvH / (float)g_longTotalH);
    if (target > 1.0f) target = 1.0f;
    if (g_longPanelScale < 0.0f) g_longPanelScale = target;
    g_longPanelScale += (target - g_longPanelScale) * 0.22f;
    if (fabs(target - g_longPanelScale) < 0.0015f) g_longPanelScale = target;

    float scale = g_longPanelScale;
    float dispW = (float)g_longRegionWidth * scale;
    float x = (REAL)pv.left + ((REAL)pvW - dispW) / 2.0f;
    g.SetClip(Rect(pv.left, pv.top, pvW, pvH));
    float y = (REAL)pv.top;
    for (size_t i = 0; i < g_longStrips.size(); ++i) {
        float shPix = (float)g_longStripHeights[i] * scale;
        g.DrawImage(g_longStrips[i], RectF(x, y, dispW, shPix),
                    RectF(0.0f, 0.0f, (REAL)g_longRegionWidth, (REAL)g_longStripHeights[i]), UnitPixel);
        y += shPix;
    }
    g.ResetClip();
    Pen borderPen(Color(255, 70, 70, 76), 1.0f);
    g.DrawRectangle(&borderPen, RectF(x, (REAL)pv.top, dispW, (float)g_longTotalH * scale));
}

static void FinishLongCapture();
static void CancelLongCapture();

// 观察步进（ShareX ScrollingCapture::CombineImages 的手动滚动适配版）：
// 每轮把画布底部 ignoreBottom 行视为"可疑区"，以可疑区上沿一行为锚，
// 在当前帧中自底向上寻找与锚点对齐的最长连续严格相等行段；
// 视口底越过画布底时，剪掉可疑区、把当前帧锚点以下的行重写进去。
// 上滑/静止时视口底不超过画布底，整步跳过——重复内容在结构上无法被拼入。
// 不注入任何输入、不抢焦点、不移动光标。
// 在 reference 帧（h 行）与 cur 帧之间做 ShareX 式锚定匹配：
// reference 的 anchorY 行为锚，在 cur 中找最长连续严格相等行段。
// 找到时返回 true：cur 的 bestCy 行与 reference 的 anchorY 行对齐，
// 内容上移量 d = anchorY - bestCy（>0 为下滚），secondRun 为排除邻近后的次优峰值。
typedef const BYTE* (*LongRowGetter)(const void* ctx, int y);

struct LongCanvasCtx {
    LongLockedBmp* locks;
    Bitmap** strips;
    int totalH;
    int lockCount;
};

static const BYTE* LongCanvasRowGet(const void* ctx, int y) {
    const LongCanvasCtx* c = (const LongCanvasCtx*)ctx;
    if (y < 0 || y >= c->totalH) return NULL;
    int idx = y / LONG_STRIP_H;
    if (idx >= c->lockCount) return NULL;
    LongLockedBmp& lb = c->locks[idx];
    // 惰性锁定：只有扫描真正触碰到该条带才锁（每轮通常只需底部 2~3 条）
    if (!lb.ok && !LockLongBitmap(c->strips[idx], lb)) return NULL;
    return lb.base + (size_t)(y % LONG_STRIP_H) * lb.stride;
}

struct LongFrameCtx {
    const BYTE* base;
    int stride;
    int height;
};

static const BYTE* LongFrameRowGet(const void* ctx, int y) {
    const LongFrameCtx* c = (const LongFrameCtx*)ctx;
    if (y < 0 || y >= c->height) return NULL;
    return c->base + (size_t)y * c->stride;
}

static bool LongAlignFrames(LongRowGetter refGet, const void* refCtx,
                            LongRowGetter curGet, const void* curCtx,
                            int w, int h, int anchorY, int ignoreSide,
                            int& bestCy, int& bestRun, int& secondRun) {
    bestCy = -1;
    bestRun = 0;
    secondRun = 0;
    int matchLimit = h / 2;
    int cmpBytes = (w - ignoreSide * 2) * 4;
    if (anchorY < 0 || cmpBytes <= 0) return false;

    std::vector<int> runOf(h, 0);
    long long budget = 64LL << 20; // 比较成本预算，防极端重复内容拖垮轮询
    for (int cy = h - 1; cy >= 0; --cy) {
        if (bestRun >= matchLimit) break; // 已达上限，不可能更长
        int run = 0;
        for (int y = 0; cy - y >= 0 && anchorY - y >= 0 && run < matchLimit; ++y) {
            budget -= cmpBytes;
            if (budget < 0) break;
            const BYTE* a = refGet(refCtx, anchorY - y);
            const BYTE* b = curGet(curCtx, cy - y);
            if (!a || !b || !LongRowEqual(a, b, w, ignoreSide)) break;
            ++run;
        }
        if (budget < 0) break;
        runOf[cy] = run;
        if (run > bestRun) {
            bestRun = run;
            bestCy = cy;
        }
    }
    if (bestCy < 0) return false;
    // 次优峰值：排除最优位置邻近（±6 行）后的最长行段
    for (int cy = 0; cy < h; ++cy) {
        if (abs(cy - bestCy) > 6 && runOf[cy] > secondRun) secondRun = runOf[cy];
    }
    return true;
}

// 追加判据：连续相等行段 ≥8 行；正常滚动要求长重叠（≥h/8），快速滚动时锚点内容
// 可能即将滚出视野、重叠变短，此时只要最优峰值显著高于次优（≥2 倍且 ≥24 行）仍予采信，
// 否则锚点一但失效将永久冻结拼接；重复内容上的假对齐会被"次优接近最优"否决。
static bool LongMatchStrong(int bestRun, int secondRun, int h) {
    if (bestRun < 8) return false;
    if (bestRun >= h / 8) return true;
    return bestRun >= 24 && bestRun >= 2 * secondRun;
}

// 检查匹配段是否包含至少一行"有内容"的行（防止纯空白区域的假对齐）
static bool LongRunInformativeCheck(LongRowGetter curGet, const void* curCtx,
                                    int bestCy, int bestRun, int w, int ignoreSide) {
    for (int r = bestCy; r > bestCy - bestRun && r >= 0; --r) {
        if (LongRowInformative(curGet(curCtx, r), w, ignoreSide)) return true;
    }
    return false;
}

static void FinishLongStepTail(HWND hPanel, Bitmap* cap, bool appended,
                               int diagExt, int diagRun, int diagD, int diagPath);

// 诊断：抓屏落盘（默认关闭，设置环境变量 SNAPCAPTURE_DIAG=1 开启，用于问题定位）
static int g_longDiagFrameNo = 0;
static void LongDiagSaveFrame(Bitmap* cap, bool appended) {
    wchar_t env[8];
    if (GetEnvironmentVariableW(L"SNAPCAPTURE_DIAG", env, 8) == 0 || env[0] != L'1') return;
    if (!cap || g_longDiagFrameNo >= 120) return;
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%sSnapCapture_diag_%04d%s.png", dir, g_longDiagFrameNo,
               appended ? L"_plus" : L"");
    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) == -1) return;
    cap->Save(path, &clsid, NULL);
    g_longDiagFrameNo++;
}

// 帧校验和（采样求和）：诊断行显示它即可判断"抓屏是否在变化"
static int LongFrameChecksum(Bitmap* cap) {
    LongLockedBmp lb;
    if (!LockLongBitmap(cap, lb)) return -1;
    int h = cap->GetHeight(), w = cap->GetWidth();
    long long sum = 0;
    for (int y = 0; y < h; y += 16) {
        const BYTE* row = lb.base + (size_t)y * lb.stride;
        for (int x = 0; x < w; x += 8) {
            const BYTE* p = row + (size_t)x * 4;
            sum += p[0] + p[1] + p[2];
        }
    }
    UnlockLongBitmap(lb);
    return (int)(sum % 100000);
}

// 观察步进（ShareX ScrollingCapture::CombineImages 的手动滚动适配版）：
// 主路径：把画布底部 ignoreBottom 行视为"可疑区"，以可疑区上沿为锚在当前帧中找
// 最长连续严格相等行段；视口底越过画布底时剪掉可疑区、用当前帧锚点以下的行重写。
// 中继路径：快速滚动使锚点滚出视野时，改与"上一帧"锚定匹配求出本次滚动位移续拼。
// 上滑/静止时视口底不超过画布底，整步跳过——重复内容在结构上无法被拼入。
// 不注入任何输入、不抢焦点、不移动光标。
static void LongStep(HWND hPanel) {
    if (!g_longModeActive || g_longRegionWidth <= 0 || g_longRegionHeight <= 0) return;
    if (g_longStrips.empty()) return;
    int w = g_longRegionWidth;
    int h = g_longRegionHeight;

    Bitmap* cap = CaptureRegionBitmap(g_longRegion);
    if (!cap) return;

    LongLockedBmp lc;
    LongFrameCtx curCtx;
    bool ok = LockLongBitmap(cap, lc);
    LongCanvasCtx canvasCtx;
    std::vector<LongLockedBmp> locks; // 惰性锁定槽位：与条带一一对应，扫描触碰才真正锁
    if (ok) {
        curCtx.base = lc.base;
        curCtx.stride = lc.stride;
        curCtx.height = h;
        locks.resize(g_longStrips.size());
        canvasCtx.locks = locks.data();
        canvasCtx.totalH = g_longTotalH;
        canvasCtx.lockCount = (int)locks.size();
        canvasCtx.strips = g_longStrips.data();
    }
    if (!ok) {
        UnlockLongBitmap(lc);
        for (size_t i = 0; i < locks.size(); ++i) UnlockLongBitmap(locks[i]);
        delete cap;
        return;
    }

    // ShareX 同款参数：排除左右边距（滚动条等），底部基准修剪量
    int ignoreSide = max(50, w / 20);
    if (ignoreSide > w / 3) ignoreSide = w / 3;
    int base = max(50, h / 10);
    if (base > h / 3) base = h / 3;
    int cmpBytes = (w - ignoreSide * 2) * 4;

    bool appended = false;

    // ---- 主路径：与画布底部锚定匹配 ----
    int ext = 0;
    int extMax = h / 3;
    while (ext < extMax && ext < g_longTotalH) {
        const BYTE* a = LongCanvasRowGet(&canvasCtx, g_longTotalH - 1 - ext);
        const BYTE* b = LongFrameRowGet(&curCtx, h - 1 - ext);
        if (!a || !b || !LongRowEqual(a, b, w, ignoreSide)) break;
        ++ext;
    }
    int ignoreBottom = base + ext;
    if (ignoreBottom > h / 3) ignoreBottom = h / 3;
    if (ignoreBottom >= g_longTotalH) ignoreBottom = g_longTotalH - 1;
    int anchorY = g_longTotalH - ignoreBottom - 1;

    int bestCy = -1, bestRun = 0, secondRun = 0;
    if (LongAlignFrames(LongCanvasRowGet, &canvasCtx, LongFrameRowGet, &curCtx,
                        w, h, anchorY, ignoreSide, bestCy, bestRun, secondRun)) {
        int matchHeight = h - 1 - bestCy;
        if (LongMatchStrong(bestRun, secondRun, h) && matchHeight > ignoreBottom &&
            LongRunInformativeCheck(LongFrameRowGet, &curCtx, bestCy, bestRun, w, ignoreSide)) {
            int newTotal = g_longTotalH - ignoreBottom + matchHeight;
            UnlockLongBitmap(lc);
            for (size_t i = 0; i < locks.size(); ++i) UnlockLongBitmap(locks[i]);
            TrimLongRows(ignoreBottom);
            g_longTotalH -= ignoreBottom; // 先同步修剪后的真实高度，追加失败也不会越界
            if (AppendLongRows(cap, bestCy + 1, matchHeight)) {
                g_longTotalH = newTotal;
                appended = true;
            }
            FinishLongStepTail(hPanel, cap, appended, ext, bestRun, matchHeight - ignoreBottom, 1);
            return;
        }
    }

    // ---- 中继路径：与上一帧锚定匹配（主路径失败时的快速滚动自救）----
    // 画布尾部恒等于上一帧（不变式），因此上一帧 row0 的画布位置 = totalH - h。
    if (g_longPrevValid && g_longPrevCap) {
        LongLockedBmp lp;
        LongFrameCtx prevCtx;
        if (LockLongBitmap(g_longPrevCap, lp)) {
            prevCtx.base = lp.base;
            prevCtx.stride = lp.stride;
            prevCtx.height = h;
            int extP = 0;
            while (extP < extMax) {
                const BYTE* a = LongFrameRowGet(&prevCtx, h - 1 - extP);
                const BYTE* b = LongFrameRowGet(&curCtx, h - 1 - extP);
                if (!a || !b || !LongRowEqual(a, b, w, ignoreSide)) break;
                ++extP;
            }
            int t = base + extP;
            if (t > h / 3) t = h / 3;
            int anchorP = h - t - 1;
            if (LongAlignFrames(LongFrameRowGet, &prevCtx, LongFrameRowGet, &curCtx,
                                w, h, anchorP, ignoreSide, bestCy, bestRun, secondRun)) {
                int d = anchorP - bestCy; // 本次轮询间隔内的下滚量
                int matchHeight = h - 1 - bestCy;
                if (d >= 1 && LongMatchStrong(bestRun, secondRun, h) && matchHeight > t &&
                    LongRunInformativeCheck(LongFrameRowGet, &curCtx, bestCy, bestRun, w, ignoreSide)) {
                    int newTotal = g_longTotalH - t + matchHeight; // = totalH + d
                    UnlockLongBitmap(lc);
                    UnlockLongBitmap(lp);
                    TrimLongRows(t);
                    g_longTotalH -= t; // 先同步修剪后的真实高度，追加失败也不会越界
                    if (AppendLongRows(cap, bestCy + 1, matchHeight)) {
                        g_longTotalH = newTotal;
                        appended = true;
                    }
                    FinishLongStepTail(hPanel, cap, appended, extP, bestRun, d, 2);
                    return;
                }
            }
        }
    }

    UnlockLongBitmap(lc);
    for (size_t i = 0; i < locks.size(); ++i) UnlockLongBitmap(locks[i]);
    FinishLongStepTail(hPanel, cap, false, ext, bestRun, bestCy >= 0 ? anchorY - bestCy : 0, 0);
}

// LongStep 收尾：释放本轮捕获（所有权转移给 prev 帧）、记录诊断、刷新面板
static void FinishLongStepTail(HWND hPanel, Bitmap* cap, bool appended,
                               int diagExt, int diagRun, int diagD, int diagPath) {
    g_longDiagExt = diagExt;
    g_longDiagRun = diagRun;
    g_longDiagD = diagD;
    g_longDiagApp = appended ? 1 : 0;
    g_longDiagPath = diagPath;
    g_longDiagSum = LongFrameChecksum(cap);
    LongDiagSaveFrame(cap, appended);
    if (g_longPrevCap) delete g_longPrevCap;
    g_longPrevCap = cap;          // 接管所有权
    g_longPrevValid = appended;   // 只有成功续拼后画布尾部才与 prev 帧对齐
    if (g_longTotalH >= 15000) {
        FinishLongCapture(); // 高度上限保护
        return;
    }
    if (appended) g_longStallCount = 0;
    else ++g_longStallCount;
    // 自适应轮询：走中继说明用户在快速滚动，提速到 60ms 减少帧丢失；恢复正常后回到 120ms
    int want = (diagPath == 2) ? 60 : 120;
    if (want != g_longPollMs) {
        g_longPollMs = want;
        if (g_hWndLongPanel) SetTimer(g_hWndLongPanel, 1, want, NULL);
    }
    if (g_hWndLongPanel) InvalidateRect(hPanel, NULL, FALSE);
}

LRESULT CALLBACK LongPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            // 面板双缓冲（文件级静态，WM_DESTROY 时释放）
            if (!g_longPanelMemDC || g_longPanelMemW != rc.right || g_longPanelMemH != rc.bottom) {
                if (g_longPanelMemBmp) { DeleteObject(g_longPanelMemBmp); g_longPanelMemBmp = NULL; }
                if (g_longPanelMemDC) { DeleteDC(g_longPanelMemDC); g_longPanelMemDC = NULL; }
                HDC hScreen = GetDC(hWnd);
                g_longPanelMemDC = CreateCompatibleDC(hScreen);
                g_longPanelMemBmp = CreateCompatibleBitmap(hScreen, rc.right, rc.bottom);
                SelectObject(g_longPanelMemDC, g_longPanelMemBmp);
                ReleaseDC(hWnd, hScreen);
                g_longPanelMemW = rc.right;
                g_longPanelMemH = rc.bottom;
            }
            bool animating = false;
            {
                Graphics g(g_longPanelMemDC);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                SolidBrush bg(Color(255, 24, 24, 28));
                g.FillRectangle(&bg, Rect(0, 0, rc.right, rc.bottom));
                Font* titleFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 11, FontStyleBold);
                SolidBrush white(Color(255, 240, 240, 244));
                g.DrawString(L"滚动截屏", -1, titleFont, RectF(12, 8, 130, 24), NULL, &white);

                Font* hintFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 9, FontStyleRegular);
                SolidBrush hintBrush(Color(255, 150, 150, 158));
                wstring hint = (g_longStallCount >= 20)
                    ? L"似乎已到底部，点完成即可"
                    : L"在页面上滚动即可拼接";
                g.DrawString(hint.c_str(), -1, hintFont, RectF(12, 32, 200, 20), NULL, &hintBrush);

                // 诊断行：默认隐藏，按住 Shift 显示（e=底部固定区 r=匹配行段 d=下滚 c=帧校验和）
                if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && g_longDiagRun >= 0) {
                    wstring diag = L"e" + ToWString(g_longDiagExt) + L" r" + ToWString(g_longDiagRun) +
                                   L" d" + ToWString(g_longDiagD) + L" c" + ToWString(g_longDiagSum) +
                                   (g_longDiagApp ? L" +" : L" -");
                    StringFormat sfDiag;
                    sfDiag.SetAlignment(StringAlignmentFar);
                    g.DrawString(diag.c_str(), -1, hintFont, RectF((REAL)rc.right - 152, 32, 140, 20), &sfDiag, &hintBrush);
                }

                Font* statFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 9, FontStyleRegular);
                SolidBrush statBrush(Color(255, 150, 150, 158));
                wstring stat = ToWString(g_longTotalH) + L" px";
                StringFormat sfR;
                sfR.SetAlignment(StringAlignmentFar);
                sfR.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(stat.c_str(), -1, statFont, RectF((REAL)rc.right - 90, 8, 78, 24), &sfR, &statBrush);

                RECT pv = { 12, 56, g_longPanelW - 12, g_longPanelH - 52 };
                DrawLongPreviewPanel(g, pv);
                animating = (g_longPanelScale >= 0.0f) && (g_longTotalH > 0) &&
                            (fabs(((float)min((float)(pv.right - pv.left) / (float)g_longRegionWidth,
                                              (float)(pv.bottom - pv.top) / (float)g_longTotalH)) - g_longPanelScale) > 0.0015f);

                RECT rDone = LongBtnDoneRect();
                RECT rCancel = LongBtnCancelRect();
                SolidBrush doneBrush(Color(255, 46, 204, 113));
                FillRoundedRectangle(g, &doneBrush, (REAL)rDone.left, (REAL)rDone.top,
                                     (REAL)(rDone.right - rDone.left), (REAL)(rDone.bottom - rDone.top), 6.0f);
                SolidBrush cancelBrush(Color(255, 60, 60, 66));
                FillRoundedRectangle(g, &cancelBrush, (REAL)rCancel.left, (REAL)rCancel.top,
                                     (REAL)(rCancel.right - rCancel.left), (REAL)(rCancel.bottom - rCancel.top), 6.0f);
                Font* btnFont = CreateSafeGdiplusFont(L"Microsoft YaHei", 10, FontStyleBold);
                StringFormat sfC;
                sfC.SetAlignment(StringAlignmentCenter);
                sfC.SetLineAlignment(StringAlignmentCenter);
                SolidBrush btnText(Color(255, 255, 255, 255));
                g.DrawString(L"完成", -1, btnFont, RectF(rDone.left, rDone.top, rDone.right - rDone.left, rDone.bottom - rDone.top), &sfC, &btnText);
                g.DrawString(L"取消", -1, btnFont, RectF(rCancel.left, rCancel.top, rCancel.right - rCancel.left, rCancel.bottom - rCancel.top), &sfC, &btnText);
            }
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_longPanelMemDC, 0, 0, SRCCOPY);
            EndPaint(hWnd, &ps);
            // 预览缩小动画：自驱动泵，收敛即停
            if (animating) InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            if (wParam == 1 && g_longModeActive) LongStep(hWnd);
            break;
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rDone = LongBtnDoneRect();
            RECT rCancel = LongBtnCancelRect();
            if (PtInRect(&rDone, pt)) FinishLongCapture();
            else if (PtInRect(&rCancel, pt)) CancelLongCapture();
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) CancelLongCapture();
            break;
        case WM_DESTROY:
            if (g_hWndLongPanel == hWnd) g_hWndLongPanel = NULL;
            KillTimer(hWnd, 1);
            if (g_longPanelMemBmp) { DeleteObject(g_longPanelMemBmp); g_longPanelMemBmp = NULL; }
            if (g_longPanelMemDC) { DeleteDC(g_longPanelMemDC); g_longPanelMemDC = NULL; }
            g_longPanelMemW = 0;
            g_longPanelMemH = 0;
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static void CreateLongPanel() {
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = LongPanelProc;
        wc.hInstance = g_hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"CaptureToolLongPanelClass";
        RegisterClassEx(&wc);
        s_registered = true;
    }
    g_longPreviewH = min(g_longRegionHeight, 470);
    if (g_longPreviewH < 80) g_longPreviewH = 80;
    g_longPanelW = 320;
    g_longPanelH = g_longPreviewH + 108;
    g_longPanelScale = -1.0f; // 动画从初始适配比例开始
    // 面板位置：右侧 → 左侧 → 选区下方 → 选区上方，全部放不下才允许贴屏幕右缘。
    // 面板绝不能压在捕获区域内（静态像素会进入每一次抓屏，彻底破坏行对齐）。
    int sx = g_screenX, sy = g_screenY, sw = g_screenWidth, sh = g_screenHeight;
    int px, py;
    if (g_longRegion.right + 12 + g_longPanelW <= sx + sw) {
        px = g_longRegion.right + 12;
        py = max(sy + 8, min((int)g_longRegion.top, sy + sh - g_longPanelH - 8));
    } else if (g_longRegion.left - 12 - g_longPanelW >= sx) {
        px = g_longRegion.left - 12 - g_longPanelW;
        py = max(sy + 8, min((int)g_longRegion.top, sy + sh - g_longPanelH - 8));
    } else if (g_longRegion.bottom + 12 + g_longPanelH <= sy + sh) {
        py = g_longRegion.bottom + 12;
        px = max(sx + 8, min((int)g_longRegion.left, sx + sw - g_longPanelW - 8));
    } else if (g_longRegion.top - 12 - g_longPanelH >= sy) {
        py = g_longRegion.top - 12 - g_longPanelH;
        px = max(sx + 8, min((int)g_longRegion.left, sx + sw - g_longPanelW - 8));
    } else {
        px = sx + sw - g_longPanelW - 8;
        py = sy + sh - g_longPanelH - 8;
    }
    g_hWndLongPanel = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"CaptureToolLongPanelClass",
                                     L"长截屏", WS_POPUP, px, py, g_longPanelW, g_longPanelH,
                                     NULL, NULL, g_hInstance, NULL);
    if (!g_hWndLongPanel) return;
    // 关键：让面板对一切屏幕抓取不可见（Win10 2004+ WDA_EXCLUDEFROMCAPTURE）。
    // 面板在屏幕上正常显示、可交互，但 BitBlt 抓屏看不到它——
    // 即使面板与选区重叠（全屏宽选区时不可避免），拼接也不会被面板静态像素污染。
    typedef BOOL(WINAPI* PFN_SetWindowDisplayAffinity)(HWND, DWORD);
    PFN_SetWindowDisplayAffinity pfnAffinity = (PFN_SetWindowDisplayAffinity)
        GetProcAddress(GetModuleHandle(L"user32.dll"), "SetWindowDisplayAffinity");
    if (pfnAffinity) pfnAffinity(g_hWndLongPanel, WDA_EXCLUDEFROMCAPTURE);
    ShowWindow(g_hWndLongPanel, SW_SHOW);
    g_longPollMs = 120;
    SetTimer(g_hWndLongPanel, 1, g_longPollMs, NULL);
}

static void FinishLongCapture() {
    if (!g_longModeActive) return;
    g_longModeActive = false;
    if (g_hWndLongPanel) KillTimer(g_hWndLongPanel, 1);
    bool stitched = false;
    if (g_longTotalH > 0 && !g_longStrips.empty()) {
        Bitmap* finalBmp = new Bitmap(g_longRegionWidth, g_longTotalH, PixelFormat32bppARGB);
        if (finalBmp && finalBmp->GetLastStatus() == Ok) {
            Graphics gf(finalBmp);
            int y = 0;
            // 边拼接边释放条带：内存峰值从"条带池+成品"双份降为"成品+单条带"。
            // 分配成品在前、消耗条带在后，成品分配失败时条带池原样保留（会话可恢复）。
            while (!g_longStrips.empty()) {
                gf.DrawImage(g_longStrips[0], Rect(0, y, g_longRegionWidth, g_longStripHeights[0]),
                             0, 0, g_longRegionWidth, g_longStripHeights[0], UnitPixel);
                y += g_longStripHeights[0];
                delete g_longStrips[0];
                g_longStrips.erase(g_longStrips.begin());
                g_longStripHeights.erase(g_longStripHeights.begin());
            }
            if (g_longFinalBmp) delete g_longFinalBmp;
            g_longFinalBmp = finalBmp;
            // 成品尺寸必须在 FreeLongStrips 清零 totalH 之前保存
            g_longFinalW = g_longRegionWidth;
            g_longFinalH = g_longTotalH;
            stitched = true;
        } else {
            delete finalBmp;
        }
    }
    if (g_hWndLongPanel) { DestroyWindow(g_hWndLongPanel); }
    g_hWndLongPanel = NULL;
    FreeLongStrips();
    if (!stitched) {
        // 没有可交付内容：恢复原会话
        if (g_hWndOverlay) {
            ShowWindow(g_hWndOverlay, SW_SHOW);
            SetForegroundWindow(g_hWndOverlay);
            InvalidateRect(g_hWndOverlay, NULL, FALSE);
        }
        return;
    }
    // 框内预览位图（一次性高质量缩放，后续绘制零开销）
    // 注意：FreeLongStrips 已把 g_longTotalH 清零，这里必须用释放前保存的成品尺寸
    if (g_longFinalH <= 0 || g_longFinalW <= 0) {
        if (g_hWndOverlay) {
            ShowWindow(g_hWndOverlay, SW_SHOW);
            SetForegroundWindow(g_hWndOverlay);
            InvalidateRect(g_hWndOverlay, NULL, FALSE);
        }
        return;
    }
    double scale = min((double)(g_screenWidth - 160) / g_longFinalW,
                       (double)(g_screenHeight - 220) / g_longFinalH);
    if (scale > 1.0) scale = 1.0;
    if (scale < 0.02) scale = 0.02;
    int vw = max(1, (int)(g_longFinalW * scale));
    int vh = max(1, (int)(g_longFinalH * scale));
    Bitmap* view = new Bitmap(vw, vh, PixelFormat32bppARGB);
    if (view && view->GetLastStatus() == Ok) {
        Graphics gv(view);
        gv.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gv.DrawImage(g_longFinalBmp, RectF(0, 0, (REAL)vw, (REAL)vh),
                     RectF(0, 0, (REAL)g_longFinalW, (REAL)g_longFinalH), UnitPixel);
        if (g_longViewBmp) delete g_longViewBmp;
        g_longViewBmp = view;
    } else {
        delete view;
    }
    if (g_longFinalHbmp) { DeleteObject(g_longFinalHbmp); g_longFinalHbmp = NULL; }
    g_longFinalBmp->GetHBITMAP(Color(255, 255, 255, 255), &g_longFinalHbmp);
    delete g_longFinalBmp;      // HBITMAP 已独立持有全部像素，GDI+ 成品即刻释放
    g_longFinalBmp = NULL;
    // 长图等比缩放放进屏幕中央的框内
    int vx = (g_screenWidth - vw) / 2;
    int vy = 40;
    g_longViewRect = { vx, vy, vx + vw, vy + vh };
    g_overlay.selection = g_longViewRect;
    g_longPreviewActive = true;
    g_annotationMode = ANNOTATION_NONE;
    g_selectedTextIndex = -1;
    g_shapes.clear();
    g_redoStack.clear();
    if (g_hWndOverlay) {
        ShowWindow(g_hWndOverlay, SW_SHOW);
        SetForegroundWindow(g_hWndOverlay);
        InvalidateRect(g_hWndOverlay, NULL, FALSE);
    }
}

static void CancelLongCapture() {
    if (!g_longModeActive) return;
    g_longModeActive = false;
    if (g_hWndLongPanel) {
        KillTimer(g_hWndLongPanel, 1);
        DestroyWindow(g_hWndLongPanel);
    }
    g_hWndLongPanel = NULL;
    FreeLongStrips();
    if (g_hWndOverlay) {
        ShowWindow(g_hWndOverlay, SW_SHOW);
        SetForegroundWindow(g_hWndOverlay);
        InvalidateRect(g_hWndOverlay, NULL, FALSE);
    }
}

static void EnterScrollCapture(HWND hWnd) {
    if (!g_overlay.selectionDone || g_longModeActive || g_longPreviewActive) return;
    int rw = g_overlay.selection.right - g_overlay.selection.left;
    int rh = g_overlay.selection.bottom - g_overlay.selection.top;
    if (rw < 60 || rh < 60) return; // 区域太小不适合滚动截屏
    CommitEditingText();
    g_selectedTextIndex = -1;
    g_longRegionLocal = g_overlay.selection;
    g_longRegion = RECT{ g_overlay.selection.left + g_screenX, g_overlay.selection.top + g_screenY,
                         g_overlay.selection.right + g_screenX, g_overlay.selection.bottom + g_screenY };
    g_longRegionWidth = rw;
    g_longRegionHeight = rh;
    // 初始内容取自冻结截图（覆盖层此刻正显示它，与实时画面一致），灌入条带池
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hb = CreateCompatibleBitmap(hScreen, rw, rh);
    HGDIOBJ old = SelectObject(hMem, hb);
    BitBlt(hMem, 0, 0, rw, rh, g_hCaptureSourceDC, g_overlay.selection.left, g_overlay.selection.top, SRCCOPY);
    SelectObject(hMem, old);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    Bitmap* init = Bitmap::FromHBITMAP(hb, NULL);
    DeleteObject(hb);
    if (!init || init->GetLastStatus() != Ok) {
        delete init;
        return;
    }
    if (!AppendLongRows(init, 0, rh)) {
        delete init;
        return;
    }
    delete init;
    g_longTotalH = rh;
    g_longDiagFrameNo = 0; // 每次长截屏会话重新编号诊断抓屏
    // 隐藏覆盖层让页面恢复可交互，显示悬浮面板；全程不注入输入、不抢焦点、不动光标
    g_longModeActive = true;
    ShowWindow(g_hWndOverlay, SW_HIDE);
    // prev 帧 = 覆盖层隐藏后的实时画面（避免把选区边框/手柄烘焙进对齐参考帧）
    if (g_longPrevCap) delete g_longPrevCap;
    g_longPrevCap = CaptureRegionBitmap(g_longRegion);
    g_longPrevValid = (g_longPrevCap && g_longPrevCap->GetLastStatus() == Ok);
    if (!g_longPrevValid) {
        delete g_longPrevCap;
        g_longPrevCap = NULL;
        g_longModeActive = false;
        FreeLongStrips();
        ShowWindow(g_hWndOverlay, SW_SHOW);
        InvalidateRect(g_hWndOverlay, NULL, FALSE);
        return;
    }
    CreateLongPanel();
    if (!g_hWndLongPanel) {
        g_longModeActive = false;
        FreeLongStrips();
        ShowWindow(g_hWndOverlay, SW_SHOW);
        InvalidateRect(g_hWndOverlay, NULL, FALSE);
        return;
    }
}

// ========== 全屏 Overlay 覆盖画布窗口过程 WndProc ==========
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // 计算本帧需要重绘的矩形：动画期间工具栏整体位移，强制整屏；其余按失效区域局部合成
            if (g_toolbarAnimStart != 0) {
                RECT rcFull;
                GetClientRect(hWnd, &rcFull);
                g_paintRect = rcFull;
            } else {
                g_paintRect = ps.rcPaint;
            }
            DrawOverlay(hWnd, hdc);
            EndPaint(hWnd, &ps);
            // 动画/滑条拖动期间自驱动连续重绘：帧率不受定时器粒度限制，直接拉满
            if (g_toolbarAnimStart != 0) {
                InvalidateRect(hWnd, NULL, FALSE); // 动画：整屏
            } else if (g_mosaicSliderDrag != 0) {
                // 滑条拖动：只失效滑条区域与笔刷圈，亚毫秒一帧
                InvalidateRect(hWnd, &g_sliderStripRect, FALSE);
                if (g_showMosaicCursor) {
                    RECT rcCircle = MosaicCursorRect(g_mosaicCursorPt);
                    InvalidateRect(hWnd, &rcCircle, FALSE);
                }
            }
            break;
        }
        case WM_SETCURSOR: {
            return TRUE;
        }
        case WM_MOUSEMOVE: {
            POINT pt;
            pt.x = (short)LOWORD(lParam);   // 符号扩展：副屏在主屏左侧/上方时客户区坐标为负
            pt.y = (short)HIWORD(lParam);

            // 笔刷圈默认隐藏，仅涂抹马赛克的指定路径重新打开
            g_showMosaicCursor = false;

            if (g_mosaicSliderDrag == 1) {
                // 拖粗细滑条：笔刷宽度只影响新笔迹，即时生效零开销；圈随数值实时变化
                // （重绘由泵式局部失效与末尾的圈失效统一驱动）
                g_mosaicBrush = SliderValueFromX(pt.x, g_sliderTrackX, 90, MOSAIC_BRUSH_MIN, MOSAIC_BRUSH_MAX);
                g_showMosaicCursor = true;
                g_mosaicCursorPt = pt;
            }
            else if (g_mosaicSliderDrag == 2) {
                // 拖质量滑条：值实时更新，图层按节流间隔重建，松手定稿
                int v = SliderValueFromX(pt.x, g_sliderTrackX, 90, MOSAIC_BLOCK_MIN, MOSAIC_BLOCK_MAX);
                if (v != g_mosaicBlock) {
                    g_mosaicBlock = v;
                    EnsureMosaicBitmap(false);
                }
            }
            else if (g_isRotatingText && g_rotatingTextIndex >= 0 && g_rotatingTextIndex < (int)g_shapes.size()) {
                auto& shp = g_shapes[g_rotatingTextIndex];
                float cx = (shp.start.x + shp.end.x) / 2.0f;
                float cy = (shp.start.y + shp.end.y) / 2.0f;
                float currentMouseAngle = atan2(pt.y - cy, pt.x - cx) * 180.0f / 3.14159265f;
                shp.angle = g_initialTextAngle + (currentMouseAngle - g_initialMouseAngle);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isResizingText && g_resizingTextIndex >= 0 && g_resizingTextIndex < (int)g_shapes.size()) {
                auto& shp = g_shapes[g_resizingTextIndex];
                float v_nowX = pt.x - g_fixedOppositeCornerX;
                float v_nowY = pt.y - g_fixedOppositeCornerY;
                float d_now = v_nowX * g_initialScreenDirX + v_nowY * g_initialScreenDirY;
                if (d_now > 5.0f && g_diagonalLocalLength > 0.0f) {
                    float newScale = d_now / g_diagonalLocalLength;
                    shp.scale = max(0.1f, min(15.0f, newScale));
                    
                    float projX = g_fixedOppositeCornerX + d_now * g_initialScreenDirX;
                    float projY = g_fixedOppositeCornerY + d_now * g_initialScreenDirY;
                    
                    float cx_new = (g_fixedOppositeCornerX + projX) / 2.0f;
                    float cy_new = (g_fixedOppositeCornerY + projY) / 2.0f;
                    
                    shp.start.x = (int)(cx_new - (shp.origW / 2.0f) * shp.scale);
                    shp.start.y = (int)(cy_new - (shp.origH / 2.0f) * shp.scale);
                    shp.end.x = (int)(cx_new + (shp.origW / 2.0f) * shp.scale);
                    shp.end.y = (int)(cy_new + (shp.origH / 2.0f) * shp.scale);
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDraggingEditingText) {
                g_editTextPos.x = pt.x - g_editingTextDragOffset.x;
                g_editTextPos.y = pt.y - g_editingTextDragOffset.y;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDraggingText && g_draggingTextIndex >= 0 && g_draggingTextIndex < (int)g_shapes.size()) {
                auto& shp = g_shapes[g_draggingTextIndex];
                int w = shp.end.x - shp.start.x;
                int h = shp.end.y - shp.start.y;
                shp.start.x = pt.x - g_textDragOffset.x;
                shp.start.y = pt.y - g_textDragOffset.y;
                shp.end.x = shp.start.x + w;
                shp.end.y = shp.start.y + h;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDrawingShape) {
                // 画笔/涂抹：只失效新增笔迹段的胶囊范围（帧率与笔迹长度无关）；其余工具整屏
                POINT prevPt = pt;
                if (g_tempShape.type == SHAPE_PENCIL || g_tempShape.type == SHAPE_MOSAIC) {
                    if (!g_tempShape.points.empty()) prevPt = g_tempShape.points.back();
                    g_tempShape.points.push_back(pt);
                } else {
                    g_tempShape.end = pt;
                }
                if (g_tempShape.type == SHAPE_MOSAIC || g_tempShape.type == SHAPE_PENCIL) {
                    int half = (g_tempShape.type == SHAPE_MOSAIC ? g_mosaicBrush : g_tempShape.thickness) / 2 + 8;
                    RECT rc = { min(prevPt.x, pt.x) - half, min(prevPt.y, pt.y) - half,
                                max(prevPt.x, pt.x) + half, max(prevPt.y, pt.y) + half };
                    InvalidateRect(hWnd, &rc, FALSE);
                    if (g_tempShape.type == SHAPE_MOSAIC) {
                        g_showMosaicCursor = true;
                        g_mosaicCursorPt = pt;
                    }
                } else {
                    InvalidateRect(hWnd, NULL, FALSE);
                }
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
                if (IsWindowCaptureMode()) {
                    // 默认窗口模式下，自动进行智能窗口捕获
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
                    } else if (g_overlay.hasDetectedWindow) {
                        g_overlay.hasDetectedWindow = false;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (g_overlay.hasDetectedWindow) {
                    g_overlay.hasDetectedWindow = false;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                SetCursor(LoadCursor(NULL, IDC_CROSS));
            }
            else {
                // 如果正处于标注模式，且光标在选区内，设置为十字光标，不要显示大小调整光标
                if (g_annotationMode != ANNOTATION_NONE && PtInRect(&g_overlay.selection, pt)) {
                    // 如果鼠标在工具栏或子工具栏按钮范围内，让它显示手形光标
                    bool onButton = false;
                    bool onRoundInput = false;
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
                        
                        // 判断是否悬停在圆角编辑控件上
                        bool isRectMode = (g_annotationMode == ANNOTATION_RECTANGLE);
                        if (!isRectMode && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                            if (g_shapes[g_selectedTextIndex].type == SHAPE_RECTANGLE) {
                                isRectMode = true;
                            }
                        }
                        if (isRectMode) {
                            int st_x = min_x;
                            int st_y = max_y + 4;
                            RECT roundRect = { st_x + 124, st_y + 8, st_x + 188, st_y + 28 };
                            if (PtInRect(&roundRect, pt)) {
                                onRoundInput = true;
                            }
                        }
                    }
                    
                    bool cursorSet = false;
                    if (onRoundInput) {
                        if (!g_isHoveringRoundRadius) {
                            g_isHoveringRoundRadius = true;
                            g_editingRoundRadiusStr = L"";
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                        SetCursor(LoadCursor(NULL, IDC_IBEAM));
                        cursorSet = true;
                    } else {
                        if (g_isHoveringRoundRadius) {
                            g_isHoveringRoundRadius = false;
                            g_editingRoundRadiusStr = L"";
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                        if (onButton) {
                            SetCursor(LoadCursor(NULL, IDC_HAND));
                            cursorSet = true;
                        }
                    }
                    
                    if (!cursorSet && g_annotationMode == ANNOTATION_TEXT) {
                        
                        // First, if there's a selected text shape, check its handles
                        if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                            auto& shp = g_shapes[g_selectedTextIndex];
                            if (shp.type == SHAPE_TEXT && !shp.text.empty() && shp.origW > 0.0f) {
                                float cx = (shp.start.x + shp.end.x) / 2.0f;
                                float cy = (shp.start.y + shp.end.y) / 2.0f;
                                float radBack = shp.angle * 3.14159265f / 180.0f;
                                
                                // A. Rotation handle (BR) - 匹配绘制时的坐标
                                float rotLocalX = shp.origW / 2.0f + 18.0f;
                                float rotLocalY = shp.origH / 2.0f + 14.0f;
                                float scaledRotX = rotLocalX * shp.scale;
                                float scaledRotY = rotLocalY * shp.scale;
                                float rotScreenX = cx + (scaledRotX * cos(radBack) - scaledRotY * sin(radBack));
                                float rotScreenY = cy + (scaledRotX * sin(radBack) + scaledRotY * cos(radBack));
                                
                                float distR = sqrt((pt.x - rotScreenX) * (pt.x - rotScreenX) + (pt.y - rotScreenY) * (pt.y - rotScreenY));
                                if (distR <= 30.0f) { // 扩大到 30.0f 方便悬停
                                    SetCursor(LoadCursor(NULL, IDC_HAND));
                                    cursorSet = true;
                                }
                                
                                // B. Resize handles (TL, TR, BL)
                                if (!cursorSet) {
                                    auto getScreenPt = [&](float lx, float ly) -> POINT {
                                        float sx = lx * shp.scale;
                                        float sy = ly * shp.scale;
                                        POINT res;
                                        res.x = (int)(cx + (sx * cos(radBack) - sy * sin(radBack)));
                                        res.y = (int)(cy + (sx * sin(radBack) + sy * cos(radBack)));
                                        return res;
                                    };
                                    
                                    POINT ptsCorners[] = {
                                        getScreenPt(-shp.origW/2.0f - 4.0f, -shp.origH/2.0f - 2.0f), // TL
                                        getScreenPt(shp.origW/2.0f + 4.0f, -shp.origH/2.0f - 2.0f),  // TR
                                        getScreenPt(-shp.origW/2.0f - 4.0f, shp.origH/2.0f + 2.0f)   // BL
                                    };
                                    
                                    HCURSOR cursors[] = {
                                        LoadCursor(NULL, IDC_SIZENWSE), // TL
                                        LoadCursor(NULL, IDC_SIZENESW), // TR
                                        LoadCursor(NULL, IDC_SIZENESW)  // BL
                                    };
                                    
                                    for (int k = 0; k < 3; ++k) {
                                        float distC = sqrt((pt.x - ptsCorners[k].x) * (pt.x - ptsCorners[k].x) + (pt.y - ptsCorners[k].y) * (pt.y - ptsCorners[k].y));
                                        if (distC <= 25.0f) { // 扩大到 25.0f
                                            SetCursor(cursors[k]);
                                            cursorSet = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        
                        // C. Check text body (for all text shapes)
                        if (!cursorSet) {
                            for (int i = (int)g_shapes.size() - 1; i >= 0; --i) {
                                auto& shp = g_shapes[i];
                                if (shp.type == SHAPE_TEXT && !shp.text.empty() && shp.origW > 0.0f) {
                                    float cx = (shp.start.x + shp.end.x) / 2.0f;
                                    float cy = (shp.start.y + shp.end.y) / 2.0f;
                                    
                                    float dx = pt.x - cx;
                                    float dy = pt.y - cy;
                                    float rad = -shp.angle * 3.14159265f / 180.0f;
                                    float localX = (dx * cos(rad) - dy * sin(rad)) / shp.scale;
                                    float localY = (dx * sin(rad) + dy * cos(rad)) / shp.scale;
                                    
                                    if (localX >= -shp.origW/2.0f - 6.0f && localX <= shp.origW/2.0f + 6.0f &&
                                        localY >= -shp.origH/2.0f - 4.0f && localY <= shp.origH/2.0f + 4.0f) {
                                        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                                        cursorSet = true;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // D. Check currently editing text body (for active editing text shape)
                        if (!cursorSet && g_isEditingText) {
                            float ew = 0.0f, eh = 0.0f;
                            if (IsPointInEditingText(hWnd, pt, ew, eh)) {
                                SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                                cursorSet = true;
                            }
                        }
                        
                        if (!cursorSet) {
                            SetCursor(LoadCursor(NULL, IDC_CROSS));
                        }
                    } else if (!cursorSet && g_annotationMode == ANNOTATION_MOSAIC && g_mosaicShapeMode == 0) {
                        // 涂抹马赛克：隐藏十字，用笔刷覆盖圈作为光标（圈在绘制阶段渲染，
                        // 失效矩形由本消息末尾统一做局部重绘）
                        g_showMosaicCursor = true;
                        g_mosaicCursorPt = pt;
                        SetCursor(NULL);
                    } else {
                        SetCursor(LoadCursor(NULL, IDC_CROSS));
                    }
                } else {
                    // 更新鼠标形状与工具栏按钮高亮
                    UpdateOverlayCursor(hWnd, pt);
                }
                
                // 工具栏整体（含底盒内边距与子工具栏）统一手形光标，与点击拦截区域一致，
                // 杜绝按钮间隙处手形/十字来回跳；高亮变化只失效新旧按钮矩形，避免整屏重合成掉帧
                int hovered = 0;
                bool onToolbarUi = false;
                if (g_overlay.selectionDone) {
                    auto buttons = GetToolbarButtons(g_overlay.selection, g_screenWidth, g_screenHeight);
                    int min_x = buttons[0].rect.left - 8;
                    int min_y = buttons[0].rect.top - 6;
                    int max_x = buttons.back().rect.right + 8;
                    int max_y = buttons[0].rect.bottom + 6;
                    RECT barBox = { min_x, min_y, max_x, max_y };
                    if (PtInRect(&barBox, pt)) onToolbarUi = true;
                    for (size_t i = 0; i < buttons.size(); ++i) {
                        if (PtInRect(&buttons[i].rect, pt)) {
                            hovered = (int)(i + 1);
                            break;
                        }
                    }
                    if (g_annotationMode != ANNOTATION_NONE || g_selectedTextIndex >= 0) {
                        RECT subBox = { min_x, max_y, max_x, max_y + 40 };
                        if (PtInRect(&subBox, pt)) onToolbarUi = true;
                    }
                    if (hovered != g_overlay.hoveredButton) {
                        if (g_overlay.hoveredButton >= 1 && g_overlay.hoveredButton <= (int)buttons.size()) {
                            InvalidateRect(hWnd, &buttons[g_overlay.hoveredButton - 1].rect, FALSE);
                        }
                        if (hovered >= 1 && hovered <= (int)buttons.size()) {
                            InvalidateRect(hWnd, &buttons[hovered - 1].rect, FALSE);
                        }
                        g_overlay.hoveredButton = hovered;
                    }
                }
                if (onToolbarUi) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    g_showMosaicCursor = false;
                }
            }

            // 笔刷圈局部重绘：圈的位置/可见性变化时，只失效新旧位置的小矩形，
            // 其余画面全部复用双缓冲——圈因此可以跟满显示器刷新率
            if (g_showMosaicCursor && !g_circleOnBuffer) {
                g_circleLastRect = MosaicCursorRect(g_mosaicCursorPt);
                InvalidateRect(hWnd, &g_circleLastRect, FALSE);
                g_circleOnBuffer = true;
            } else if (g_showMosaicCursor && g_circleOnBuffer) {
                RECT rcNew = MosaicCursorRect(g_mosaicCursorPt);
                RECT rcUnion;
                UnionRect(&rcUnion, &rcNew, &g_circleLastRect);
                InvalidateRect(hWnd, &rcUnion, FALSE);
                g_circleLastRect = rcNew;
            } else if (g_circleOnBuffer) {
                // 圈消失（移出选区/进入工具栏/切到矩形模式）：擦除缓冲中的旧圈
                InvalidateRect(hWnd, &g_circleLastRect, FALSE);
                g_circleOnBuffer = false;
            }
            break;
        }
        case WM_LBUTTONDBLCLK: {
            POINT pt;
            pt.x = (short)LOWORD(lParam);   // 符号扩展：副屏在主屏左侧/上方时客户区坐标为负
            pt.y = (short)HIWORD(lParam);
            
            if (g_annotationMode == ANNOTATION_TEXT && g_overlay.selectionDone) {
                // Find if double click is inside an existing text shape
                for (int i = (int)g_shapes.size() - 1; i >= 0; --i) {
                    auto& shp = g_shapes[i];
                    if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                        // Measure it first if not measured
                        {
                            HDC tmpDC = GetDC(hWnd);
                            Graphics tmpG(tmpDC);
                            EnsureTextShapeMeasured(tmpG, shp);
                            ReleaseDC(hWnd, tmpDC);
                        }
                        
                        float cx = (shp.start.x + shp.end.x) / 2.0f;
                        float cy = (shp.start.y + shp.end.y) / 2.0f;
                        
                        float dx = pt.x - cx;
                        float dy = pt.y - cy;
                        float rad = -shp.angle * 3.14159265f / 180.0f;
                        float localX = (dx * cos(rad) - dy * sin(rad)) / shp.scale;
                        float localY = (dx * sin(rad) + dy * cos(rad)) / shp.scale;
                        
                        if (localX >= -shp.origW/2.0f - 6.0f && localX <= shp.origW/2.0f + 6.0f &&
                            localY >= -shp.origH/2.0f - 4.0f && localY <= shp.origH/2.0f + 4.0f) {
                            
                            // Double clicked this text! Finalize any previous editing text first
                            CommitEditingText();
                            
                            g_isEditingText = true;
                            g_editingText = shp.text;
                            g_editingCaretIndex = (int)g_editingText.length(); // 光标置于末尾
                            // 双击进入编辑：编辑锚点 = 包围盒中心 - 未缩放文本尺寸的一半，
                            // 与提交时的锚定规则互为逆运算，缩放/旋转过的文字进入编辑不会跳位
                            g_editTextPos.x = shp.start.x + (shp.end.x - shp.start.x) / 2 - (int)(shp.origW / 2.0f);
                            g_editTextPos.y = shp.start.y + (shp.end.y - shp.start.y) / 2 - (int)(shp.origH / 2.0f);
                            g_editAngle = shp.angle;
                            g_editScale = shp.scale;
                            g_currentColor = shp.color;
                            g_currentThickness = shp.thickness;
                            g_currentFontSize = shp.fontSize;
                            g_currentFontFamily = shp.fontFamily;
                            g_currentFontStyle = shp.fontStyle;
                            
                            // Remove from static shapes so it becomes editable
                            g_shapes.erase(g_shapes.begin() + i);
                            g_selectedTextIndex = -1;
                            
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                    }
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = (short)LOWORD(lParam);   // 符号扩展：副屏在主屏左侧/上方时客户区坐标为负
            pt.y = (short)HIWORD(lParam);
            
            if (!g_overlay.selectionDone) {
                // 开始全新框选流程 (窗口模式支持单击选窗，区域模式则直接进入拖拽框选)
                g_overlay.startPos = pt;
                g_overlay.selection = { pt.x, pt.y, pt.x, pt.y };
                g_overlay.isSelecting = true;
                g_overlay.isPossibleClick = IsWindowCaptureMode();
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
                    
                    if (g_annotationMode != ANNOTATION_NONE || g_selectedTextIndex >= 0) {
                        subToolbarBox = { min_x, max_y, max_x, max_y + 40 };
                        if (PtInRect(&subToolbarBox, pt)) {
                            clickedOnToolbar = true;
                        }
                    }
                }
                
                // 2. 如果点击在属性子工具栏区域，处理粗细/字体/字号 and 颜色选择
                if ((g_annotationMode != ANNOTATION_NONE || g_selectedTextIndex >= 0) && !buttons.empty() && PtInRect(&subToolbarBox, pt)) {
                    int min_x = buttons[0].rect.left - 8;
                    int max_y = buttons[0].rect.bottom + 6;
                    int st_x = min_x;
                    int st_y = max_y + 4;
                    
                    bool isTextMode = (g_annotationMode == ANNOTATION_TEXT || g_isEditingText);
                    if (!isTextMode && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                        if (g_shapes[g_selectedTextIndex].type == SHAPE_TEXT) {
                            isTextMode = true;
                        }
                    }
                    
                    bool isRectMode = (g_annotationMode == ANNOTATION_RECTANGLE);
                    if (!isRectMode && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                        if (g_shapes[g_selectedTextIndex].type == SHAPE_RECTANGLE) {
                            isRectMode = true;
                        }
                    }
                    
                    if (g_annotationMode == ANNOTATION_MOSAIC) {
                        // A0. 马赛克属性：模式 / 风格切换 + 粗细/程度两条滑条
                        RECT chipBrush = { st_x + 12, st_y + 7, st_x + 56, st_y + 29 };
                        RECT chipRectM = { st_x + 60, st_y + 7, st_x + 104, st_y + 29 };
                        RECT chipPixel = { st_x + 116, st_y + 7, st_x + 160, st_y + 29 };
                        RECT chipBlur = { st_x + 164, st_y + 7, st_x + 208, st_y + 29 };
                        if (PtInRect(&chipBrush, pt)) {
                            g_mosaicShapeMode = 0;
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                        if (PtInRect(&chipRectM, pt)) {
                            g_mosaicShapeMode = 1;
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                        if (PtInRect(&chipPixel, pt)) {
                            if (g_mosaicStyle != 0) {
                                g_mosaicStyle = 0;
                                EnsureMosaicBitmap(true);
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }
                        if (PtInRect(&chipBlur, pt)) {
                            if (g_mosaicStyle != 1) {
                                g_mosaicStyle = 1;
                                EnsureMosaicBitmap(true);
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                            break;
                        }

                        // 粗细滑条（仅涂抹模式）：按住即进入无级拖动，松开结束
                        if (g_mosaicShapeMode == 0) {
                            RECT brushStrip = { st_x + 244, st_y + 4, st_x + 346, st_y + 30 };
                            if (PtInRect(&brushStrip, pt)) {
                                g_mosaicSliderDrag = 1;
                                g_sliderTrackX = st_x + 250;
                                g_mosaicBrush = SliderValueFromX(pt.x, g_sliderTrackX, 90, MOSAIC_BRUSH_MIN, MOSAIC_BRUSH_MAX);
                                g_sliderStripRect = { st_x + 214, st_y + 2, st_x + 380, st_y + 34 };
                                SetCapture(hWnd);
                                InvalidateRect(hWnd, &g_sliderStripRect, FALSE);
                                g_showMosaicCursor = true;
                                g_mosaicCursorPt = pt;
                                g_circleLastRect = MosaicCursorRect(pt);
                                InvalidateRect(hWnd, &g_circleLastRect, FALSE);
                                g_circleOnBuffer = true;
                                break;
                            }
                        }

                        // 质量滑条：按住进入拖动（节流重建），松开按最终值重建图层
                        RECT blockStrip = { st_x + 406, st_y + 4, st_x + 508, st_y + 30 };
                        if (PtInRect(&blockStrip, pt)) {
                            g_mosaicSliderDrag = 2;
                            g_sliderTrackX = st_x + 412;
                            g_mosaicBlock = SliderValueFromX(pt.x, g_sliderTrackX, 90, MOSAIC_BLOCK_MIN, MOSAIC_BLOCK_MAX);
                            g_sliderStripRect = { st_x + 376, st_y + 2, st_x + 540, st_y + 34 };
                            EnsureMosaicBitmap(true);
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, NULL, FALSE); // 质量变化影响所有已有马赛克，整屏刷新
                            break;
                        }
                        break; // 拦截消息，不往下处理
                    }

                    if (!isTextMode) {
                        // A. 粗细项点击
                        int thicks[] = { 2, 4, 8 };
                        bool thickClicked = false;
                        for (int i = 0; i < 3; ++i) {
                            int bx = st_x + 12 + i * 30;
                            int by = st_y + 6;
                            RECT dotRect = { bx, by, bx + 24, by + 24 };
                            if (PtInRect(&dotRect, pt)) {
                                g_currentThickness = thicks[i];
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].thickness = thicks[i];
                                }
                                InvalidateRect(hWnd, NULL, FALSE);
                                thickClicked = true;
                                break;
                            }
                        }
                        
                        // 调节圆角大小点击
                        if (!thickClicked && isRectMode) {
                            RECT decRect = { st_x + 100, st_y + 8, st_x + 120, st_y + 28 };
                            RECT incRect = { st_x + 192, st_y + 8, st_x + 212, st_y + 28 };
                            if (PtInRect(&decRect, pt)) {
                                int currentRound = g_currentRoundRadius;
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    currentRound = g_shapes[g_selectedTextIndex].roundRadius;
                                }
                                currentRound = max(0, currentRound - 5);
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].roundRadius = currentRound;
                                } else {
                                    g_currentRoundRadius = currentRound;
                                }
                                SaveConfig();
                                InvalidateRect(hWnd, NULL, FALSE);
                            } else if (PtInRect(&incRect, pt)) {
                                int currentRound = g_currentRoundRadius;
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    currentRound = g_shapes[g_selectedTextIndex].roundRadius;
                                }
                                currentRound = min(30, currentRound + 5);
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].roundRadius = currentRound;
                                } else {
                                    g_currentRoundRadius = currentRound;
                                }
                                SaveConfig();
                                InvalidateRect(hWnd, NULL, FALSE);
                            }
                        }
                    } else {
                        // A. 字号/字体点击
                        RECT decRect = { st_x + 12, st_y + 8, st_x + 32, st_y + 28 };
                        RECT incRect = { st_x + 68, st_y + 8, st_x + 88, st_y + 28 };
                        RECT fontRect = { st_x + 96, st_y + 8, st_x + 220, st_y + 28 };
                        
                        if (PtInRect(&decRect, pt)) {
                            int currentSize = g_currentFontSize;
                            if (!g_isEditingText && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                currentSize = g_shapes[g_selectedTextIndex].fontSize;
                            }
                            currentSize = max(8, currentSize - 2);
                            if (g_isEditingText) {
                                g_currentFontSize = currentSize;
                            } else if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                g_shapes[g_selectedTextIndex].fontSize = currentSize;
                                g_shapes[g_selectedTextIndex].origW = 0.0f; // force re-measure
                                // 与滚轮调字号保持一致：同步全局字号，之后新建的文字沿用该大小
                                g_currentFontSize = currentSize;
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                        else if (PtInRect(&incRect, pt)) {
                            int currentSize = g_currentFontSize;
                            if (!g_isEditingText && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                currentSize = g_shapes[g_selectedTextIndex].fontSize;
                            }
                            currentSize = min(120, currentSize + 2);
                            if (g_isEditingText) {
                                g_currentFontSize = currentSize;
                            } else if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                g_shapes[g_selectedTextIndex].fontSize = currentSize;
                                g_shapes[g_selectedTextIndex].origW = 0.0f; // force re-measure
                                g_currentFontSize = currentSize;
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                        else if (PtInRect(&fontRect, pt)) {
                            wstring family = g_currentFontFamily;
                            int size = g_currentFontSize;
                            int style = g_currentFontStyle;
                            if (!g_isEditingText && g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                family = g_shapes[g_selectedTextIndex].fontFamily;
                                size = g_shapes[g_selectedTextIndex].fontSize;
                                style = g_shapes[g_selectedTextIndex].fontStyle;
                            }
                            
                            if (PromptForFont(hWnd, family, size, style)) {
                                if (g_isEditingText) {
                                    g_currentFontFamily = family;
                                    g_currentFontSize = size;
                                    g_currentFontStyle = style;
                                } else if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].fontFamily = family;
                                    g_shapes[g_selectedTextIndex].fontSize = size;
                                    g_shapes[g_selectedTextIndex].fontStyle = style;
                                    g_shapes[g_selectedTextIndex].origW = 0.0f; // force re-measure
                                    // 同步全局字体属性，之后新建的文字沿用该字体
                                    g_currentFontFamily = family;
                                    g_currentFontSize = size;
                                    g_currentFontStyle = style;
                                }
                            }
                            // 字体对话框关闭后，必须重新将焦点拉回覆盖层窗口，否则键盘输入会丢失
                            SetForegroundWindow(hWnd);
                            SetFocus(hWnd);
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    }
                    
                    // B. 颜色项点击 (7个默认 + 4个自定义色槽 + 1个修改设置按钮)
                    Color colors[] = {
                        Color(255, 231, 76, 60),      // 红
                        Color(255, 52, 152, 219),     // 蓝
                        Color(255, 46, 204, 113),     // 绿
                        Color(255, 241, 196, 15),     // 黄
                        Color(255, 127, 140, 141),    // 灰
                        Color(255, 255, 255, 255),    // 白
                        Color(255, 44, 62, 80)        // 黑
                    };
                    
                    int colorStart = 120;
                    if (isTextMode) {
                        colorStart = 232;
                    } else if (isRectMode) {
                        colorStart = 224;
                    }
                    for (int i = 0; i < 12; ++i) {
                        int cx = st_x + colorStart + i * 20;
                        int cy = st_y + 9;
                        RECT colRect = { cx, cy, cx + 16, cy + 16 };
                        if (PtInRect(&colRect, pt)) {
                            if (i < 7) {
                                g_currentColor = colors[i];
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].color = colors[i];
                                }
                            } else if (i >= 7 && i < 11) {
                                int slotIdx = i - 7;
                                g_activeCustomSlotIndex = slotIdx;
                                g_currentColor = g_config.custom_colors[slotIdx];
                                if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                    g_shapes[g_selectedTextIndex].color = g_config.custom_colors[slotIdx];
                                }
                            } else {
                                // 点击 "+" 按钮，弹出 Windows 调色板修改当前高亮的自定义槽位颜色
                                Color tempColor = g_config.custom_colors[g_activeCustomSlotIndex];
                                if (PromptForColor(hWnd, tempColor)) {
                                    g_config.custom_colors[g_activeCustomSlotIndex] = tempColor;
                                    g_currentColor = tempColor;
                                    
                                    // 立即保存到 config.json
                                    SaveConfig();
                                    
                                    if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                                        g_shapes[g_selectedTextIndex].color = tempColor;
                                    }
                                }
                            }
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
                    // 长图预览模式：只响应 保存/取消/确定
                    if (g_longPreviewActive) {
                        if (clickedBtn == 1) {
                            DoCaptureSaveAs(hWnd);
                        } else if (clickedBtn == 2) {
                            SendMessage(hWnd, WM_CLOSE, 0, 0);
                        } else if (clickedBtn == 3) {
                            DoCaptureConfirm(hWnd);
                        }
                        break;
                    }
                    int btn = clickedBtn;
                    g_overlay.hoveredButton = clickedBtn; // 同步高亮状态
                    
                    if (btn == 1) { // 矩形
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_RECTANGLE) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_RECTANGLE;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 2) { // 圆形
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_CIRCLE) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_CIRCLE;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 3) { // 箭头
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_ARROW) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_ARROW;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 4) { // 画笔
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_PENCIL) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_PENCIL;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 5) { // 马赛克
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_MOSAIC) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_MOSAIC;
                            EnsureMosaicBitmap(true); // 预热图层，首次拖拽零延迟
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 6) { // 文字
                        CommitEditingText();
                        g_selectedTextIndex = -1;
                        if (g_annotationMode == ANNOTATION_TEXT) {
                            g_annotationMode = ANNOTATION_NONE;
                        } else {
                            g_annotationMode = ANNOTATION_TEXT;
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 7) { // 长截屏（滚动截屏）
                        EnterScrollCapture(hWnd);
                    }
                    else if (btn == 8) { // 撤销
                        g_selectedTextIndex = -1;
                        if (g_isEditingText) {
                            ResetEditingTextState();
                        } else {
                            UndoShape();
                        }
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    else if (btn == 9) { // 保存
                        DoCaptureSaveAs(hWnd);
                    }
                    else if (btn == 10) { // 取消
                        SendMessage(hWnd, WM_CLOSE, 0, 0);
                    }
                    else if (btn == 11) { // 确定
                        DoCaptureConfirm(hWnd);
                    }
                    break; // 拦截消息，不往下处理
                }
                
                // 4. 如果点击在工具栏的空白/边框区域（非按钮），直接拦截，防止误触导致选区重置
                if (clickedOnToolbar) {
                    break;
                }

                // 长图预览模式下，框内点击一律无效（仅允许工具栏的 保存/取消/确定）
                if (g_longPreviewActive) {
                    break;
                }
                
                // 5. 如果处于标注模式，且在选区内
                if (g_annotationMode != ANNOTATION_NONE && PtInRect(&g_overlay.selection, pt)) {
                    if (g_annotationMode == ANNOTATION_TEXT) {
                        // 文字模式：检查是否点击在已有文字上（拖动/旋转/缩放/选择）
                        bool hitExistingText = false;
                        for (int i = (int)g_shapes.size() - 1; i >= 0; --i) {
                            auto& shp = g_shapes[i];
                            if (shp.type == SHAPE_TEXT && !shp.text.empty()) {
                                // 必须有已初始化的宽度和高度
                                HDC tmpDC = GetDC(hWnd);
                                Graphics tmpG(tmpDC);
                                EnsureTextShapeMeasured(tmpG, shp);
                                ReleaseDC(hWnd, tmpDC);
                                
                                float cx = (shp.start.x + shp.end.x) / 2.0f;
                                float cy = (shp.start.y + shp.end.y) / 2.0f;
                                
                                // A. 检查旋转手柄 (BR 旋转大圆圈) - 匹配绘制时的坐标
                                float rotLocalX = shp.origW / 2.0f + 18.0f;
                                float rotLocalY = shp.origH / 2.0f + 14.0f;
                                float radBack = shp.angle * 3.14159265f / 180.0f;
                                float scaledRotX = rotLocalX * shp.scale;
                                float scaledRotY = rotLocalY * shp.scale;
                                float rotScreenX = cx + (scaledRotX * cos(radBack) - scaledRotY * sin(radBack));
                                float rotScreenY = cy + (scaledRotX * sin(radBack) + scaledRotY * cos(radBack));
                                
                                float distR = sqrt((pt.x - rotScreenX) * (pt.x - rotScreenX) + (pt.y - rotScreenY) * (pt.y - rotScreenY));
                                if (distR <= 30.0f) { // 扩大到 30.0f，使之极易点击
                                    g_isRotatingText = true;
                                    g_rotatingTextIndex = i;
                                    g_selectedTextIndex = i;
                                    g_currentColor = shp.color;
                                    g_currentThickness = shp.thickness;
                                    g_initialMouseAngle = atan2(pt.y - cy, pt.x - cx) * 180.0f / 3.14159265f;
                                    g_initialTextAngle = shp.angle;
                                    SetCapture(hWnd);
                                    hitExistingText = true;
                                    break;
                                }
                                
                                // B. 检查 3 个角上的缩放手柄 (TL, TR, BL)
                                auto getScreenPt = [&](float lx, float ly) -> POINT {
                                    float sx = lx * shp.scale;
                                    float sy = ly * shp.scale;
                                    POINT res;
                                    res.x = (int)(cx + (sx * cos(radBack) - sy * sin(radBack)));
                                    res.y = (int)(cy + (sx * sin(radBack) + sy * cos(radBack)));
                                    return res;
                                };
                                
                                POINT ptsCorners[] = {
                                    getScreenPt(-shp.origW/2.0f - 4.0f, -shp.origH/2.0f - 2.0f), // TL
                                    getScreenPt(shp.origW/2.0f + 4.0f, -shp.origH/2.0f - 2.0f),  // TR
                                    getScreenPt(-shp.origW/2.0f - 4.0f, shp.origH/2.0f + 2.0f)   // BL
                                };
                                
                                bool hitResize = false;
                                for (int k = 0; k < 3; ++k) {
                                    float distC = sqrt((pt.x - ptsCorners[k].x) * (pt.x - ptsCorners[k].x) + (pt.y - ptsCorners[k].y) * (pt.y - ptsCorners[k].y));
                                    if (distC <= 25.0f) { // 扩大到 25.0f 提高灵敏度
                                        g_isResizingText = true;
                                        g_resizingTextIndex = i;
                                        g_selectedTextIndex = i;
                                        g_currentColor = shp.color;
                                        g_currentThickness = shp.thickness;
                                        
                                        // 记录对角线固定点缩放状态
                                        g_resizeTextHandle = k;
                                        g_initialTextScale = shp.scale;
                                        
                                        float lx_opp = 0.0f, ly_opp = 0.0f;
                                        float lx_drag = 0.0f, ly_drag = 0.0f;
                                        if (k == 0) { // TL
                                            lx_drag = -shp.origW/2.0f; ly_drag = -shp.origH/2.0f;
                                            lx_opp = shp.origW/2.0f;   ly_opp = shp.origH/2.0f;
                                        } else if (k == 1) { // TR
                                            lx_drag = shp.origW/2.0f;  ly_drag = -shp.origH/2.0f;
                                            lx_opp = -shp.origW/2.0f;  ly_opp = shp.origH/2.0f;
                                        } else if (k == 2) { // BL
                                            lx_drag = -shp.origW/2.0f; ly_drag = shp.origH/2.0f;
                                            lx_opp = shp.origW/2.0f;   ly_opp = -shp.origH/2.0f;
                                        }
                                        
                                        float s_oppX = cx + (lx_opp * shp.scale * cos(radBack) - ly_opp * shp.scale * sin(radBack));
                                        float s_oppY = cy + (lx_opp * shp.scale * sin(radBack) + ly_opp * shp.scale * cos(radBack));
                                        g_fixedOppositeCornerX = s_oppX;
                                        g_fixedOppositeCornerY = s_oppY;
                                        
                                        float s_dragX = cx + (lx_drag * shp.scale * cos(radBack) - ly_drag * shp.scale * sin(radBack));
                                        float s_dragY = cy + (lx_drag * shp.scale * sin(radBack) + ly_drag * shp.scale * cos(radBack));
                                        
                                        float v_initX = s_dragX - s_oppX;
                                        float v_initY = s_dragY - s_oppY;
                                        float d_init = sqrt(v_initX * v_initX + v_initY * v_initY);
                                        if (d_init > 0.0f) {
                                            g_initialScreenDirX = v_initX / d_init;
                                            g_initialScreenDirY = v_initY / d_init;
                                        }
                                        g_diagonalLocalLength = sqrt((lx_drag - lx_opp)*(lx_drag - lx_opp) + (ly_drag - ly_opp)*(ly_drag - ly_opp));
                                        
                                        SetCapture(hWnd);
                                        hitExistingText = true;
                                        hitResize = true;
                                        break;
                                    }
                                }
                                if (hitResize) break;
                                
                                // C. 检查是否点击在文字矩形内部
                                float dx = pt.x - cx;
                                float dy = pt.y - cy;
                                float rad = -shp.angle * 3.14159265f / 180.0f;
                                float localX = (dx * cos(rad) - dy * sin(rad)) / shp.scale;
                                float localY = (dx * sin(rad) + dy * cos(rad)) / shp.scale;
                                
                                if (localX >= -shp.origW/2.0f - 6.0f && localX <= shp.origW/2.0f + 6.0f &&
                                    localY >= -shp.origH/2.0f - 4.0f && localY <= shp.origH/2.0f + 4.0f) {
                                    g_isDraggingText = true;
                                    g_draggingTextIndex = i;
                                    g_selectedTextIndex = i;
                                    
                                    g_currentColor = shp.color;
                                    g_currentThickness = shp.thickness;
                                    
                                    g_textDragOffset.x = pt.x - shp.start.x;
                                    g_textDragOffset.y = pt.y - shp.start.y;
                                    SetCapture(hWnd);
                                    hitExistingText = true;
                                    break;
                                }
                            }
                        }
                        
                        // 新增：如果点击在当前正在编辑的文字内部，启动拖拽移动，不提交、不创建新框
                        if (!hitExistingText && g_isEditingText) {
                            float ew = 0.0f, eh = 0.0f;
                            if (IsPointInEditingText(hWnd, pt, ew, eh)) {
                                g_isDraggingEditingText = true;
                                g_editingTextDragOffset.x = pt.x - g_editTextPos.x;
                                g_editingTextDragOffset.y = pt.y - g_editTextPos.y;
                                SetCapture(hWnd);
                                hitExistingText = true;
                            }
                        }
                        
                        if (!hitExistingText) {
                            // 提交上一次编辑中的文字
                            CommitEditingText();
                            // 点击空白，清除选择
                            g_selectedTextIndex = -1;
                            
                            // 在点击位置开始新的文字输入
                            g_isEditingText = true;
                            g_editingText = L"";
                            g_editingCaretIndex = 0;
                            g_imeComposing = false;
                            g_imeComposition = L"";
                            g_imeCaretInComposition = 0;
                            g_editTextPos = pt;
                            g_editAngle = 0.0f;
                            g_editScale = 1.0f;
                            InvalidateRect(hWnd, NULL, FALSE);
                        } else {
                            InvalidateRect(hWnd, NULL, FALSE);
                        }
                    } else {
                        // 其它标注模式，清除文字选择
                        g_selectedTextIndex = -1;
                        
                        // 箭头/矩形/圆形/画笔/马赛克模式
                        g_isDrawingShape = true;
                        if (g_annotationMode == ANNOTATION_ARROW) {
                            g_tempShape.type = SHAPE_ARROW;
                        } else if (g_annotationMode == ANNOTATION_CIRCLE) {
                            g_tempShape.type = SHAPE_CIRCLE;
                        } else if (g_annotationMode == ANNOTATION_PENCIL) {
                            g_tempShape.type = SHAPE_PENCIL;
                            g_tempShape.points.clear();
                            g_tempShape.points.push_back(pt);
                        } else if (g_annotationMode == ANNOTATION_MOSAIC) {
                            g_tempShape.type = (g_mosaicShapeMode == 1) ? SHAPE_MOSAIC_RECT : SHAPE_MOSAIC;
                            g_tempShape.points.clear();
                            if (g_mosaicShapeMode == 0) g_tempShape.points.push_back(pt);
                        } else {
                            g_tempShape.type = SHAPE_RECTANGLE;
                        }
                        g_tempShape.start = pt;
                        g_tempShape.end = pt;
                        g_tempShape.color = g_currentColor;
                        g_tempShape.thickness = g_currentThickness;
                        g_tempShape.roundRadius = g_currentRoundRadius;
                        // 涂抹马赛克的笔刷直径必须在通用赋值之后覆盖，
                        // 否则会被上面的画笔粗细(2/4/8)盖掉，滑条完全失效
                        if (g_annotationMode == ANNOTATION_MOSAIC && g_mosaicShapeMode == 0) {
                            g_tempShape.thickness = g_mosaicBrush;
                        }
                        SetCapture(hWnd);
                    }
                } else {
                    // 点击在选区外，清除文字选择
                    g_selectedTextIndex = -1;
                    
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
            if (g_mosaicSliderDrag) {
                bool wasIntensity = (g_mosaicSliderDrag == 2);
                g_mosaicSliderDrag = 0;
                ReleaseCapture();
                if (wasIntensity) EnsureMosaicBitmap(true); // 松手立即按最终值重建图层
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isRotatingText) {
                g_isRotatingText = false;
                g_rotatingTextIndex = -1;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isResizingText) {
                g_isResizingText = false;
                g_resizingTextIndex = -1;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDraggingText) {
                g_isDraggingText = false;
                g_draggingTextIndex = -1;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDraggingEditingText) {
                g_isDraggingEditingText = false;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (g_isDrawingShape) {
                g_isDrawingShape = false;
                ReleaseCapture();
                
                bool shouldAdd = false;
                if (g_tempShape.type == SHAPE_PENCIL || g_tempShape.type == SHAPE_MOSAIC) {
                    if (g_tempShape.points.size() > 1) {
                        shouldAdd = true;
                    }
                } else {
                    int dx = g_tempShape.end.x - g_tempShape.start.x;
                    int dy = g_tempShape.end.y - g_tempShape.start.y;
                    if (sqrt(dx * dx + dy * dy) > 5) {
                        shouldAdd = true;
                    }
                }
                if (shouldAdd) {
                    ClearRedoStack();
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
                        g_toolbarAnimStart = GetTickCount();
                    } else {
                        g_overlay.selection = { 0, 0, 0, 0 };
                        g_overlay.selectionDone = false;
                    }
                } else {
                    // 这是一次长按鼠标左键拖拽框选的流程！
                    g_overlay.selection.left = min(g_overlay.startPos.x, (LONG)(short)LOWORD(lParam));
                    g_overlay.selection.top = min(g_overlay.startPos.y, (LONG)(short)HIWORD(lParam));
                    g_overlay.selection.right = max(g_overlay.startPos.x, (LONG)(short)LOWORD(lParam));
                    g_overlay.selection.bottom = max(g_overlay.startPos.y, (LONG)(short)HIWORD(lParam));
                    
                    int w = g_overlay.selection.right - g_overlay.selection.left;
                    int h = g_overlay.selection.bottom - g_overlay.selection.top;
                    
                    if (w > MIN_SELECTION_SIZE && h > MIN_SELECTION_SIZE) {
                        g_overlay.selectionDone = true;
                        g_toolbarAnimStart = GetTickCount();
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
        case WM_IME_SETCONTEXT: {
            // 保留默认行为以显示候选窗口，同时把组合窗口挪到文字光标处
            LRESULT res = DefWindowProc(hWnd, message, wParam, lParam);
            UpdateImeWindowPosition(hWnd);
            return res;
        }
        case WM_IME_STARTCOMPOSITION: {
            if (g_isEditingText) {
                g_imeComposing = true;
                g_imeComposition = L"";
                g_imeCaretInComposition = 0;
                UpdateImeWindowPosition(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        case WM_IME_COMPOSITION: {
            if (g_isEditingText) {
                UpdateImeComposition(hWnd, lParam);
                UpdateImeWindowPosition(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            // 交回默认处理，最终上屏字符仍通过 WM_CHAR 插入，避免重复输入
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        case WM_IME_ENDCOMPOSITION: {
            g_imeComposing = false;
            g_imeComposition = L"";
            g_imeCaretInComposition = 0;
            InvalidateRect(hWnd, NULL, FALSE);
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        case WM_KEYDOWN: {
            // 组合输入期间的按键交给输入法，不要被当成编辑命令
            if (g_imeComposing) break;

            // 全局编辑快捷键（正在编辑文字或圆角数值时不拦截）
            if (!g_isEditingText && !g_isHoveringRoundRadius) {
                bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                if (ctrl && wParam == 'Z' && shift) {
                    RedoShape();
                    g_selectedTextIndex = -1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                if (ctrl && wParam == 'Y') {
                    RedoShape();
                    g_selectedTextIndex = -1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                if (ctrl && wParam == 'Z') {
                    UndoShape();
                    g_selectedTextIndex = -1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                if (ctrl && wParam == 'S' && g_overlay.selectionDone) {
                    DoCaptureSaveAs(hWnd);
                    break;
                }
                if (ctrl && wParam == 'C' && g_overlay.selectionDone) {
                    DoCaptureConfirm(hWnd);
                    break;
                }
                if (wParam == VK_RETURN && g_overlay.selectionDone) {
                    DoCaptureConfirm(hWnd);
                    break;
                }
            }
            if (g_isHoveringRoundRadius) {
                if (wParam == VK_BACK) {
                    if (!g_editingRoundRadiusStr.empty()) {
                        g_editingRoundRadiusStr.pop_back();
                        int val = g_editingRoundRadiusStr.empty() ? 0 : wcstol(g_editingRoundRadiusStr.c_str(), NULL, 10);
                        if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                            g_shapes[g_selectedTextIndex].roundRadius = val;
                        } else {
                            g_currentRoundRadius = val;
                        }
                        SaveConfig();
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                break;
            }
            if (g_isEditingText) {
                if (wParam == VK_BACK) {
                    if (g_editingCaretIndex > 0) {
                        // 光标前是代理对（如表情符号）时整对删除，避免拆散产生无效字符
                        int step = 1;
                        if (g_editingCaretIndex >= 2 &&
                            IS_LOW_SURROGATE(g_editingText[g_editingCaretIndex - 1]) &&
                            IS_HIGH_SURROGATE(g_editingText[g_editingCaretIndex - 2])) {
                            step = 2;
                        }
                        g_editingText.erase(g_editingCaretIndex - step, step);
                        g_editingCaretIndex -= step;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (wParam == VK_DELETE) {
                    if (g_editingCaretIndex < (int)g_editingText.length()) {
                        int step = 1;
                        if (g_editingCaretIndex + 1 < (int)g_editingText.length() &&
                            IS_HIGH_SURROGATE(g_editingText[g_editingCaretIndex]) &&
                            IS_LOW_SURROGATE(g_editingText[g_editingCaretIndex + 1])) {
                            step = 2;
                        }
                        g_editingText.erase(g_editingCaretIndex, step);
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (wParam == VK_LEFT) {
                    if (g_editingCaretIndex > 0) {
                        int step = 1;
                        if (g_editingCaretIndex >= 2 &&
                            IS_LOW_SURROGATE(g_editingText[g_editingCaretIndex - 1]) &&
                            IS_HIGH_SURROGATE(g_editingText[g_editingCaretIndex - 2])) {
                            step = 2;
                        }
                        g_editingCaretIndex -= step;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (wParam == VK_RIGHT) {
                    if (g_editingCaretIndex < (int)g_editingText.length()) {
                        int step = 1;
                        if (g_editingCaretIndex + 1 < (int)g_editingText.length() &&
                            IS_HIGH_SURROGATE(g_editingText[g_editingCaretIndex]) &&
                            IS_LOW_SURROGATE(g_editingText[g_editingCaretIndex + 1])) {
                            step = 2;
                        }
                        g_editingCaretIndex += step;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (wParam == VK_HOME) {
                    g_editingCaretIndex = 0;
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wParam == VK_END) {
                    g_editingCaretIndex = (int)g_editingText.length();
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wParam == VK_RETURN) {
                    // 回车确认文字
                    CommitEditingText();
                    InvalidateRect(hWnd, NULL, FALSE);
                } else if (wParam == VK_ESCAPE) {
                    ResetEditingTextState();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                break;
            }
            if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                if (wParam == VK_DELETE || wParam == VK_BACK) {
                    g_shapes.erase(g_shapes.begin() + g_selectedTextIndex);
                    g_selectedTextIndex = -1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
            }
            if (wParam == VK_ESCAPE) {
                SendMessage(hWnd, WM_CLOSE, 0, 0);
            }
            break;
        }
        case WM_CHAR: {
            if (g_isHoveringRoundRadius) {
                wchar_t ch = (wchar_t)wParam;
                if (ch >= L'0' && ch <= L'9') {
                    if (g_editingRoundRadiusStr.length() < 2) {
                        g_editingRoundRadiusStr += ch;
                        int val = wcstol(g_editingRoundRadiusStr.c_str(), NULL, 10);
                        if (val > 30) val = 30;
                        if (val < 0) val = 0;
                        if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                            g_shapes[g_selectedTextIndex].roundRadius = val;
                        } else {
                            g_currentRoundRadius = val;
                        }
                        SaveConfig();
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                break;
            }
            if (g_isEditingText) {
                wchar_t ch = (wchar_t)wParam;
                if (ch >= 32 && ch != 127) {
                    g_editingText.insert(g_editingCaretIndex, 1, ch);
                    g_editingCaretIndex++;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (g_selectedTextIndex >= 0 && g_selectedTextIndex < (int)g_shapes.size()) {
                auto& shp = g_shapes[g_selectedTextIndex];
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (shp.type == SHAPE_TEXT) {
                    if (delta > 0) {
                        shp.fontSize = min(120, shp.fontSize + 2);
                    } else {
                        shp.fontSize = max(8, shp.fontSize - 2);
                    }
                    shp.origW = 0.0f; // Force re-measure
                    g_currentFontSize = shp.fontSize;
                } else {
                    int currentThick = shp.thickness;
                    if (delta > 0) {
                        currentThick = min(15, currentThick + 1);
                    } else {
                        currentThick = max(1, currentThick - 1);
                    }
                    shp.thickness = currentThick;
                    g_currentThickness = currentThick;
                }
                InvalidateRect(hWnd, NULL, FALSE);
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
            ResetOverlaySessionState();
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

            // 释放马赛克预生成图层 (GDI+ 对象，须在 GdiplusShutdown 前销毁；先删画刷再删位图)
            delete g_mosaicTexBrush;
            g_mosaicTexBrush = NULL;
            delete g_mosaicBitmap;
            g_mosaicBitmap = NULL;
            
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
    static bool s_overlayClassRegistered = false;
    if (!s_overlayClassRegistered) {
        WNDCLASSEX wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcex.lpfnWndProc = OverlayWndProc;
        wcex.hInstance = g_hInstance;
        wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
        wcex.lpszClassName = L"CaptureToolOverlayClass";
        RegisterClassEx(&wcex);
        s_overlayClassRegistered = true;
    }
    
    ResetOverlaySessionState();

    // 马赛克图层基于上一会话的冻结截图，直接作废待重建（画刷引用图层，先删画刷）
    delete g_mosaicTexBrush;
    g_mosaicTexBrush = NULL;
    delete g_mosaicBitmap;
    g_mosaicBitmap = NULL;
    s_mosaicGenBlock = -1;
    s_mosaicGenStyle = -1;

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
    static HWND hCheckClipboard = NULL;
    static HWND hCheckNotification = NULL;
    static HWND hCheckWindowMode = NULL;
    static HWND hBtnSaveDir = NULL;
    static HWND hStaticSaveDir = NULL;
    static HWND hBtnSave = NULL;
    static HWND hStaticText = NULL;
    static HWND hBtnRecord = NULL;
    
    static bool s_suppress = false;
    static bool s_autostart = false;
    static bool s_clipboard = true;
    static bool s_notification = true;
    static bool s_windowMode = false;
    static wstring s_saveDirectory;
    
    switch (message) {
        case WM_CREATE: {
            s_suppress = g_config.hotkey.suppress;
            s_autostart = g_config.auto_start;
            s_clipboard = g_config.save_to_clipboard;
            s_notification = g_config.notification;
            s_windowMode = IsWindowCaptureMode();
            s_saveDirectory = g_config.save_directory;

            // 背景与基本 UI 控件创建 (极简现代暗黑风，去除任何丑陋的 Win32 控件背景)
            // 字体与背景刷只在首次打开时创建一次并复用，避免每次打开设置窗口泄漏 GDI 对象
            static HBRUSH s_hGrayBrush = NULL;
            static HFONT s_hFont = NULL;
            static HFONT s_hTextFont = NULL;
            if (!s_hGrayBrush) s_hGrayBrush = CreateSolidBrush(RGB(30, 30, 30));
            SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)s_hGrayBrush);

            if (!s_hFont) s_hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            if (!s_hTextFont) s_hTextFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            HFONT hFont = s_hFont;
            HFONT hTextFont = s_hTextFont;

            HWND hTitle = CreateWindow(L"STATIC", L"CaptureTool 设置", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 504, 30, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

            g_recordedHotkey = g_config.hotkey;
            wstring hotkeyText = L"当前快捷键: " + FormatHotkeyConfig(g_config.hotkey);
            hStaticText = CreateWindow(L"STATIC", hotkeyText.c_str(), WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 65, 504, 25, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hStaticText, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            hBtnRecord = CreateWindow(L"BUTTON", L"录入新快捷键", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 172, 105, 200, 38, hWnd, (HMENU)1002, g_hInstance, NULL);
            MakeButtonFlat(hBtnRecord);

            hCheckSuppress = CreateWindow(L"BUTTON", L"阻止其他应用接收此按键", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 165, 380, 28, hWnd, (HMENU)1003, g_hInstance, NULL);
            MakeButtonFlat(hCheckSuppress);

            hCheckAutostart = CreateWindow(L"BUTTON", L"开机自启动", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 200, 380, 28, hWnd, (HMENU)1004, g_hInstance, NULL);
            MakeButtonFlat(hCheckAutostart);

            hCheckClipboard = CreateWindow(L"BUTTON", L"截图后复制到剪贴板", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 235, 380, 28, hWnd, (HMENU)1005, g_hInstance, NULL);
            MakeButtonFlat(hCheckClipboard);

            hCheckNotification = CreateWindow(L"BUTTON", L"截图完成后显示托盘通知", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 270, 380, 28, hWnd, (HMENU)1006, g_hInstance, NULL);
            MakeButtonFlat(hCheckNotification);

            hCheckWindowMode = CreateWindow(L"BUTTON", L"默认窗口模式（关闭则为区域模式）", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 305, 420, 28, hWnd, (HMENU)1007, g_hInstance, NULL);
            MakeButtonFlat(hCheckWindowMode);

            wstring saveDirText = s_saveDirectory.empty() ? L"自动保存目录: 未设置" : (L"自动保存目录: " + s_saveDirectory);
            hStaticSaveDir = CreateWindow(L"STATIC", saveDirText.c_str(), WS_VISIBLE | WS_CHILD | SS_LEFT, 40, 350, 460, 40, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hStaticSaveDir, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            hBtnSaveDir = CreateWindow(L"BUTTON", L"选择自动保存目录", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 172, 395, 200, 38, hWnd, (HMENU)1008, g_hInstance, NULL);
            MakeButtonFlat(hBtnSaveDir);

            hBtnSave = CreateWindow(L"BUTTON", L"保存设置", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 172, 455, 200, 44, hWnd, (HMENU)1001, g_hInstance, NULL);
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
            
            if (id == 1001 || id == 1002 || id == 1008) { // 主按钮绘制：保存 / 录入 / 选择目录
                Color btnColor;
                if (id == 1001) { // 保存设置：现代高保真扁平绿
                    if (isPressed) btnColor = Color(255, 30, 130, 70);
                    else if (isHovered) btnColor = Color(255, 46, 204, 113);
                    else btnColor = Color(255, 39, 174, 96);
                } else { // 次级操作按钮：现代扁平深灰/蓝
                    if (isPressed) btnColor = Color(255, 30, 40, 50);
                    else if (isHovered) btnColor = Color(255, 52, 152, 219);
                    else btnColor = Color(255, 52, 73, 94);
                }
                
                // 圆角扁平按钮，加一圈同色系深色描边增加层次
                SolidBrush bBrush(btnColor);
                FillRoundedRectangle(g, &bBrush, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top), 6.0f);
                Pen btnBorderPen((id == 1001) ? Color(255, 26, 122, 68) : Color(255, 38, 56, 74), 1.0f);
                DrawRoundedRectangle(g, &btnBorderPen, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top), 6.0f);

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
            else if (id >= 1003 && id <= 1007) { // 复选框绘制
                bool isChecked = false;
                if (id == 1003) isChecked = s_suppress;
                else if (id == 1004) isChecked = s_autostart;
                else if (id == 1005) isChecked = s_clipboard;
                else if (id == 1006) isChecked = s_notification;
                else if (id == 1007) isChecked = s_windowMode;
                
                // 绘制自制方正精致暗黑复选框
                int boxSize = 16;
                int boxX = r.left;
                int boxY = r.top + (r.bottom - r.top - boxSize) / 2;
                
                if (isChecked) {
                    SolidBrush checkedBrush(Color(255, 0, 174, 255)); // 炫酷品牌蓝 #00AEFF
                    FillRoundedRectangle(g, &checkedBrush, (REAL)boxX, (REAL)boxY, (REAL)boxSize, (REAL)boxSize, 4.0f);

                    // 绘制纯白高清对号
                    Pen whitePen(Color(255, 255, 255, 255), 2.0f);
                    g.DrawLine(&whitePen, boxX + 4, boxY + 8, boxX + 7, boxY + 11);
                    g.DrawLine(&whitePen, boxX + 7, boxY + 11, boxX + 12, boxY + 4);
                } else {
                    Pen borderPen(isHovered ? Color(255, 180, 180, 180) : Color(255, 100, 100, 100), 1.5f);
                    DrawRoundedRectangle(g, &borderPen, (REAL)boxX, (REAL)boxY, (REAL)boxSize, (REAL)boxSize, 4.0f);
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
                g_config.save_to_clipboard = s_clipboard;
                g_config.notification = s_notification;
                g_config.capture_mode = s_windowMode ? L"window" : L"region";
                g_config.save_directory = s_saveDirectory;

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
            else if (LOWORD(wParam) == 1005) {
                s_clipboard = !s_clipboard;
                InvalidateRect(hCheckClipboard, NULL, FALSE);
            }
            else if (LOWORD(wParam) == 1006) {
                s_notification = !s_notification;
                InvalidateRect(hCheckNotification, NULL, FALSE);
            }
            else if (LOWORD(wParam) == 1007) {
                s_windowMode = !s_windowMode;
                InvalidateRect(hCheckWindowMode, NULL, FALSE);
            }
            else if (LOWORD(wParam) == 1008) {
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hWnd;
                bi.lpszTitle = L"选择截图自动保存目录";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t selectedPath[MAX_PATH] = { 0 };
                    if (SHGetPathFromIDListW(pidl, selectedPath)) {
                        s_saveDirectory = selectedPath;
                        wstring saveDirText = L"自动保存目录: " + s_saveDirectory;
                        SetWindowText(hStaticSaveDir, saveDirText.c_str());
                    }
                    CoTaskMemFree(pidl);
                }
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
        case WM_TIMER: {
            if (wParam == 2) {
                // 钩子看门狗：非截图会话期间若钩子缺失则重新装载
                if (!g_hWndOverlay && (!g_hMouseHook || !g_hKeyHook)) {
                    StartHooks();
                }
            }
            break;
        }
        case WM_TRAY_MSG: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_CAPTURE, L"开始截图");
                AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置中心");
                AppendMenu(hMenu, MF_STRING, ID_TRAY_AUTOSTART, L"开机自启动");
                AppendMenu(hMenu, MF_STRING, ID_TRAY_RELOADHOOKS, L"重新装载快捷键钩子");
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
                case ID_TRAY_RELOADHOOKS:
                    StopHooks();
                    StartHooks();
                    if (g_hMouseHook && g_hKeyHook) {
                        ShowTrayNotification(L"SnapCapture", L"快捷键钩子已重新装载。", NIIF_INFO);
                    } else {
                        ShowTrayNotification(L"SnapCapture", L"钩子装载失败，请尝试重启程序。", NIIF_WARNING);
                    }
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
                    static bool s_settingsClassRegistered = false;
                    if (!s_settingsClassRegistered) {
                        RegisterClassEx(&wcexS);
                        s_settingsClassRegistered = true;
                    }
                    
                    int sw = GetSystemMetrics(SM_CXSCREEN);
                    int sh = GetSystemMetrics(SM_CYSCREEN);
                    int wWidth = 560;
                    int wHeight = 560;
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
    SetLastError(ERROR_SUCCESS);
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
    SetTimer(g_hWndMain, 2, 5000, NULL); // 5s 钩子看门狗
    
    // 8. 消息泵循环运行
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 9. 清除环境并退出 (先释放字体缓存，再关闭 GDI+)
    for (size_t i = 0; i < g_fontCache.size(); ++i) {
        delete g_fontCache[i].second;
    }
    g_fontCache.clear();
    GdiplusShutdown(g_gdiplusToken);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
