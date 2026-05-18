# fast-wc

A high-performance `wc`-style line, word, byte, and character counter for Windows. The tool memory-maps input files, uses SIMD where available, and parallelizes work across files with a custom thread pool.

## Features

- Count **lines**, **words**, **bytes**, and **UTF-8 characters** (same columns as classic `wc`)
- **Memory-mapped I/O** via the Windows API for large files
- **SIMD** paths for line and character counting (AVX-512, AVX2, or SSE2, with scalar fallbacks)
- **Parallel processing** when multiple files are provided
- **stdin** support (`-` or no file arguments)
- Built with **C++20/23** and MSVC

## Usage

```text
fast-wc [options] [file ...]
```

If no options are given, all four counts are reported. With no files, input is read from **stdin**.

### Options

| Flag | Description |
|------|-------------|
| `-l`, `--lines` | Count lines |
| `-w`, `--words` | Count words |
| `-c`, `--chars` | Count characters (UTF-8) |
| `-m`, `--bytes` | Count bytes |
| `-h`, `--help` | Show help and exit |
| `-v`, `--version` | Show version and exit |

### Examples

```powershell
# All counts for one file
.\fast-wc.exe document.txt

# Lines and words only
.\fast-wc.exe -l -w document.txt

# Bytes only (uses a lighter I/O path)
.\fast-wc.exe -m document.txt

# Several files (processed in parallel)
.\fast-wc.exe -l -w chapter1.txt chapter2.txt chapter3.txt

# Read from stdin
Get-Content log.txt | .\fast-wc.exe -
```

Output is column-aligned; when multiple files are given, a **total** row is printed last.

> **Note:** Short flags here differ from GNU `wc`: `-c` is characters and `-m` is bytes.

## Building

### Requirements

- Windows 10 or later
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (or newer) with the **Desktop development with C++** workload
- **OpenSSL** development libraries (`libcrypto`, `libssl`)
- Optional: [vcpkg](https://vcpkg.io/) (see `vcpkg-configuration.json`)

### OpenSSL

The project links against OpenSSL (used by the thread-pool task worker). Install one of:

1. **Win64 OpenSSL** from [https://slproweb.com/products/Win32OpenSSL.html](https://slproweb.com/products/Win32OpenSSL.html) (default project paths expect `C:\Program Files\OpenSSL-Win64\`), or  
2. **vcpkg**: `vcpkg install openssl:x64-windows` and point include/lib directories in the project settings if needed.

### Build steps

1. Clone the repository:

   ```powershell
   git clone https://github.com/sergiorandria/fast-wc-win32.git
   cd fast-wc-win32
   ```

2. Open `fast-wc.sln` in Visual Studio.

3. Select a configuration (**Debug** or **Release**) and platform (**x64** recommended).

4. Build the **fast-wc** project (`Build` → `Build Solution`).

The executable is produced under `fast-wc\x64\Debug\` or `fast-wc\x64\Release\` (paths depend on configuration).

### Tests

The solution includes a **Sample-Test1** Google Test project. Build and run it from Visual Studio’s Test Explorer after building the solution.

## Project layout

```text
fast-wc-win32/
├── fast-wc.sln          # Visual Studio solution
├── fast-wc/             # Main application
│   ├── fast-wc.cpp      # Entry point
│   └── _FastWc*.h/cpp   # Core implementation (mapping, SIMD, thread pool)
├── Sample-Test1/        # Unit tests (Google Test)
├── vcpkg-configuration.json
└── LICENSE.txt
```

## How it works (overview)

1. **Parse** CLI flags and open inputs (files or stdin).
2. **Map** file contents into memory (`CreateFileMapping` / `MapViewOfFile`).
3. **Count** using SIMD kernels when the CPU supports them; otherwise use optimized scalar loops.
4. For **multiple files**, dispatch work to a fixed-size thread pool and merge per-thread totals.
5. **Print** aligned columns and optional totals.

When only byte count is requested (`-m` / `--bytes`), the reader can skip full mapping and use a bytes-only path.

## License

This project is licensed under the [MIT License](LICENSE.txt).
