# PrettySumatraPDF — v1.0.0 (First Release)

Release date: 2026-05-09

## Overview

PrettySumatraPDF is the first official release of the PrettySumatra redesign. It preserves the full functionality of the original SumatraPDF reader while providing a modernized native UI and optional hybrid WebView2 features.

## Highlights

- Modern native Win32 toolbar and visual refresh
- All original formats supported (PDF, EPUB, MOBI, CBZ, CBR, FB2, CHM, XPS, DjVu)
- Preserved command behavior, shortcuts, and features
- Improved toolbar density and Windows 11–inspired visuals
- Hybrid WebView2 integration for advanced UI components

## Screenshots

- Toolbar: `docs/md/img/toolbar.png`
- Command palette: `docs/md/img/command-palette-tabs.png`
- Annotation editor: `docs/md/img/annotation-editor.png`
- Settings: `docs/md/img/settings-app.png`

## Notes

This is a first release derived from the `prettysumatra-base` work and integrates native toolbar modernization phases. See `PRETTYSUMATRA_NOTES.md` for development details and roadmap.

---

To publish this release to the remote repository, run:

```bash
# push branch
git push origin release/1.0.0
# push tag
git tag -a v1.0.0 -m "PrettySumatraPDF v1.0.0" && git push origin v1.0.0
```
