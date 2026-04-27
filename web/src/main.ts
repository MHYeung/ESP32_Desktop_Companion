import { buildAssetPack, maxPhotosFor } from "./asset_pack";
import { convertImageFile, type ConvertedImage } from "./image_convert";
import { flashDevice } from "./flash";
import type { BoardProfile, UserFlashConfig } from "./types";
import boardProfiles from "../boards/index.json";
import { presetForTz, TIMEZONE_PRESETS, type TimezonePreset } from "./timezone_presets";
import "./styles.css";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const field = (id: string) => $<HTMLInputElement | HTMLSelectElement>(id);

let profiles: BoardProfile[] = [];
let activeProfile: BoardProfile;
let convertedImages: ConvertedImage[] = [];

function appendLog(message: string): void {
  const log = $("log");
  log.textContent += `${message}\n`;
  log.scrollTop = log.scrollHeight;
}

function bytes(value: number): string {
  return `${(value / 1024).toFixed(1)} KB`;
}

function readConfig(): UserFlashConfig {
  const tz = field("timezone").value.trim() || "UTC0";
  const geo = presetForTz(tz);
  return {
    ssid: field("ssid").value.trim(),
    password: field("password").value,
    timezone: tz,
    rotationIntervalSec: Number(field("rotation").value || 60),
    pomodoroSeconds: Number(field("pomodoro").value || 1500),
    weatherLatE6: geo.latE6,
    weatherLonE6: geo.lonE6,
  };
}

function renderUsage(): void {
  const imageBytes = activeProfile.width * activeProfile.height * 2;
  const used = 256 + convertedImages.length * imageBytes;
  $("usage").textContent =
    `${convertedImages.length}/${maxPhotosFor(activeProfile)} photos, ` +
    `${bytes(used)} of ${bytes(activeProfile.assetsPartitionSize)} assets partition`;
}

function renderPreviews(): void {
  const previews = $("previews");
  previews.innerHTML = "";
  for (const image of convertedImages) {
    const card = document.createElement("figure");
    card.innerHTML = `<img src="${image.previewUrl}" alt=""><figcaption>${image.name}</figcaption>`;
    previews.appendChild(card);
  }
  renderUsage();
}

function fillTimezoneSelect(): void {
  const sel = field("timezone") as HTMLSelectElement;
  const byGroup = new Map<string, TimezonePreset[]>();
  for (const p of TIMEZONE_PRESETS) {
    const list = byGroup.get(p.group) ?? [];
    list.push(p);
    byGroup.set(p.group, list);
  }
  sel.innerHTML = "";
  for (const [groupName, presets] of byGroup) {
    const og = document.createElement("optgroup");
    og.label = groupName;
    for (const p of presets) {
      const opt = document.createElement("option");
      opt.value = p.value;
      opt.textContent = p.label;
      og.appendChild(opt);
    }
    sel.appendChild(og);
  }
  sel.value = TIMEZONE_PRESETS[0]?.value ?? "UTC0";
}

async function loadProfiles(): Promise<void> {
  profiles = boardProfiles as BoardProfile[];
  const select = $("board") as HTMLSelectElement;
  select.innerHTML = profiles.map((p) => `<option value="${p.id}">${p.name}</option>`).join("");
  activeProfile = profiles[0];
  select.addEventListener("change", () => {
    activeProfile = profiles.find((p) => p.id === select.value) ?? profiles[0];
    convertedImages = [];
    renderPreviews();
  });
  fillTimezoneSelect();
  renderUsage();
}

async function handlePhotos(files: FileList | null): Promise<void> {
  if (!files?.length) return;
  $("previews").textContent = "Converting photos...";
  convertedImages = [];
  for (const file of Array.from(files)) {
    convertedImages.push(await convertImageFile(file, activeProfile));
  }
  renderPreviews();
}

async function handleFlash(): Promise<void> {
  $("log").textContent = "";
  const config = readConfig();
  if (!config.ssid) throw new Error("SSID is required");
  const assets = buildAssetPack(activeProfile, config, convertedImages);
  appendLog(`Generated assets image: ${bytes(assets.byteLength)}`);
  await flashDevice(activeProfile, assets, {
    log: appendLog,
    progress: (percent) => ($("progress").textContent = `${percent}%`),
  });
  appendLog("Flash complete. Device is resetting.");
}

async function init(): Promise<void> {
  if (!("serial" in navigator)) {
    $("support").textContent = "Web Serial unavailable. Use Chrome or Edge over HTTPS.";
  }
  await loadProfiles();
  $("photos").addEventListener("change", (event) => {
    void handlePhotos((event.target as HTMLInputElement).files).catch((err) => appendLog(err.message));
  });
  $("flash").addEventListener("click", () => {
    void handleFlash().catch((err) => appendLog(`ERROR: ${err.message}`));
  });
}

void init();
