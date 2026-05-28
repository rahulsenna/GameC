import sys
from PIL import Image

def main():
    try:
        img = Image.open("../assets/alien-panels-bl/alien-panels_metallic.png")
        img = img.convert("L")
        pixels = list(img.getdata())
        avg = sum(pixels) / len(pixels)
        print(f"Average metallic value: {avg/255.0:.2f} (0 is non-metal, 1 is full metal)")
    except Exception as e:
        print(e)

if __name__ == "__main__":
    main()
