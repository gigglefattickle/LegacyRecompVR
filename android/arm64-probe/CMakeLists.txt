cmake_minimum_required(VERSION 3.22)
project(LegacyRecompAndroidArm64Probe LANGUAGES CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "Configure this probe with the Android NDK toolchain.")
endif()

if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Expected ANDROID_ABI=arm64-v8a, got '${ANDROID_ABI}'.")
endif()

# Exercise the same architecture policy used by the full project.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/TargetArchitecture.cmake")

add_library(legacyrecomp_android_arm64_probe SHARED probe.cpp)
target_compile_features(legacyrecomp_android_arm64_probe PRIVATE cxx_std_17)
target_compile_definitions(legacyrecomp_android_arm64_probe PRIVATE VK_USE_PLATFORM_ANDROID_KHR)

find_library(ANDROID_LOG_LIBRARY log REQUIRED)
find_library(ANDROID_LIBRARY android REQUIRED)
find_library(VULKAN_LIBRARY vulkan REQUIRED)

target_link_libraries(legacyrecomp_android_arm64_probe
    PRIVATE
        ${ANDROID_LOG_LIBRARY}
        ${ANDROID_LIBRARY}
        ${VULKAN_LIBRARY}
)
