import { useSyncExternalStore } from "react";

export type TemperatureUnit = "C" | "F";

const storageKey = "pyronet-temperature-display-unit";
const defaultTemperatureUnit: TemperatureUnit = "C";
const listeners = new Set<() => void>();

function isTemperatureUnit(value: string | null): value is TemperatureUnit {
  return value === "C" || value === "F";
}

function readStoredTemperatureUnit(): TemperatureUnit {
  if (typeof window === "undefined") {
    return defaultTemperatureUnit;
  }

  const storedValue = window.localStorage.getItem(storageKey);
  return isTemperatureUnit(storedValue) ? storedValue : defaultTemperatureUnit;
}

let currentTemperatureUnit = readStoredTemperatureUnit();

function emitChange() {
  listeners.forEach((listener) => listener());
}

export function getTemperatureUnit() {
  return currentTemperatureUnit;
}

export function setTemperatureUnit(nextTemperatureUnit: TemperatureUnit) {
  if (currentTemperatureUnit === nextTemperatureUnit) {
    return;
  }

  currentTemperatureUnit = nextTemperatureUnit;

  if (typeof window !== "undefined") {
    window.localStorage.setItem(storageKey, nextTemperatureUnit);
  }

  emitChange();
}

function subscribe(listener: () => void) {
  listeners.add(listener);

  return () => {
    listeners.delete(listener);
  };
}

export function useTemperatureUnit() {
  return useSyncExternalStore(subscribe, getTemperatureUnit, () => defaultTemperatureUnit);
}
