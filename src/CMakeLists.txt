file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(zppbits STATIC)
target_sources(zppbits PUBLIC FILE_SET CXX_MODULES FILES ${srcs})
target_include_directories(zppbits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
include(GNUInstallDirs)
install(
    TARGETS zppbits
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    CXX_MODULES_BMI DESTINATION ${CMAKE_INSTALL_LIBDIR}/cxx/bmi
)
configure_file(${CMAKE_SOURCE_DIR}/packaging/zppbits.pc.in zppbits.pc @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/zppbits.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
