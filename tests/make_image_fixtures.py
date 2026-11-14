from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageOps, ImageSequence
from PIL.PngImagePlugin import PngInfo


fixtures = Path(__file__).parent / "fixtures"
image = Image.new("RGBA", (13, 7))
for y in range(image.height):
    for x in range(image.width):
        image.putpixel((x, y), ((x * 29 + y * 7) % 256, (x * 13 + y * 37) % 256,
                              (x * 31 + y * 19) % 256, (x * 47 + y * 41) % 256))
image.save(fixtures / "pipeline-source.png")
metadata = PngInfo()
metadata.add_text("ignored", "x" * 2_000_000, zip=True)
image.save(fixtures / "pipeline-metadata.png", pnginfo=metadata)
normalized = ImageOps.pad(image, (512, 512), method=Image.Resampling.LANCZOS, color=(0, 0, 0, 0))
normalized.save(fixtures / "pipeline-normalized.png")
backdrop = Image.new("RGBA", (512, 512), (32, 48, 64, 255))
backdrop.alpha_composite(normalized)
enhanced = ImageEnhance.Brightness(ImageEnhance.Contrast(backdrop.convert("RGB")).enhance(1.3)).enhance(.8)
enhanced.save(fixtures / "pipeline-enhanced.png")
enhanced.resize((256, 256), Image.Resampling.LANCZOS).save(fixtures / "pipeline-preview.png")
background = image.resize((7, 13))
background.save(fixtures / "pipeline-background.png")
ImageOps.fit(background, (512, 512), method=Image.Resampling.LANCZOS).save(fixtures / "pipeline-fit.png")

exif = Image.Exif()
exif[274] = 6
image.convert("RGB").save(fixtures / "orientation.jpg", exif=exif, quality=98, subsampling=0)
image.save(fixtures / "orientation.png", exif=exif)
ImageOps.exif_transpose(Image.open(fixtures / "orientation.png")).save(fixtures / "orientation-expected.png")

frames = []
for bounds in ((0, 0, 2, 2), (2, 1, 4, 3), (1, 2, 3, 4)):
    frame = Image.new("RGBA", (6, 5), (0, 0, 0, 0))
    ImageDraw.Draw(frame).rectangle(bounds, fill=(20, 50, 90, 255))
    frames.append(frame)
for image_format, suffix in (("GIF", "gif"), ("PNG", "apng"), ("WEBP", "webp")):
    path = fixtures / f"composition.{suffix}"
    frames[0].save(path, format=image_format, save_all=True, append_images=frames[1:],
                   duration=[90, 170, 230], loop=3, disposal=2, lossless=True)
    with Image.open(path) as animation:
        for index, frame in enumerate(ImageSequence.Iterator(animation)):
            frame.convert("RGBA").save(fixtures / f"composition-{suffix}-{index}.png")
poster = Image.new("RGBA", (6, 5), (200, 220, 240, 255))
poster.save(fixtures / "poster.apng", format="PNG", default_image=True, save_all=True,
            append_images=frames, duration=[90, 170, 230], loop=3, disposal=2)
with Image.open(fixtures / "poster.apng") as animation:
    for index, frame in enumerate(ImageSequence.Iterator(animation)):
        frame.convert("RGBA").save(fixtures / f"poster-{index}.png")
