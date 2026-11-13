import { readFileSync } from "node:fs";
import { resolve } from "node:path";

export interface FirmwarePart {
  path: string;
  offset: number;
  size: number;
  sha256: string;
}

export interface Board {
  id: string;
  label: string;
  chip: string;
  buttons: number;
  display: boolean;
  battery: boolean;
  features: string[];
  manifest: string;
  parts: FirmwarePart[];
}

export interface FirmwareIndex {
  schemaVersion: 2;
  version: string;
  commit: string;
  configSha256: string;
  project: { name: string; tagline: string; description: string };
  config: Record<string, unknown>;
  boards: Board[];
}

const HASH = /^[a-f0-9]{64}$/;

function fail(message: string): never {
  throw new Error(`Invalid firmware index: ${message}`);
}

/** Structural validation of index.json; throws with a short reason. */
export function validateIndex(value: unknown): FirmwareIndex {
  const v = value as Partial<FirmwareIndex> | null;
  if (!v || typeof v !== "object") fail("not an object");
  if (v.schemaVersion !== 2) fail("unsupported schemaVersion");
  for (const key of ["version", "commit"] as const)
    if (typeof v[key] !== "string" || !v[key]) fail(`missing ${key}`);
  if (!HASH.test(String(v.configSha256))) fail("bad configSha256");
  const p = v.project;
  if (!p || typeof p.name !== "string" || !p.name) fail("missing project.name");
  if (!Array.isArray(v.boards) || v.boards.length === 0) fail("no boards");
  for (const b of v.boards) {
    if (!b || typeof b.id !== "string" || typeof b.manifest !== "string")
      fail("bad board entry");
    if (!Array.isArray(b.parts) || b.parts.length === 0)
      fail(`board ${b.id} has no parts`);
    for (const part of b.parts) {
      if (
        typeof part.path !== "string" ||
        part.path.startsWith("/") ||
        part.path.split("/").includes("..") ||
        !Number.isSafeInteger(part.size) ||
        !Number.isSafeInteger(part.offset) ||
        !HASH.test(part.sha256)
      )
        fail(`bad part in board ${b.id}`);
    }
  }
  return v as FirmwareIndex;
}

/** Read `<dir>/index.json`; null when there is no packaged release. */
export function readIndex(
  dir: string = resolve(process.cwd(), "public/firmware"),
): FirmwareIndex | null {
  let text: string;
  try {
    text = readFileSync(resolve(dir, "index.json"), "utf8");
  } catch {
    return null;
  }
  return validateIndex(JSON.parse(text));
}

/** "ESP32-S3 · 2 buttons · display · battery" */
export function boardSummary(board: Board): string {
  const parts = [
    board.chip,
    `${board.buttons} ${board.buttons === 1 ? "button" : "buttons"}`,
  ];
  if (board.display) parts.push("display");
  if (board.battery) parts.push("battery");
  return parts.join(" · ");
}
