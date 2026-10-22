import { join, resolve } from "node:path";
import { detectVisualStudio, runLogged, findSolutionPath } from "./util";
import { clearDirPreserveSettings } from "./clean";

function removeReleaseBuilds(): void {
  // join("out", "arm64"), join("out", "rel32"),
  const dirs = [join("out", "rel64")];
  for (const dir of dirs) {
    clearDirPreserveSettings(dir);
  }
}

async function main() {
  const timeStart = performance.now();

  console.log("smoke build");
  removeReleaseBuilds();

  const { msbuildPath } = detectVisualStudio();
  const sln = findSolutionPath();
  const t = `/t:SumatraPDF-dll:Rebuild;test_util:Rebuild`;
  const p = `/p:Configuration=Release;Platform=x64`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  const outDir = join("out", "rel64");

  await runLogged(resolve(join(outDir, "test_util.exe")), [], outDir);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`smoke build took ${elapsed}s`);
}

await main();
