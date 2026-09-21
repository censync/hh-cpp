#!/bin/sh
# Consumer smoke tests: installs hh into a scratch prefix and builds a program
# against it with find_package, with pkg-config, and with add_subdirectory.
# usage: tests/consumer/run.sh [build directory]
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
work=${1:-$root/build/consumer}
rm -rf "$work"
mkdir -p "$work"

for shared in OFF ON; do
    prefix=$work/prefix-$shared
    cmake -S "$root" -B "$work/hh-$shared" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=$shared \
        -DHH_BUILD_TESTS=OFF -DHH_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$prefix" > /dev/null
    cmake --build "$work/hh-$shared" --parallel > /dev/null
    cmake --install "$work/hh-$shared" > /dev/null

    cmake -S "$here/find_package" -B "$work/find_package-$shared" -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_BUILD_TYPE=Release > /dev/null
    cmake --build "$work/find_package-$shared" > /dev/null
    libdir=$(dirname "$(find "$prefix" -name 'libhh.*' | head -n 1)")
    LD_LIBRARY_PATH="$libdir" DYLD_LIBRARY_PATH="$libdir" \
        ctest --test-dir "$work/find_package-$shared" --output-on-failure > /dev/null
    echo "find_package, BUILD_SHARED_LIBS=$shared: ok"
done

if command -v pkg-config > /dev/null 2>&1; then
    # The installed tree is moved first: hh.pc finds its prefix from its own location.
    mv "$work/prefix-OFF" "$work/moved"
    pcdir=$(dirname "$(find "$work/moved" -name hh.pc)")
    flags=$(PKG_CONFIG_PATH="$pcdir" pkg-config --cflags --libs hh)
    # shellcheck disable=SC2086
    ${CXX:-c++} -std=c++17 "$here/consumer.cpp" $flags -o "$work/pkg_config_consumer"
    "$work/pkg_config_consumer" > /dev/null
    # A C program links the static library through the C ABI; it needs Libs.private.
    static_flags=$(PKG_CONFIG_PATH="$pcdir" pkg-config --static --cflags --libs hh)
    # shellcheck disable=SC2086
    ${CC:-cc} "$here/consumer.c" $static_flags -o "$work/pkg_config_c_consumer"
    "$work/pkg_config_c_consumer" > /dev/null
    echo "pkg-config (moved prefix, C++ and C): ok"
fi

cmake -S "$here/add_subdirectory" -B "$work/add_subdirectory" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$work/add_subdirectory" --parallel > /dev/null
ctest --test-dir "$work/add_subdirectory" --output-on-failure > /dev/null
echo "add_subdirectory: ok"
