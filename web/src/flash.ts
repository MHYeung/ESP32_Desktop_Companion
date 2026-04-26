import {
  ESPLoader,
  Transport,
  type FlashOptions,
  type IEspLoaderTerminal,
} from "esptool-js";
import type { BoardProfile } from "./types";

export interface FlashCallbacks {
  log: (message: string) => void;
  progress: (percent: number) => void;
}

async function fetchBinary(path: string): Promise<Uint8Array> {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`Missing firmware artifact: ${path}`);
  return new Uint8Array(await response.arrayBuffer());
}

export async function flashDevice(
  profile: BoardProfile,
  assetsImage: Uint8Array,
  callbacks: FlashCallbacks,
): Promise<void> {
  const serial = (navigator as Navigator & { serial?: Serial }).serial;
  if (!serial) throw new Error("Web Serial is unavailable. Use Chrome or Edge over HTTPS.");

  const firmwareParts = await Promise.all(
    profile.firmwareParts.map(async (part) => ({
      address: part.offset,
      data: await fetchBinary(part.path),
    })),
  );
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

    const options: FlashOptions = {
      fileArray: firmwareParts,
      flashMode: profile.flashMode,
      flashFreq: profile.flashFreq,
      flashSize: profile.flashSize,
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
