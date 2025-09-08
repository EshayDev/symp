# symp

[English](README.md) | [中文](README.zh-CN.md)

基于Mach-O内符号的补丁工具，轻松地修改符号函数

**已支持：**

 - 符号表 `LC_SYMTAB`
 - 导出表 `LC_DYLD_INFO/LC_DYLD_EXPORTS_TRIE`
 - 导入表 `S_SYMBOL_STUBS`
 - `ObjC`元数据

## 安装

### 从源码编译

克隆仓库并使用`cmake`编译

```sh
git clone https://github.com/Antibioticss/symp.git
cd symp
mkdir build && cd build
cmake .. && make
```

安装到`/usr/local/bin`

```sh
sudo make install
```

## 使用

```sh
symp [options] -- <symbol> <file>
```

### 快速上手

往虚拟地址（反汇编器中看到的）对应的文件偏移写入十六进制数据

```sh
symp -x 909090 -- 0x10002a704 file
```

让一个OC函数返回1

```sh
symp -p ret1 -- '-[MyClass isSmart]' file
```

用新的二进制（纯指令）覆盖一个原有的函数

```sh
symp -b new.bin -- '_old_func' file
```

### 符号类型

目前支持支持三种类型的`symbol`

| 类型         | 说明                                               | 示例               |
| ------------ | -------------------------------------------------- | ------------------ |
| 十六进制偏移 | 为在内存中的偏移量，以`0x`或者`0X`开头会被自动识别 | `0x100007e68`      |
| `ObjC`符号名 | 不会demangle类名，以`+`/`-`开头，用`[]`框起来      | `-[MyClass hello]` |
| 一般的符号名 | 不满足上面两条的符号都会当作此类型                 | `_printf`          |

### 参数

| 参数 | 说明 | 示例 |
| ------------ | ---- | ---- |
| `-p`/`--patch` | 使用内置的补丁，可选`ret`, `ret0`, `ret1`, `ret2` | `-p ret1` |
| `-b`/`--binary` | 使用一个二进制文件作为补丁 | `-b data.bin` |
| `-x`/`--hex` | 使用十六进制数据作为补丁（不要求大小写，可以有空格） | `-x "C0 03 5F D6"` |
|`-a`/`--arch`|指定`FAT`文件中的某个架构，目前仅支持`x86_64`和`arm64`|`-a arm64`|

`-p/b/x`这三个参数只能有其中一个，当都没有提供时，会输出该符号在整个文件中的偏移量

`-a`可以有多个，当未提供`-a`参数时，默认会查找文件中的所有架构

## LICENSE

[MIT LICENSE][LICENSE]

