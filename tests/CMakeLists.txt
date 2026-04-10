find_package(GTest REQUIRED)

add_library(test_helper STATIC)
target_sources(
    test_helper
    PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/helper.cpp
)
target_link_libraries(test_helper GTest::GTest zppbits)

file(
    GLOB test_srcs
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

include(GoogleTest)
foreach(test_src ${test_srcs})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} GTest::GTest GTest::Main test_helper zppbits)
    gtest_discover_tests(${test_name})
endforeach()
