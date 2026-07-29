file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(zpp-bits)
target_sources(zpp-bits PUBLIC FILE_SET CXX_MODULES FILES ${srcs})
target_include_directories(zpp-bits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(zpp-bits PROPERTIES EXPORT_NAME "cxx_module")
install(
    TARGETS zpp-bits
    EXPORT zpp-bits-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zpp-bits)

install(
    PACKAGE_INFO zpp-bits
    EXPORT zpp-bits-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cps/zpp-bits
    CXX_MODULES_DIRECTORY cxxmodules)
