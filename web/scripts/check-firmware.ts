import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { dirname, posix, resolve, sep } from "node:path";
import type { Board } from "../src/lib/project";
import { FIRMWARE_DIR, readIndex } from "../src/lib/project";

// Optional argv[2] overrides the packaged firmware directory (used by tests).
const dir = process.argv[2]
  ? resolve(process.argv[2])
  : FIRMWARE_DIR;

/** The manifest esp-web-install-button loads must describe exactly the verified parts. */
async function checkManifest(board: Board): Promise<void> {
  const where = `${board.id} manifest`;
  let manifest: any;
  try {
    manifest = JSON.parse(await readFile(resolve(dir, board.manifest), "utf8"));
  } catch (error) {
    throw new Error(`Cannot read ${where} ${board.manifest}: ${(error as Error).message}`);
  }
  const build = manifest?.builds?.[0];
  if (build?.chipFamily !== board.chip)
    throw new Error(`${where} chipFamily ${build?.chipFamily} is not the board chip ${board.chip}`);
  if (!Array.isArray(build.parts) || build.parts.length === 0)
    throw new Error(`${where} has no parts`);
  const base = dirname(board.manifest);
  for (const part of build.parts) {
    const indexed = board.parts.find((p) => p.path === posix.join(base, String(part?.path)));
    if (!indexed) throw new Error(`${where} part ${part?.path} is not in the index`);
    if (part.offset !== indexed.offset)
      throw new Error(`${where} offset for ${part.path} is ${part.offset}, index says ${indexed.offset}`);
  }
}

try {
  const index = readIndex(dir);
  if (!index) {
    console.log("No packaged firmware; building without a release");
  } else {
    let count = 0;
    for (const board of index.boards) {
      await checkManifest(board);
      for (const part of board.parts) {
        const file = resolve(dir, part.path);
        if (!file.startsWith(dir + sep))
          throw new Error(`Part path escapes the firmware directory: ${part.path}`);
        const bytes = await readFile(file);
        const hash = createHash("sha256").update(bytes).digest("hex");
        if (bytes.length !== part.size || hash !== part.sha256)
          throw new Error(`Size or checksum mismatch: ${part.path}`);
        count++;
      }
    }
    console.log(
      `Firmware package verified: ${index.version} (${index.boards.length} boards, ${count} parts)`,
    );
  }
} catch (error) {
  console.error("The packaged firmware is invalid; refusing to build the site.");
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
}
