# 📄 PrettySumatraPDF

[![GitHub release](https://img.shields.io/github/v/release/JaviLendi/PrettySumatraPDF?include_prereleases&style=flat-square)](https://github.com/JaviLendi/PrettySumatraPDF/releases)
[![License](https://img.shields.io/github/license/JaviLendi/PrettySumatraPDF?style=flat-square)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/JaviLendi/PrettySumatraPDF?style=flat-square)](https://github.com/JaviLendi/PrettySumatraPDF/stargazers)
[![Build](https://img.shields.io/github/actions/workflow/status/JaviLendi/PrettySumatraPDF/build.yml?style=flat-square)](https://github.com/JaviLendi/PrettySumatraPDF/actions)

<div align="center">
  <img src="gfx/SumatraPDF-128x128x32.png" alt="PrettySumatraPDF Logo" width="128" height="128">
</div>

**PrettySumatraPDF** is a modern redesign of the classic SumatraPDF reader, bringing a fresh Windows 11-style interface while maintaining 100% of the original functionality and features.

**Fork notice:** PrettySumatraPDF is a fork of [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf). This repository is maintained by Javi Lendi and preserves the original project's authors and licensing; see the `AUTHORS` and `LICENSE` files for details.

---

## ✨ Key Features

- 🚀 **Performance Focused**: Optimized rendering core maintains the lightweight, fast performance SumatraPDF is known for.
- 🎨 **Modern Native UI**: Windows 11-inspired design philosophy applied to the native Win32 toolbar and interface.
- 🌙 **Dark Mode**: A true dark mode, pleasant to the eye and well-integrated with the application.
- 🌐 **Zero Functionality Loss**: All original features and capabilities are fully preserved.
- 📦 **Multi-Format Support**: PDF, EPUB, MOBI, CBZ, CBR, FB2, CHM, XPS, DjVu.
- 📝 **Annotations**: Annotations with saved/unsaved state tracking.
- 🎯 **Advanced Navigation**: Command palette, tabs, bookmarks, table of contents.
- 🔧 **Customization**: Keyboard shortcuts, theme support, settings management.


---

## 📸 Screenshots

### Home Screen

#### Light Theme
![Home Screen - Light Theme](docs/md/img/Home-light.png)

#### Dark Theme
![Home Screen - Dark Theme](docs/md/img/Home-dark.png)

### Toolbar

#### Light Theme
![Toolbar - Light Theme](docs/md/img/Light-pdf.png)

#### Dark Theme
![Toolbar - Dark Theme](docs/md/img/Dark-pdf.png)

---

## 🚀 Download & Installation

Ready to try it out? Head to the releases section to download the latest compiled version:

👉 [**Download PrettySumatraPDF (Latest Release)**](https://github.com/JaviLendi/PrettySumatraPDF/releases)

**Note:** You can use it as a portable application or replace your current SumatraPDF installation.

---

## 🛠️ Build & Development

This project is built on top of the official SumatraPDF codebase. If you want to compile it yourself or contribute to visual development, follow these steps:

### Requirements

- **Visual Studio** with C++ support
- **Bun** (for build tools) - [Download Bun](https://bun.sh)
- **Clang-Format** (for code formatting)

### Basic Build

1. Clone the repository:
   ```bash
   git clone https://github.com/JaviLendi/PrettySumatraPDF.git
   cd PrettySumatraPDF
   ```

3. Run the build:
   ```bash
   bun ./cmd/build.ts
   ```

4. The executable will be located at: `./out/dbg64/PrettySumatraPDF.exe`

### Debugging

To run with the debugger:
```bash
windbgx -Q -o -g ./out/dbg64/PrettySumatraPDF.exe
```

### Code Formatting

After making changes to `.cpp`, `.c` or `.h` files, run:
```bash
bun ./cmd/format.ts
```

---

## 🤝 Maintainability & Contributions

PrettySumatraPDF is an open-source project licensed under the same terms as SumatraPDF (A)GPLv3, with some code under BSD license. Contributions are welcome! For more information on how to contribute, please visit the [Developer Information](https://www.sumatrapdfreader.org/docs/Contribute-to-SumatraPDF) page.

## Fork notice and attribution

- This project is a fork of the original [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf) project.
- All original authors are credited in the `AUTHORS` file. The maintainer of this fork is **Javi Lendi**.
- Licensing from the upstream project is preserved; do not change `COPYING`/`COPYING.BSD` without legal review.

My goal with PrettySumatraPDF is to create a visually appealing and modernized version of SumatraPDF while ensuring that all existing features and functionalities remain intact. I am committed to maintaining the core principles of SumatraPDF, including its lightweight nature and fast performance, while enhancing the user experience with a fresh design.

I am committed to updating PrettySumatraPDF with the latest features and improvements from the original SumatraPDF project, ensuring that users can enjoy the best of both worlds: a modern interface and the full functionality of the classic reader.

### More Information

- 🌐 [PrettySumatraPDF GitHub](https://github.com/JaviLendi/PrettySumatraPDF)
- 📚 [Manual (docs)](https://github.com/JaviLendi/PrettySumatraPDF/tree/main/docs/md)
- 👨‍💻 [Developer Information (docs)](https://github.com/JaviLendi/PrettySumatraPDF/tree/main/docs/md)
- 📄 [License](LICENSE)
- ✍️ [Authors](AUTHORS)
