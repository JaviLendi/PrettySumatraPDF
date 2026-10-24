import { join, resolve } from "node:path";
import { detectVisualStudio, runLogged, findSolutionPath } from "./util";

const { msbuildPath } = detectVisualStudio();
const slnPath = findSolutionPath();
await runLogged(msbuildPath, [slnPath, `/t:test_util:Rebuild`, `/p:Configuration=Release;Platform=x64`, `/m`]);

const dir = join("out", "rel64");
await runLogged(resolve(join(dir, "test_util.exe")), [], dir);
