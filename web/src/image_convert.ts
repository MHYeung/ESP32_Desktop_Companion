import type { BoardProfile } from "./types";

export interface ConvertedImage {
  name: string;
  data: Uint8Array;
  previewUrl: string;
}

export async function convertImageFile(
  file: File,
  profile: BoardProfile,
  brightness = 0.85,
): Promise<ConvertedImage> {
  const bitmap = await createImageBitmap(file);
  const canvas = document.createElement("canvas");
  canvas.width = profile.width;
  canvas.height = profile.height;
  const ctx = canvas.getContext("2d", { willReadFrequently: true });
  if (!ctx) throw new Error("Canvas 2D context is unavailable");

  const scale = Math.max(profile.width / bitmap.width, profile.height / bitmap.height);
  const dw = bitmap.width * scale;
  const dh = bitmap.height * scale;
  ctx.drawImage(bitmap, (profile.width - dw) / 2, (profile.height - dh) / 2, dw, dh);

  const pixels = ctx.getImageData(0, 0, profile.width, profile.height).data;
  const out = new Uint8Array(profile.width * profile.height * 2);
  for (let i = 0, j = 0; i < pixels.length; i += 4, j += 2) {
    const r = Math.round(pixels[i] * brightness);
    const g = Math.round(pixels[i + 1] * brightness);
    const b = Math.round(pixels[i + 2] * brightness);
    const rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    out[j] = profile.rgb565ByteOrder === "big" ? rgb565 >> 8 : rgb565 & 0xff;
    out[j + 1] = profile.rgb565ByteOrder === "big" ? rgb565 & 0xff : rgb565 >> 8;
  }

  return { name: file.name, data: out, previewUrl: canvas.toDataURL("image/jpeg", 0.85) };
}
