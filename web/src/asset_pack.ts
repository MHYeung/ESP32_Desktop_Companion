import type { BoardProfile, UserFlashConfig } from "./types";
import type { ConvertedImage } from "./image_convert";

export const ASSET_HEADER_SIZE = 256;
export const ASSET_FORMAT_VERSION = 1;

const OFFSETS = {
  magic: 0,
  version: 4,
  headerSize: 6,
  width: 8,
  height: 10,
  imageCount: 12,
  imageSize: 16,
  rotationInterval: 20,
  pomodoroSeconds: 24,
  assetId: 28,
  flags: 32,
  timezone: 36,
  ssid: 100,
  password: 133,
};

function writeCString(target: Uint8Array, offset: number, maxLen: number, value: string): void {
  const encoded = new TextEncoder().encode(value);
  target.set(encoded.slice(0, maxLen - 1), offset);
}

export function maxPhotosFor(profile: BoardProfile): number {
  const imageSize = profile.width * profile.height * 2;
  return Math.floor((profile.assetsPartitionSize - ASSET_HEADER_SIZE) / imageSize);
}

export function buildAssetPack(
  profile: BoardProfile,
  config: UserFlashConfig,
  images: ConvertedImage[],
): Uint8Array {
  const imageSize = profile.width * profile.height * 2;
  if (images.length === 0) throw new Error("Add at least one photo before flashing");
  if (images.length > maxPhotosFor(profile)) {
    throw new Error(`Too many photos for ${profile.name}; max is ${maxPhotosFor(profile)}`);
  }

  const totalSize = ASSET_HEADER_SIZE + images.length * imageSize;
  const buffer = new Uint8Array(totalSize);
  const view = new DataView(buffer.buffer);

  buffer.set(new TextEncoder().encode("DCAS"), OFFSETS.magic);
  view.setUint16(OFFSETS.version, ASSET_FORMAT_VERSION, true);
  view.setUint16(OFFSETS.headerSize, ASSET_HEADER_SIZE, true);
  view.setUint16(OFFSETS.width, profile.width, true);
  view.setUint16(OFFSETS.height, profile.height, true);
  view.setUint16(OFFSETS.imageCount, images.length, true);
  view.setUint32(OFFSETS.imageSize, imageSize, true);
  view.setUint32(OFFSETS.rotationInterval, config.rotationIntervalSec, true);
  view.setUint32(OFFSETS.pomodoroSeconds, config.pomodoroSeconds, true);
  view.setUint32(OFFSETS.assetId, Math.floor(Date.now() / 1000), true);
  view.setUint32(OFFSETS.flags, 0, true);

  view.setInt32(198, config.weatherLatE6 | 0, true);
  view.setInt32(202, config.weatherLonE6 | 0, true);

  writeCString(buffer, OFFSETS.timezone, 64, config.timezone || "UTC0");
  writeCString(buffer, OFFSETS.ssid, 33, config.ssid);
  writeCString(buffer, OFFSETS.password, 65, config.password);

  images.forEach((image, index) => {
    buffer.set(image.data, ASSET_HEADER_SIZE + index * imageSize);
  });
  return buffer;
}
