# zpp.bits

zpp.bits is a C++ Module port of zpp.bits, A lightweight C++20 serialization and RPC library

Project is built using CMake/Ninja and packaged via CPS. CMake 4.4 and later is required.

Build using cmake, and consume via CPS by pointing to `CMAKE_INSTALL_PREFIX` via `CMAKE_PREFIX_PATH`

```cmake
find_package(zppbits)
target_link_libraries($PROJECT PRIVATE zppbits::cxx_module)
```
