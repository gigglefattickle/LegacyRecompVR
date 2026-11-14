# Apply host-specific baseline flags only to matching target architectures.
# Android ARM64 must use the NDK's ARMv8 baseline instead of an x86 CPU target.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
    add_compile_options(-march=sandybridge)
endif()
