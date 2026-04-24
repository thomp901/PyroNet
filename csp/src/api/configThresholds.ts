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
  l2TempThresh: 35,
  l2HumidityThresh: 40,
  l2VocThresh: 100,
  l3TempThresh: 45,
  l3HumidityThresh: 25,
  l3VocThresh: 200,
  l4VocThresh: 300,
  l5VocThresh: 500,
  l5Pm25Thresh: 35,
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
