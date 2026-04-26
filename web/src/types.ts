import type { FlashFreqValues, FlashModeValues, FlashSizeValues } from "esptool-js";

export interface FirmwarePart {
  name: string;
  path: string;
  offset: number;
}

export interface BoardProfile {
  id: string;
  name: string;
  chipFamily: string;
  width: number;
  height: number;
  rgb565ByteOrder: "big" | "little";
  assetsOffset: number;
  assetsPartitionSize: number;
  flashMode: FlashModeValues;
  flashFreq: FlashFreqValues;
  flashSize: FlashSizeValues;
  baudRate: number;
  firmwareParts: FirmwarePart[];
}

export interface UserFlashConfig {
  ssid: string;
  password: string;
  timezone: string;
  rotationIntervalSec: number;
  countdownSeconds: number;
}
