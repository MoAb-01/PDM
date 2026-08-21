import sys
import os
from PIL import Image

def make_transparent(input_path, output_png, output_ico):
    img = Image.open(input_path).convert("RGBA")
    datas = img.getdata()

    # Flood fill or threshold to make pure white background transparent
    # The outer background is white (#ffffff or >240)
    width, height = img.size
    pixels = img.load()

    # Use flood fill from corners (0,0), (width-1, 0), (0, height-1), (width-1, height-1)
    # to convert outer white background to transparent while keeping interior white intact
    from collections import deque

    visited = set()
    queue = deque([(0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)])

    for pt in list(queue):
        visited.add(pt)

    while queue:
        x, y = queue.popleft()
        r, g, b, a = pixels[x, y]

        # If it is white/near-white (background), turn transparent and expand
        if r > 230 and g > 230 and b > 230:
            pixels[x, y] = (0, 0, 0, 0)

            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and (nx, ny) not in visited:
                    visited.add((nx, ny))
                    nr, ng, nb, na = pixels[nx, ny]
                    # expand if light colored (outer background)
                    if nr > 210 and ng > 210 and nb > 210:
                        queue.append((nx, ny))

    # Save transparent PNG
    img.save(output_png, "PNG")

    # Generate multi-size ICO
    icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(output_ico, format="ICO", sizes=icon_sizes)

    # Also save extension icons
    ext_dir = os.path.join(os.path.dirname(output_png), "src", "extension")
    if os.path.exists(ext_dir):
        img.resize((16, 16), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon16.png"), "PNG")
        img.resize((48, 48), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon48.png"), "PNG")
        img.resize((128, 128), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon128.png"), "PNG")

    print(f"Successfully generated transparent icon: {output_png} and {output_ico}")

if __name__ == "__main__":
    src = r"C:\Users\UHD\.gemini\antigravity-ide\brain\39e5b59e-e367-4afe-ad75-46915925f582\.user_uploaded\media_1787271258747.png"
    out_png = r"d:\Download Manager AB\app_icon.png"
    out_ico = r"d:\Download Manager AB\app_icon.ico"
    make_transparent(src, out_png, out_ico)
