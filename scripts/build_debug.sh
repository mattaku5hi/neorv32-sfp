curdir=$(pwd)
builddir="$(pwd)/debug"
cmake -DCMAKE_TOOLCHAIN_FILE="$curdir/cross.cmake" -DCMAKE_BUILD_TYPE=Debug -B $builddir && make -j$(nproc) -C $builddir
