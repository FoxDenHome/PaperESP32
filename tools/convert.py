#!/usr/bin/env python3

from PIL import Image
from argparse import ArgumentParser
from io import BytesIO
from base64 import b64encode

"""
1.	Black	0b000	0x0	0,0,0
2.	White	0b001	0x1	255,255,255
3.	Green	0b010	0x2	0,255,0
4.	Blue	0b011	0x3	0,0,255
5.	Red	0b100	0x4	255,0,0
6.	Yellow	0b101	0x5	255,255,0
7.	Orange	0b110	0x6	255,128,0
8.      Secret purple/pink-ish
"""

#PALLETTE = [
#    0, 0, 0,
#    255, 255, 255,
#    0, 255, 0,
#    0, 0, 255,
#    255, 0, 0,
#    255, 255, 0,
#    255, 128, 0,
#]

PALLETTE = [
	0, 0, 0,
	255, 255, 255,
	67, 138, 28,
	100, 64, 255,
	191, 0, 0,
	255, 243, 56,
	232, 126, 0,
	194 ,164, 244,
]

TARGET_WIDTH = 600
TARGET_HEIGHT = 448
TARGET_RATIO = TARGET_WIDTH / TARGET_HEIGHT

def main():
    parser = ArgumentParser()
    parser.add_argument("src", help="Source image")
    parser.add_argument("dst", help="Destination file")
    parser.add_argument("--save-intermediate", help="Save intermediate file", default="")
    parser.add_argument("--show", help="Show image preview", action="store_true", default=False)
    args = parser.parse_args()

    tmp_byte = bytearray()

    print("Loading image...")
    with Image.open(args.src) as img:
        palette_img = Image.new("P", (1, 1))
        palette_img.putpalette(PALLETTE)

        print("Cropping image to target ratio...")
        image_ratio = img.width / img.height
        if image_ratio > TARGET_RATIO:
            target_width = img.height * TARGET_RATIO
            img = img.crop(((img.width - target_width) / 2, 0, (img.width + target_width) / 2, img.height))
        elif image_ratio < TARGET_RATIO:
            target_height = img.width * (1.0 / TARGET_RATIO)
            img = img.crop((0, (img.height - target_height) / 2, img.width, (img.height + target_height) / 2))

        print("Resizing image...")
        img = img.resize((TARGET_WIDTH, TARGET_HEIGHT))

        print("Quantizing image...")
        processed_img = img.quantize(palette=palette_img, dither=Image.Dither.FLOYDSTEINBERG)

    print("Converting to binary...")

    for y in range(0, TARGET_HEIGHT):
        for x in range(0, TARGET_WIDTH, 2):
            tmp_byte.append(processed_img.getpixel((x, y)) | (processed_img.getpixel((x + 1, y)) << 4))

    if args.save_intermediate:
        print("Saving intermediate file...")
        processed_img.save(args.save_intermediate)

    if args.show:
        print("Preview: ", flush=True)
        b = BytesIO()
        processed_img.save(b, "PNG")
        data = b.getvalue()
        b64data = b64encode(data).decode("utf-8")
        print(f"\033]1337;File=inline=1;name={args.src}-epaper.png;size={len(data)}:{b64data}\a", flush=True)

    print("Saving binary file...")
    with open(args.dst, "wb") as out:
        out.write(tmp_byte)

    print("Done!")

if __name__ == "__main__":
    main()
