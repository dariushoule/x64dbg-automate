# PluginTemplate

Template CMake project for x64dbg plugins. This uses [cmkr](https://cmkr.build), `cmake.toml` contains the project configuration.

## Using the template

You can click the green *Use this template* button. See the article [*Creating a repository from a template*
](https://docs.github.com/en/free-pro-team@latest/github/creating-cloning-and-archiving-repositories/creating-a-repository-from-a-template) by GitHub for more details.

Alternatively you can download a ZIP of this repository and set up the template locally.

## Getting started

- Pretty much all of the available functionality can be found in [`bridgemain.h`](https://github.com/x64dbg/x64dbg/blob/97ff1ec98a5bbf543e6d80ebbbd2401edf6c8cca/src/bridge/bridgemain.h).
- You can find some (commented) example code in [`src/plugin.cpp`](./src/plugin.cpp).
- Example plugins: https://plugins.x64dbg.com.
- References:
  - https://help.x64dbg.com/en/latest/developers/plugins/index.html
  - https://x64dbg.com/blog/2016/10/04/architecture-of-x64dbg.html
  - https://x64dbg.com/blog/2016/10/20/threading-model.html
  - https://x64dbg.com/blog/2016/07/30/x64dbg-plugin-sdk.html

## Building

From a Visual Studio command prompt:

```
cmake -B build64 -A x64
cmake --build build64 --config Release
```

You will get `build64\PluginTemplate.sln` that you can open in Visual Studio.

To build a 32-bit plugin:

```
cmake -B build32 -A Win32
cmake --build build32 --config Release
```

Alternatively you can open this folder in Visual Studio/CLion/Qt Creator.

![building animation](https://github.com/x64dbg/PluginTemplate/blob/3951eb4b320b7a26164616ab5141414e8cd5b0a1/building.gif?raw=true)

## Creating releases

This template has GitHub Actions set up in [`.github/workflows/build.yml`](./.github/workflows/build.yml). If you push a tag prefixed with `v` (for instance `v1.0`) it will automatically publish a GitHub release with the plugin binaries compiled for both architectures.

## Automatic reloading

You can set up the [PluginDevHelper](https://github.com/x64dbg/PluginDevHelper) utility to automatically unload and reload the plugin from x64dbg when compiling. See the README there for more detailled instructions.
