curdir=$(pwd)
builddir="$(pwd)/release"
cmake -DCMAKE_TOOLCHAIN_FILE="$curdir/cross.cmake" -DCMAKE_BUILD_TYPE=Release -B $builddir && make -j$(nproc) -C $builddir
