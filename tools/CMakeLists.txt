add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/bc_diff)
add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/file_to_c)
add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/fshasher)
add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/x_decompress)
add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp)
# XenosRecomp is a host-side shader-cache generator. The Android game build
# uses the checked-in runtime sources and does not generate this cache.
if (NOT ANDROID)
    add_subdirectory(${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp)
endif()
