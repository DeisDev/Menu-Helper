# Building

Use a Windows shell (not Git Bash) with xmake and the Visual Studio C++ tools.

```
git submodule update --init --recursive
xmake f -p windows -a x64 --toolchain=msvc -m releasedbg
xmake build
xmake package MenuHelper
```

The archive is written to `build/packages`. Set `XSE_FO4_GAME_PATH` to the game folder to install after each build.

## SWF

`swf/MenuHelper.swf` is built from `swf/src` and checked in. To rebuild it you need Java, the [Apache Flex SDK](https://flex.apache.org/download-binaries.html) and a `playerglobal.swc` (FFDec has one in its `flashlib` folder).

```
xmake f --flex_sdk=<Flex SDK folder> --playerglobal=<path to playerglobal.swc>
xmake build MenuHelperSWF
```

## Papyrus

`papyrus/MenuHelper.pex` is checked in. To rebuild it you need the Creation Kit compiler, the F4SE script sources and the Creation Kit `Base.zip` extracted.

```
xmake f --papyrus_compiler=<path to PapyrusCompiler.exe> --papyrus_imports=<F4SE script sources>;<extracted Base.zip>
xmake build MenuHelperPapyrus
```
