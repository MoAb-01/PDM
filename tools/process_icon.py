import sys
import os
import shutil
from PIL import Image
from collections import deque

def make_transparent(input_path, output_png, output_ico):
    img = Image.open(input_path).convert("RGBA")
    width, height = img.size
    pixels = img.load()

    # Flood fill from corners to convert outer white background to transparent
    visited = set()
    queue = deque([(0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)])

    for pt in list(queue):
        visited.add(pt)

    while queue:
        x, y = queue.popleft()
        r, g, b, a = pixels[x, y]

        if r > 225 and g > 225 and b > 225:
            pixels[x, y] = (0, 0, 0, 0)
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and (nx, ny) not in visited:
                    visited.add((nx, ny))
                    nr, ng, nb, na = pixels[nx, ny]
                    if nr > 200 and ng > 200 and nb > 200:
                        queue.append((nx, ny))

    # Pad onto a standard square 256x256 canvas so Windows .ico entries are 1:1 square
    max_dim = max(width, height)
    scale = 240.0 / max_dim
    new_w = int(width * scale)
    new_h = int(height * scale)
    scaled_img = img.resize((new_w, new_h), Image.Resampling.LANCZOS)

    sq_img = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    offset_x = (256 - new_w) // 2
    offset_y = (256 - new_h) // 2
    sq_img.paste(scaled_img, (offset_x, offset_y), scaled_img)

    # Save square PNG and multi-size ICO
    sq_img.save(output_png, "PNG")

    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    sq_img.save(output_ico, format="ICO", sizes=sizes)

    # Copy to cpp directory
    cpp_dir = os.path.join(os.path.dirname(output_png), "cpp")
    if os.path.exists(cpp_dir):
        shutil.copy(output_png, os.path.join(cpp_dir, "app_icon.png"))
        shutil.copy(output_ico, os.path.join(cpp_dir, "app_icon.ico"))

    # Also save extension icons
    ext_dir = os.path.join(os.path.dirname(output_png), "src", "extension")
    if os.path.exists(ext_dir):
        sq_img.resize((16, 16), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon16.png"), "PNG")
        sq_img.resize((48, 48), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon48.png"), "PNG")
        sq_img.resize((128, 128), Image.Resampling.LANCZOS).save(os.path.join(ext_dir, "icon128.png"), "PNG")

    print(f"Successfully generated transparent square icons: {output_png} and {output_ico}")

if __name__ == "__main__":
    src = r"C:\Users\UHD\.gemini\antigravity-ide\brain\7fbc562b-df97-4f3c-8ca8-96bb20a035d9\.user_uploaded\media_1787516943788.png"
    out_png = r"d:\Download Manager AB\app_icon.png"
    out_ico = r"d:\Download Manager AB\app_icon.ico"
    make_transparent(src, out_png, out_ico)
