# Cross toolchain for the phones (Arch Linux ARM, aarch64): clang from the PC, headers and libraries from a sysroot.
# The sysroot is made by `tools/zrn sysroot`; ZRN_SYSROOT (environment or cache variable) points at it.

if(NOT ZRN_SYSROOT)
	set(ZRN_SYSROOT "$ENV{ZRN_SYSROOT}")
endif()
if(NOT ZRN_SYSROOT)
	set(ZRN_SYSROOT "$ENV{HOME}/.cache/zrn/sysroot")
endif()
if(NOT EXISTS "${ZRN_SYSROOT}/usr/include")
	message(FATAL_ERROR "no aarch64 sysroot at ${ZRN_SYSROOT}: run tools/zrn sysroot")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET aarch64-unknown-linux-gnu)
set(CMAKE_SYSROOT "${ZRN_SYSROOT}")
# the linker of the PC is for x86_64; lld links any target
foreach(kind EXE SHARED MODULE)
	set(CMAKE_${kind}_LINKER_FLAGS_INIT "-fuse-ld=lld")
endforeach()

# programs (cmake helpers, wayland-scanner) come from the PC, libraries and headers from the sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config looks only into the sysroot and puts its path in front of -I and -L
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${ZRN_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR} "${ZRN_SYSROOT}/usr/lib/pkgconfig:${ZRN_SYSROOT}/usr/share/pkgconfig")
