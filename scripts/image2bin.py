import os
import struct
import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageOps

# ==========================================
# CONFIGURATION
# ==========================================
SCREEN_WIDTH = 240               # Your ST7789 screen width
SCREEN_HEIGHT = 240              # Your ST7789 screen height

# Most ESP32 SPI setups expect Big-Endian byte order for ST7789 screens. 
# If your colors look swapped (e.g., red looks blue), change this to False.
SWAP_BYTES = True                

def convert_to_rgb565(input_path, output_path, size, swap_bytes):
    try:
        # 1. Open the image and convert it to standard RGB
        img = Image.open(input_path).convert('RGB')
        
        # 2. Automatically resize and crop the image to fit the screen exactly 
        img = ImageOps.fit(img, size, Image.Resampling.LANCZOS)
        print(f"Image loaded and resized to {size[0]}x{size[1]}")
        
        pixels = img.load()
        
        # 3. Open the output binary file
        with open(output_path, 'wb') as bin_file:
            # 4. Iterate over every pixel
            for y in range(img.height):
                for x in range(img.width):
                    r, g, b = pixels[x, y]
                    
                    # Compress 24-bit RGB into 16-bit RGB565
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    
                    rgb565 = (r5 << 11) | (g6 << 5) | b5
                    
                    # Pack the 16-bit integer into 2 raw bytes
                    if swap_bytes:
                        byte_data = struct.pack('>H', rgb565)
                    else:
                        byte_data = struct.pack('<H', rgb565)
                        
                    bin_file.write(byte_data)
                    
        print(f"Success! Saved binary to:\n{output_path}")
        print(f"File size: {os.path.getsize(output_path)} bytes.")
        
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # Initialize tkinter but hide the empty main window
    root = tk.Tk()
    root.withdraw()

    print("Opening file dialog... Please select an image.")

    # Open the system file explorer
    file_path = filedialog.askopenfilename(
        title="Select an Image to Convert",
        filetypes=[
            ("Image Files", "*.png;*.jpg;*.jpeg;*.bmp"),
            ("All Files", "*.*")
        ]
    )

    # Check if the user selected a file or hit "Cancel"
    if not file_path:
        print("No file selected. Exiting...")
    else:
        print(f"Selected: {file_path}")
        
        # Generate the output filename automatically
        # e.g., "C:/folder/my_photo.jpg" -> "C:/folder/my_photo.bin"
        base_name = os.path.splitext(file_path)[0]
        output_bin = f"{base_name}.bin"

        # Run the conversion
        convert_to_rgb565(file_path, output_bin, (SCREEN_WIDTH, SCREEN_HEIGHT), SWAP_BYTES)