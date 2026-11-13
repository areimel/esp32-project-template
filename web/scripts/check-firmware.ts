import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { resolve } from "node:path";
import { readIndex } from "../src/lib/project";

// Optional argv[2] overrides the packaged firmware directory (used by tests).
const dir = process.argv[2]
  ? resolve(process.argv[2])
  : resolve(import.meta.dirname, "../public/firmware");

try {
  const index = readIndex(dir);
  if (!index) {
    console.log("No packaged firmware; building without a release");
  } else {
    let count = 0;
    for (const board of index.boards)
      for (const part of board.parts) {
        const bytes = await readFile(resolve(dir, part.path));
        const hash = createHash("sha256").update(bytes).digest("hex");
        if (bytes.length !== part.size || hash !== part.sha256)
          throw new Error(`Size or checksum mismatch: ${part.path}`);
        count++;
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
