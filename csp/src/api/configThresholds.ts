import type { ConfigThresholds } from "./types";

export const configThresholdKeys = [
  "l2TempThresh",
  "l2HumidityThresh",
  "l2VocThresh",
  "l3TempThresh",
  "l3HumidityThresh",
  "l3VocThresh",
  "l4VocThresh",
  "l5VocThresh",
  "l5Pm25Thresh",
] as const satisfies ReadonlyArray<keyof ConfigThresholds>;

export const DEFAULT_CONFIG_THRESHOLDS: ConfigThresholds = {
  l2TempThresh: 32,
  l2HumidityThresh: 33,
  l2VocThresh: 110,
  l3TempThresh: 36,
  l3HumidityThresh: 23,
  l3VocThresh: 165,
  l4VocThresh: 225,
  l5VocThresh: 255,
  l5Pm25Thresh: 58,
};

export function cloneConfigThresholds(thresholds: ConfigThresholds = DEFAULT_CONFIG_THRESHOLDS): ConfigThresholds {
  return { ...thresholds };
}

export function mergeConfigThresholds(thresholds?: Partial<ConfigThresholds> | null): ConfigThresholds {
  return {
    ...DEFAULT_CONFIG_THRESHOLDS,
    ...(thresholds ?? {}),
  };
}
