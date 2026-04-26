import {
  ESPLoader,
  Transport,
  type FlashOptions,
  type IEspLoaderTerminal,
} from "esptool-js";
import type { BoardProfile, FirmwareManifest, FirmwarePart } from "./types";

export interface FlashCallbacks {
  log: (message: string) => void;
  progress: (percent: number) => void;
}

async function fetchBinary(part: FirmwarePart): Promise<Uint8Array> {
  const path = part.path;
  const response = await fetch(path);
  if (!response.ok) throw new Error(`Missing firmware artifact: ${path}`);
  const data = new Uint8Array(await response.arrayBuffer());
  if (part.espImage !== false && data[0] !== 0xe9) {
    throw new Error(`Firmware artifact is not an ESP image: ${path}`);
  }
  return data;
}

async function loadFirmwareManifest(profile: BoardProfile): Promise<FirmwareManifest> {
  if (!profile.firmwareManifestPath) {
    return { parts: profile.firmwareParts };
  }
  const response = await fetch(`${profile.firmwareManifestPath}?t=${Date.now()}`);
  if (!response.ok) {
    throw new Error(`Missing firmware manifest: ${profile.firmwareManifestPath}`);
  }
  return response.json() as Promise<FirmwareManifest>;
}

function normalizeChipName(chip: string): string {
  return chip.toUpperCase().replace(/[^A-Z0-9]/g, "");
}

function chipMatchesProfile(chip: string, expectedFamily: string): boolean {
  return normalizeChipName(chip).startsWith(normalizeChipName(expectedFamily));
}

export async function flashDevice(
  profile: BoardProfile,
  assetsImage: Uint8Array,
  callbacks: FlashCallbacks,
): Promise<void> {
  const serial = (navigator as Navigator & { serial?: Serial }).serial;
  if (!serial) throw new Error("Web Serial is unavailable. Use Chrome or Edge over HTTPS.");

  const manifest = await loadFirmwareManifest(profile);
  const firmwareParts = await Promise.all(
    manifest.parts.map(async (part) => {
      const data = await fetchBinary(part);
      callbacks.log(`Queued ${part.name}: 0x${part.offset.toString(16)} (${data.byteLength} bytes)`);
      return { address: part.offset, data };
    }),
  );
  callbacks.log(`Queued assets: 0x${profile.assetsOffset.toString(16)} (${assetsImage.byteLength} bytes)`);
  firmwareParts.push({ address: profile.assetsOffset, data: assetsImage });

  const port = await serial.requestPort();
  const transport = new Transport(port, true);
  const terminal: IEspLoaderTerminal = {
    clean: () => undefined,
    writeLine: (data) => callbacks.log(data),
    write: (data) => callbacks.log(data),
  };

  try {
    const loader = new ESPLoader({ transport, baudrate: profile.baudRate, terminal });
    const chip = await loader.main("default_reset");
    callbacks.log(`Connected to ${chip}`);
    if (!chipMatchesProfile(chip, profile.chipFamily)) {
      throw new Error(`Selected ${profile.name}, but connected chip is ${chip}`);
    }

    const options: FlashOptions = {
      fileArray: firmwareParts,
      flashMode: manifest.flashMode ?? profile.flashMode,
      flashFreq: manifest.flashFreq ?? profile.flashFreq,
      flashSize: manifest.flashSize ?? profile.flashSize,
      eraseAll: true,
      compress: true,
      reportProgress: (_fileIndex, written, total) => {
        callbacks.progress(Math.round((written / total) * 100));
      },
    };
    await loader.writeFlash(options);
    await loader.after("hard_reset");
  } finally {
    await transport.disconnect().catch(() => undefined);
  }
}
