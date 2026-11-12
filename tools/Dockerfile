# The build image of zrnSDK: Arch Linux ARM (aarch64, what the phones run) with everything zrnui, zrnapp and the examples
# are built with. One file, two uses:
#
#   1. the sysroot of the cross build on the PC: `tools/zrn sysroot` builds this image and exports its file system;
#   2. a native aarch64 build environment (CI, or a PC without clang): the sources are mounted, nothing is installed.
#
#   docker build --platform linux/arm64 -t zrn-build -f tools/Dockerfile tools
#   docker run --rm --platform linux/arm64 -v "$PWD":/src zrn-build \
#       sh -c 'cmake -S zrnui -B /tmp/b -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/b && ctest --test-dir /tmp/b'
#
# On an x86_64 PC the container runs under qemu-user (binfmt for aarch64 must be registered); podman takes this file too.
FROM docker.io/menci/archlinuxarm:latest

# pacman's download sandbox needs Landlock, which the kernel behind qemu-user does not give
RUN pacman-key --init && pacman-key --populate archlinuxarm \
 && pacman --disable-sandbox -Syu --noconfirm --needed \
      base-devel cmake ninja pkgconf freetype2 libpng fontconfig mesa wayland libxkbcommon wayland-protocols \
 && pacman --disable-sandbox -Scc --noconfirm

WORKDIR /src
CMD ["bash"]
