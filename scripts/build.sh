#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Unified build front-end for neorv32-sfp.
#
# Usage:
#   scripts/build.sh [options]
#
# Options:
#   --debug              Build firmware in Debug configuration (default: Release)
#   --release            Build firmware in Release configuration (default)
#   --tests              Also build and run the host-side unit tests
#   --tests-only         Skip firmware; build/run only the host tests
#   --toolchain <pfx>    Cross toolchain prefix (e.g. /opt/riscv/bin/riscv32-unknown-elf-)
#   --install            Install host build dependencies via apt (sudo) and EXIT.
#                        This option is exclusive: run it once on a fresh system,
#                        then re-invoke the script without --install for builds.
#                        Rationale: any cmake/make work performed in the same run
#                        would inherit root privileges and pollute the workspace
#                        with root-owned build artefacts.
#   --clean              Remove existing build directories before configuring
#   --jobs <N> | -j <N>  Parallel build jobs (default: nproc)
#   --build-dir <dir>    Override base build directory (default: ./build)
#   --verbose            Verbose build output
#   -h | --help          Print this message
#
# Examples:
#   scripts/build.sh                          # release firmware
#   scripts/build.sh --debug                  # debug firmware
#   scripts/build.sh --tests                  # firmware + host tests
#   scripts/build.sh --tests-only             # host tests only
#   scripts/build.sh --toolchain ~/riscv/bin/riscv32-unknown-elf-
#   scripts/build.sh --install                # one-shot dependency install
# -----------------------------------------------------------------------------
set -euo pipefail

# ---- Defaults ---------------------------------------------------------------
BUILD_TYPE=Release
BUILD_TESTS=0
TESTS_ONLY=0
DO_INSTALL=0
DO_CLEAN=1
VERBOSE=0
JOBS="$(nproc 2>/dev/null || echo 4)"
TOOLCHAIN_PREFIX=""
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_BASE="${PROJECT_ROOT}/build"

# ---- Helpers ----------------------------------------------------------------
log()  { printf '\033[1;34m[build]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m  %s\n' "$*"; }
die()  { printf '\033[1;31m[err]\033[0m   %s\n' "$*" >&2; exit 1; }

print_help() {
    # Print the leading banner comment block (everything before the
    # first non-comment, non-empty line at the top of this file).
    awk 'NR==1 {next} /^[[:space:]]*$/ || /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
}

# ---- Argument parsing -------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)         BUILD_TYPE=Debug ;;
        --release)       BUILD_TYPE=Release ;;
        --tests)         BUILD_TESTS=1 ;;
        --tests-only)    BUILD_TESTS=1; TESTS_ONLY=1 ;;
        --toolchain)     TOOLCHAIN_PREFIX="${2:?--toolchain requires an argument}"; shift ;;
        --toolchain=*)   TOOLCHAIN_PREFIX="${1#*=}" ;;
        --install)       DO_INSTALL=1 ;;
        --no-clean)      DO_CLEAN=0 ;;
        --jobs|-j)       JOBS="${2:?--jobs requires an argument}"; shift ;;
        --jobs=*)        JOBS="${1#*=}" ;;
        --build-dir)     BUILD_BASE="${2:?--build-dir requires an argument}"; shift ;;
        --build-dir=*)   BUILD_BASE="${1#*=}" ;;
        --verbose)       VERBOSE=1 ;;
        -h|--help)       print_help; exit 0 ;;
        *) die "Unknown option: $1 (use --help)" ;;
    esac
    shift
done

# ---- Dependency install (exclusive mode) ------------------------------------
# --install requires sudo; running any subsequent build step in the same
# invocation would create root-owned artefacts in the workspace.  To keep the
# build tree owned by the unprivileged user, --install is mutually exclusive
# with every other action: do the apt work, then exit.
if [[ $DO_INSTALL -eq 1 ]]; then
    EXTRA_FLAGS=()
    [[ $BUILD_TESTS -eq 1 ]] && EXTRA_FLAGS+=("--tests")
    [[ $TESTS_ONLY  -eq 1 ]] && EXTRA_FLAGS+=("--tests-only")
    [[ $DO_CLEAN    -eq 1 ]] && EXTRA_FLAGS+=("--clean")
    [[ $VERBOSE     -eq 1 ]] && EXTRA_FLAGS+=("--verbose")
    [[ -n "${TOOLCHAIN_PREFIX}" ]] && EXTRA_FLAGS+=("--toolchain")
    if [[ ${#EXTRA_FLAGS[@]} -gt 0 ]]; then
        die "--install is exclusive; remove: ${EXTRA_FLAGS[*]}"
    fi

    log "Installing host build dependencies (sudo apt)"
    if ! command -v apt-get >/dev/null 2>&1; then
        die "--install only supports apt-based distributions"
    fi
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git
    log "Host dependencies installed.  Re-run the script without --install to build."
    exit 0
fi

# ---- Clean ------------------------------------------------------------------
if [[ $DO_CLEAN -eq 1 ]]; then
    log "Cleaning ${BUILD_BASE}"
    rm -rf "${BUILD_BASE}"
fi

# ---- Verbose flag ----------------------------------------------------------
CMAKE_VERBOSE=()
if [[ $VERBOSE -eq 1 ]]; then
    CMAKE_VERBOSE=(--verbose)
fi

# ---- Firmware build ---------------------------------------------------------
if [[ $TESTS_ONLY -eq 0 ]]; then
    FW_DIR="${BUILD_BASE}/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
    log "Configuring firmware (${BUILD_TYPE}) -> ${FW_DIR}"

    CMAKE_ARGS=(
        -DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/cross.cmake"
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -B "${FW_DIR}"
        -S "${PROJECT_ROOT}"
    )
    if [[ -n "${TOOLCHAIN_PREFIX}" ]]; then
        CMAKE_ARGS+=( -DTOOLCHAIN_PREFIX="${TOOLCHAIN_PREFIX}" )
    fi

    cmake "${CMAKE_ARGS[@]}"
    log "Building firmware (-j ${JOBS})"
    cmake --build "${FW_DIR}" -j "${JOBS}" "${CMAKE_VERBOSE[@]}"
fi

# ---- Tests build / run ------------------------------------------------------
if [[ $BUILD_TESTS -eq 1 ]]; then
    TEST_DIR="${BUILD_BASE}/tests"
    log "Configuring host tests -> ${TEST_DIR}"

    cmake \
        -DBUILD_TESTS=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        -B "${TEST_DIR}" \
        -S "${PROJECT_ROOT}"

    log "Building tests (-j ${JOBS})"
    cmake --build "${TEST_DIR}" -j "${JOBS}" "${CMAKE_VERBOSE[@]}"

    log "Running tests"
    ctest --test-dir "${TEST_DIR}" --output-on-failure
fi

log "Done."
