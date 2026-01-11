# pdf2jpeg

`pdf2jpeg` is a small Windows (x64) command-line tool that converts all `*.pdf` files in the same directory as the executable into JPEG images (one JPEG per page) using the MuPDF rendering library.

## What it does

- Scans the executable directory for `*.pdf`
- Renders every page at a fixed DPI and saves as JPEG
- Output naming:
  - `input.pdf` → `input_p0001.jpg`, `input_p0002.jpg`, ...
- Handles Korean (Unicode) PDF filenames and prints them correctly in Windows 10/11 terminals (UTF-8 console output)

## Usage

1. Put `pdf2jpeg.exe` in a folder.
2. Put one or more `*.pdf` files in the same folder.
3. Run:
   - `pdf2jpeg.exe`

JPEGs will be created next to the PDFs.

## Build (static CRT: `/MT`, Windows x64)

### Prerequisites

- Windows 10/11 x64
- Visual Studio 2022 (MSVC) + Windows 10/11 SDK

This repository vendors MuPDF under `mupdf/` (see `mupdf/COPYING` for licensing).

### 1) Build MuPDF static libraries (Release|x64)

Build with MSBuild and keep the build serialized to avoid PDB write collisions:

- `"%ProgramFiles%\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe" mupdf\\platform\\win32\\libmupdf.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1`

Notes:
- The Win32 project files under `mupdf/platform/win32/` are configured for `/MT` in `Release|x64`.
- `mupdf/platform/win32/libtesseract.vcxproj` disables multi-processor compilation in `Release|x64` to avoid `fatal error C1041` (shared PDB contention).

### 2) Build `pdf2jpeg.exe`

From an x64 Developer Command Prompt (or by calling `vcvars64.bat`), build and link against the MuPDF libs:

- `"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"`
- `cl /nologo /W4 /EHsc /MT /DUNICODE /D_UNICODE /I mupdf\\include /Fe:pdf2jpeg.exe pdf2jpeg.c /link /LTCG /LIBPATH:mupdf\\platform\\win32\\x64\\Release libmupdf.lib libthirdparty.lib libmuthreads.lib`

If `pdf2jpeg.obj` is locked by another process, override the object output path:

- `cl ... /Fo:pdf2jpeg_mt.obj ...`

## Configuration

In `pdf2jpeg.c`:

- `kDpi` controls rendering DPI
- `kJpegQuality` controls JPEG quality (1–100)

