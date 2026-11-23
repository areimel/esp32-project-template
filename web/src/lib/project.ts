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

/** Packaged firmware directory; builds run from web/, so this is web/public/firmware. */
export const FIRMWARE_DIR = resolve(process.cwd(), "public/firmware");

const HASH = /^[a-f0-9]{64}$/;
const COMMIT = /^[a-f0-9]{40}$/;

function fail(message: string): never {
  throw new Error(`Invalid firmware index: ${message}`);
}

const isStr = (v: unknown): v is string => typeof v === "string" && v.length > 0;

/** Relative, forward-slash path with no traversal, backslash or drive letter. */
function safeRelPath(v: unknown): v is string {
  return (
    isStr(v) &&
    !v.startsWith("/") &&
    !v.includes("\\") &&
    !/^[A-Za-z]:/.test(v) &&
    !v.split("/").includes("..")
  );
}

/** Structural validation of index.json; throws with a short reason. */
export function validateIndex(value: unknown): FirmwareIndex {
  const v = value as Partial<FirmwareIndex> | null;
  if (!v || typeof v !== "object") fail("not an object");
  if (v.schemaVersion !== 2) fail("unsupported schemaVersion");
  if (!isStr(v.version)) fail("missing version");
  if (!COMMIT.test(String(v.commit))) fail("commit must be 40 lowercase hex");
  if (!HASH.test(String(v.configSha256))) fail("bad configSha256");
  const p = v.project;
  if (!p || !isStr(p.name)) fail("missing project.name");
  if (typeof p.tagline !== "string") fail("project.tagline must be a string");
  if (typeof p.description !== "string") fail("project.description must be a string");
  if (!Array.isArray(v.boards) || v.boards.length === 0) fail("no boards");
  for (const b of v.boards) {
    if (!b || !isStr(b.id) || !isStr(b.label) || !isStr(b.chip))
      fail("board needs string id, label and chip");
    if (!Number.isInteger(b.buttons) || b.buttons < 0)
      fail(`board ${b.id} buttons must be an integer >= 0`);
    if (typeof b.display !== "boolean" || typeof b.battery !== "boolean")
      fail(`board ${b.id} display and battery must be booleans`);
    if (!Array.isArray(b.features) || !b.features.every((f) => typeof f === "string"))
      fail(`board ${b.id} features must be a string array`);
    if (!safeRelPath(b.manifest)) fail(`board ${b.id} has an unsafe manifest path`);
    if (!Array.isArray(b.parts) || b.parts.length === 0)
      fail(`board ${b.id} has no parts`);
    for (const part of b.parts) {
      if (
        !safeRelPath(part?.path) ||
        !Number.isSafeInteger(part.size) ||
        part.size < 0 ||
        !Number.isSafeInteger(part.offset) ||
        part.offset < 0 ||
        !HASH.test(part.sha256)
      )
        fail(`bad part in board ${b.id}`);
    }
  }
  return v as FirmwareIndex;
}

/** Join a site-relative path onto the Astro base, whatever slashes the base has. */
export function withBase(
  path: string,
  base: string = import.meta.env?.BASE_URL ?? "/",
): string {
  return `${base.replace(/\/+$/, "")}/${path.replace(/^\/+/, "")}`;
}

/** Read `<dir>/index.json`; null when there is no packaged release. */
export function readIndex(dir: string = FIRMWARE_DIR): FirmwareIndex | null {
  let text: string;
  try {
    text = readFileSync(resolve(dir, "index.json"), "utf8");
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return null;
    throw error;
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
