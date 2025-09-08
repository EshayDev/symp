# symp

[English](README.md) | [中文](README.zh-CN.md)

A patching tool based on symbols in Mach-O files, patches symbol functions with ease

**Supported:**

 - Symbol table `LC_SYMTAB`
 - Export table `LC_DYLD_INFO/LC_DYLD_EXPORTS_TRIE`
 - Import table `S_SYMBOL_STUBS`
 - `ObjC` metadata

## Installation

### Build from source

Clone this repo and build it with `cmake`

```sh
git clone https://github.com/Antibioticss/symp.git
cd symp
mkdir build && cd build
cmake .. && make
```

Install to `/usr/local/bin`

```sh
sudo make install
```

## Usage

```sh
symp [options] -- <symbol> <file>
```

### Quick examples

Write hex data to the file offset corresponding to the virtual address (as shown in the disassembler)

```sh
symp -x 909090 -- 0x10002a704 file
```

Make an OC function return 1

```sh
symp -p ret1 -- '-[MyClass isSmart]' file
```

Overwrite a function with a new binary (pure instructions)

```sh
symp -b new.bin -- '_old_func' file
```

### Symbol type

Currently supports three types of `symbol`

| Type           | Description                                                  | Example            |
| -------------- | ------------------------------------------------------------ | ------------------ |
| Hex offset     | the hex offset in memory, auto-detect symbols start with  `0x` or `0X` | `0x100007e68`      |
| `ObjC` symbol  | won't demangle class names, start with `+`/`-`, brakets with `[]` | `-[MyClass hello]` |
| regular symbol | symbols not meeting the above two conditions will be treated as this type | `_printf`          |

### Arguments

| Argument        | Description                                                  | Example            |
| --------------- | ------------------------------------------------------------ | ------------------ |
| `-p`/`--patch`  | use builtin patch, available value: `ret`, `ret0`, `ret1`, `ret2` | `-p ret1`          |
| `-b`/`--binary` | use a binary file as the patch                               | `-b data.bin`      |
| `-x`/`--hex`    | use hex data as the patch (case insensitive, can include spaces) | `-x "C0 03 5F D6"` |
| `-a`/`--arch`   | select an arch in `FAT`file, currently only supports `x86_64` and `arm64` | `-a arm64`         |

`-p/b/x` There should be only one of these, when none offered, it would print the offset of the symbol in the whole file

`-a` There can be several of this, when not offered, it would search symbol in all architectures of the given file

## LICENSE

[MIT LICENSE][LICENSE]