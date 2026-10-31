cmake_minimum_required(VERSION 3.10)
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()
project(xz_embedded LANGUAGES C)

# Preserve the upstream build; this target uses only caller-owned storage.
add_library(radlab_xz_embedded STATIC
    linux/lib/xz/xz_dec_stream.c
    linux/lib/xz/xz_dec_lzma2.c
    linux/lib/xz/xz_crc32.c
    linux/lib/xz/xz_crc64.c
    linux/lib/xz/xz_sha256.c)
add_library(xz::embedded ALIAS radlab_xz_embedded)
set_target_properties(radlab_xz_embedded PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED YES POSITION_INDEPENDENT_CODE YES)
target_include_directories(radlab_xz_embedded PUBLIC linux/include/linux
    PRIVATE userspace)
target_compile_definitions(radlab_xz_embedded PUBLIC XZ_DEC_FIXED_STORAGE XZ_USE_CRC64
    PRIVATE XZ_DEC_CONCATENATED XZ_USE_SHA256)

option(XZ_EMBEDDED_TESTS "Build caller-owned storage regression tests" "${XZ_EMBEDDED_TESTS}")
if(XZ_EMBEDDED_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(xz_fixed_storage_test userspace/fixed_storage_test.c)
    target_link_libraries(xz_fixed_storage_test PRIVATE xz::embedded)
    set_target_properties(xz_fixed_storage_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME xz_fixed_storage
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/userspace/test_fixed_storage.py
            --driver $<TARGET_FILE:xz_fixed_storage_test>
            --library $<TARGET_FILE:radlab_xz_embedded> --nm ${CMAKE_NM})
endif()
