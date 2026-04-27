/** POSIX TZ string → label (place — UTC offset) and coordinates for Open-Meteo. */

export interface TimezonePreset {
  group: string;
  value: string;
  label: string;
  latE6: number;
  lonE6: number;
}

function e6(lat: number, lon: number): Pick<TimezonePreset, "latE6" | "lonE6"> {
  return { latE6: Math.round(lat * 1e6), lonE6: Math.round(lon * 1e6) };
}

export const TIMEZONE_PRESETS: TimezonePreset[] = [
  {
    group: "Europe / UTC",
    value: "UTC0",
    label: "Prime Meridian — UTC+0",
    ...e6(51.4779, -0.0015),
  },
  {
    group: "Europe / UTC",
    value: "GMT0BST,M3.5.0/1,M10.5.0",
    label: "London — UTC+0 / +1 (DST)",
    ...e6(51.5074, -0.1278),
  },
  {
    group: "Europe / UTC",
    value: "CET-1CEST,M3.5.0,M10.5.0/3",
    label: "Paris / Berlin — UTC+1 / +2 (DST)",
    ...e6(52.52, 13.405),
  },
  { group: "Asia Pacific", value: "CST-8", label: "Taipei — UTC+8", ...e6(25.033, 121.565) },
  { group: "Asia Pacific", value: "JST-9", label: "Tokyo — UTC+9", ...e6(35.6762, 139.6503) },
  {
    group: "Asia Pacific",
    value: "AEST-10AEDT,M10.1.0,M4.1.0/3",
    label: "Sydney — UTC+10 / +11 (DST)",
    ...e6(-33.8688, 151.2093),
  },
  {
    group: "Americas",
    value: "PST8PDT,M3.2.0,M11.1.0",
    label: "Los Angeles — UTC−8 / −7 (DST)",
    ...e6(34.0522, -118.2437),
  },
  {
    group: "Americas",
    value: "MST7MDT,M3.2.0,M11.1.0",
    label: "Denver — UTC−7 / −6 (DST)",
    ...e6(39.7392, -104.9903),
  },
  {
    group: "Americas",
    value: "CST6CDT,M3.2.0,M11.1.0",
    label: "Chicago — UTC−6 / −5 (DST)",
    ...e6(41.8781, -87.6298),
  },
  {
    group: "Americas",
    value: "EST5EDT,M3.2.0,M11.1.0",
    label: "New York — UTC−5 / −4 (DST)",
    ...e6(40.7128, -74.006),
  },
];

export function presetForTz(value: string): TimezonePreset {
  const v = value.trim() || "UTC0";
  return TIMEZONE_PRESETS.find((p) => p.value === v) ?? TIMEZONE_PRESETS[0];
}
