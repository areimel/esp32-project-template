import { test } from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { boardSummary, readIndex } from "../src/lib/project";

const fixtures = resolve(import.meta.dirname, "fixtures");

test("readIndex returns null for a missing directory", () => {
  assert.equal(readIndex(join(tmpdir(), "no-such-firmware-dir")), null);
});

test("readIndex returns boards in file order", () => {
  const index = readIndex(fixtures);
  assert.ok(index);
  assert.deepEqual(
    index.boards.map((b) => b.id),
    ["sticks3", "esp32"],
  );
  assert.equal(index.project.name, "Fixture Starter");
});

test("boardSummary describes board capabilities", () => {
  const index = readIndex(fixtures)!;
  assert.equal(
    boardSummary(index.boards[0]),
    "ESP32-S3 · 2 buttons · display · battery",
  );
  assert.equal(boardSummary(index.boards[1]), "ESP32 · 1 button");
});

const script = resolve(import.meta.dirname, "../scripts/check-firmware.ts");
const tsx = resolve(import.meta.dirname, "../node_modules/.bin/tsx");
const run = (dir: string) => spawnSync(tsx, [script, dir], { encoding: "utf8" });

/** Copy the fixture index and write matching part files into a temp dir. */
function packaged(corrupt = false): string {
  const dir = mkdtempSync(join(tmpdir(), "fw-"));
  const index = JSON.parse(readFileSync(join(fixtures, "index.json"), "utf8"));
  const bytes = Buffer.from([1, 2, 3, 4]);
  for (const board of index.boards) {
    mkdirSync(join(dir, board.id), { recursive: true });
    for (const part of board.parts) {
      part.size = bytes.length;
      part.sha256 = createHash("sha256").update(bytes).digest("hex");
      writeFileSync(join(dir, part.path), corrupt ? Buffer.from([9, 9, 9, 9]) : bytes);
    }
  }
  writeFileSync(join(dir, "index.json"), JSON.stringify(index));
  return dir;
}

test("check-firmware builds without a release when no index exists", () => {
  const r = run(mkdtempSync(join(tmpdir(), "fw-empty-")));
  assert.equal(r.status, 0);
  assert.match(r.stdout, /No packaged firmware; building without a release/);
});

test("check-firmware accepts matching hashes", () => {
  const r = run(packaged());
  assert.equal(r.status, 0, r.stderr);
});

test("check-firmware fails on a hash mismatch", () => {
  assert.equal(run(packaged(true)).status, 1);
});
