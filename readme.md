# zpp-bits

[User Manual](https://github.com/mccakit/zppbits/blob/main/docs/user_manual.md) | [Api Reference](https://mccakit.github.io/zppbits/)

zpp-bits is a C++ Module port of [zpp-bits](https://github.com/eyalz800/zpp_bits), A lightweight C++20 serialization and RPC library

Project is built using CMake/Ninja and packaged via CPS. CMake 4.4 and later is required.

Build using cmake, and consume via CPS by pointing to `CMAKE_INSTALL_PREFIX` via `CMAKE_PREFIX_PATH`

```cmake
find_package(zpp-bits)
target_link_libraries($PROJECT PRIVATE zpp-bits::cxx_module)
```
