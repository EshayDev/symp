# symp

[English](README.md) | [中文](README.zh-CN.md)

A Mach-O symbol-based tool for easily patching functions by symbol.

**Supported:**

 - Symbol table `LC_SYMTAB`
 - Export table `LC_DYLD_INFO/LC_DYLD_EXPORTS_TRIE`
 - Import table `S_SYMBOL_STUBS`
 - `Obj-C` metadata

## Installation

### Pre-built binary

Grab the latest release from
[here.](https://github.com/Antibioticss/symp/releases/latest)

A `pkg` installer is also available; it installs to `/usr/local/bin` by default.

### Build from source

Clone the repo and build with `cmake`.

```sh
git clone https://github.com/Antibioticss/symp.git
cd symp
mkdir build && cd build
cmake .. && make
```

Install to `/usr/local/bin`:

```sh
sudo make install
```

## Usage

```sh
symp [options] -- <symbol> <file>
```

### Example usage

Write hex bytes to the file offset corresponding to a virtual address (as shown in your disassembler):

```sh
symp -x 909090 -- 0x10002a704 file
```

Make an Obj-C function return 1:

```sh
symp -p ret1 -- '-[MyClass isSmart]' file
```

Overwrite a function with a new binary (instructions only):

```sh
symp -b new.bin -- '_old_func' file
```

### Symbol types

Three symbol formats are supported:

| Type           | Description                                                  | Example            |
| -------------- | ------------------------------------------------------------ | ------------------ |
| Hex address     | virtual address in hex; auto-detected when it starts with `0x` or `0X` | `0x100007e68`      |
| `ObjC` symbol   | does not demangle class names; starts with `+`/`-`, enclosed in `[]` | `-[MyClass hello]` |
| Regular symbol  | anything that does not match the two cases above                      | `_printf`          |

### Arguments

| Argument        | Description                                                  | Example            |
| --------------- | ------------------------------------------------------------ | ------------------ |
| `-p`/`--patch`  | use a built-in patch; available values: `ret`, `ret0`, `ret1`, `ret2` | `-p ret1`          |
| `-b`/`--binary` | use a binary file as the patch                               | `-b data.bin`      |
| `-x`/`--hex`    | use hex data as the patch (case-insensitive; spaces allowed) | `-x "C0 03 5F D6"` |
| `-a`/`--arch`   | select an arch in a `FAT` file; currently supports `x86_64` and `arm64` | `-a arm64`         |

Only one of `-p`, `-b`, or `-x` may be specified. If none is provided, the tool prints the symbol's file offset.

`-a` can be passed multiple times. If omitted, the tool searches all architectures in the file.

## License

[MIT LICENSE](LICENSE)