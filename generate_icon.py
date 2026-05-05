"""生成应用图标"""
from PIL import Image, ImageDraw, ImageFont
import os

def create_icon():
    """创建 SnapCapture 应用图标"""
    sizes = [256, 128, 64, 48, 32, 16]
    
    # 创建最大尺寸版本
    size = 256
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 圆角矩形背景（蓝色渐变模拟）
    margin = 10
    for y in range(margin, size - margin):
        ratio = (y - margin) / (size - 2 * margin)
        r = int(30 + ratio * 0)
        g = int(144 - ratio * 44)
        b = int(255 - ratio * 55)
        draw.line([(margin, y), (size - margin, y)], fill=(r, g, b, 255))
    
    # 圆角遮罩
    mask = Image.new("L", (size, size), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=40, fill=255
    )
    img.putalpha(mask)
    
    # 在背景上绘制截屏图标
    draw = ImageDraw.Draw(img)
    
    # 外框（屏幕形状）
    sx, sy = 55, 60
    sw, sh = 146, 110
    draw.rounded_rectangle(
        [sx, sy, sx + sw, sy + sh],
        radius=8, outline="white", width=5
    )
    
    # 十字准星
    cx, cy = sx + sw // 2, sy + sh // 2
    cross_size = 25
    draw.line([(cx - cross_size, cy), (cx + cross_size, cy)], fill="white", width=3)
    draw.line([(cx, cy - cross_size), (cx, cy + cross_size)], fill="white", width=3)
    
    # 准星中心圆
    cr = 6
    draw.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], outline="white", width=2)
    
    # 右下角的裁切指示角
    corner_x, corner_y = sx + sw - 5, sy + sh - 5
    draw.polygon([
        (corner_x, corner_y - 30),
        (corner_x, corner_y),
        (corner_x - 30, corner_y)
    ], fill=(255, 255, 255, 180))
    
    # 保存为 ICO（多尺寸）
    icon_dir = os.path.join(os.path.dirname(__file__), "src", "resources", "icons")
    os.makedirs(icon_dir, exist_ok=True)
    
    # 生成各尺寸
    icon_images = []
    for s in sizes:
        resized = img.resize((s, s), Image.Resampling.LANCZOS)
        icon_images.append(resized)
    
    ico_path = os.path.join(icon_dir, "app.ico")
    png_path = os.path.join(icon_dir, "app.png")
    
    # 保存 ICO
    img.save(ico_path, format="ICO", sizes=[(s, s) for s in sizes])
    # 保存 PNG
    img.save(png_path, format="PNG")
    
    print(f"图标已生成: {ico_path}")
    print(f"PNG 已生成: {png_path}")

if __name__ == "__main__":
    create_icon()
