file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(zppbits)
target_sources(zppbits PUBLIC FILE_SET CXX_MODULES FILES ${srcs})
target_include_directories(zppbits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(zppbits PROPERTIES EXPORT_NAME "cxx_module")
install(
    TARGETS zppbits
    EXPORT zppbits-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zppbits)

install(
    PACKAGE_INFO zppbits
    EXPORT zppbits-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cps/zppbits
    CXX_MODULES_DIRECTORY cxxmodules)
