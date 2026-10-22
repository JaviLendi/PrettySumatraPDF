"""Generate optimized WebUI fonts. Requires: python -m pip install fonttools brotli."""

from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "prettysumatra" / "webui"
SOURCE = WEBUI / "vendor" / "material-icons" / "fonts" / (
    "syl0-zNym6YjUruM-QrEh7-nyTnjDwKNJ_190FjpZIvDmUSVOK7BDB_Qb9vUSzq3wzLK-"
    "P0J-V_Zs-QtQth3-jOcbTCVpeRL2w5rwZu2rIelXxI.ttf"
)
OUTPUT = WEBUI / "vendor" / "material-icons" / "fonts" / "material-symbols-rounded-subset.woff2"
INTER_FONTS = (
    "UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuLyfMZg.ttf",
    "UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuI6fMZg.ttf",
    "UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuGKYMZg.ttf",
    "UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuFuYMZg.ttf",
    "UcCO3FwrK3iLTeHuS_nVMrMxCp50SjIw2boKoduKmMEVuDyYMZg.ttf",
)
ICON_PATTERN = re.compile(
    r"class=[\"'][^\"']*\bmaterial-symbols-rounded\b[^\"']*[\"'][^>]*>\s*([a-z0-9_]+)\s*</span>"
)


def main():
    icons = set()
    for page in (WEBUI / "toolbar.html", WEBUI / "home.html"):
        icons.update(ICON_PATTERN.findall(page.read_text(encoding="utf-8")))
    if not icons:
        raise SystemExit("No Material Symbols ligatures found in the WebUI pages")

    command = [
        sys.executable,
        "-m",
        "fontTools.subset",
        str(SOURCE),
        "--text=" + " ".join(sorted(icons)),
        "--layout-features=*",
        "--flavor=woff2",
        "--output-file=" + str(OUTPUT),
    ]
    subprocess.run(command, check=True)
    print(f"Generated {OUTPUT.name} with {len(icons)} ligatures")

    inter_dir = WEBUI / "vendor" / "inter" / "fonts"
    for source_name in INTER_FONTS:
        source = inter_dir / source_name
        output = source.with_suffix(".woff2")
        command = [
            sys.executable,
            "-m",
            "fontTools.subset",
            str(source),
            "--unicodes=*",
            "--layout-features=*",
            "--flavor=woff2",
            "--output-file=" + str(output),
        ]
        subprocess.run(command, check=True)
        print(f"Generated {output.name}")


if __name__ == "__main__":
    main()