#!/usr/bin/env bash
set -e

full=1
debug=1
release=0
vst=0
standalone=0
clap=0
data=0
hotreload=0
reloadable=0

for arg in "$@"; do
  case "$arg" in
    full|debug|release|vst|standalone|clap|data|hotreload|reloadable)
      declare "$arg=1"
      ;;
  esac
done

echo

[[ $data == 1 ]] && { vst=0; debug=0; full=0; }
[[ $standalone == 1 ]] && { echo "[standalone build]"; vst=0; clap=0; full=0; }
[[ $clap == 1 ]] && { echo "[clap build]"; vst=0; standalone=0; full=0; }
[[ $vst == 1 ]] && { echo "[vst build]"; standalone=0; clap=0; full=0; }
[[ $hotreload == 1 ]] && { echo "[hotreload build]"; full=0; }
[[ $reloadable == 1 ]] && echo "[reloadable version]"

if [[ $release == 1 ]]; then
    echo "[release mode]"
    debug=0
else
    echo "[debug mode]"
fi

echo

CXX=${CXX:-g++}
CC=${CC:-gcc}

command -v "$CXX" >/dev/null || { echo "clang++ not found"; exit 1; }

TOP="$(cd "$(dirname "$0")" && pwd)"

timestamp_ms() {
    perl -MTime::HiRes=time -e 'printf "%.0f\n", time()*1000'
}

start_timer() {
    START=$(timestamp_ms)
}

stop_timer() {
    END=$(timestamp_ms)
    ELAPSED=$((END - START))
    printf "Elapsed: %d.%03d seconds\n" \
        $((ELAPSED / 1000)) \
        $((ELAPSED % 1000))
}

data_gen(){
    echo "[serialising data]"
    pushd helpers >/dev/null
    rm -rf build
    mkdir build
    pushd build >/dev/null
    $CC ../resource_generator.c -o resource_generator
    ./resource_generator ../../Data ../../Source/Data
    popd >/dev/null
    rm -rf build
    popd >/dev/null
}

project_compile() {
    datetime=$(date +"%Y%m%d-%H%M%S")
    build_dir=build
    hotreload_dir=$build_dir/hotreload
    mkdir -p "$hotreload_dir"

    sources=(
      "$TOP/Source/unity1.mm"
      "$TOP/Source/unity2.mm"
    )
    c_sources=(
      "$TOP/Source/unity_extern.m"
    )

    cflags=(-I"$TOP/Source" -Wall -Wno-missing-braces -Wno-multichar -DPUGL_STATIC -march=armv8.1-a)
    ldflags=(-framework CoreFoundation -framework Foundation -framework CoreGraphics -framework CoreVideo -framework AppKit)

    [[ $reloadable == 1 ]] && cflags+=(-DCOMPLEX_HOTRELOAD_DIR="\"$hotreload_dir\"")

    bundle_type="BNDL"
    if [[ $hotreload == 1 ]]; then
        build_dir=$hotreload_dir
        outfile="Complex_${datetime}.dylib"
        ldflags+=(-dynamiclib -fPIC)
    elif [[ $vst == 1 ]]; then
        rm -rf "$hotreload_dir"/*
        build_dir=build/vst3
        outfile="Complex.vst3"
        ldflags+=(-dynamiclib -fPIC)
    elif [[ $clap == 1 ]]; then
        rm -rf "$hotreload_dir"/*
        build_dir=build/clap
        outfile="Complex.clap"
        cflags+=(-DCOMPLEX_CLAP)
        ldflags+=(-dynamiclib -fPIC)
    elif [[ $standalone == 1 ]]; then
        rm -rf "$hotreload_dir"/*
        build_dir=build/standalone
        outfile="Complex.app"
        bundle_type="APPL"
        cflags+=(-DCOMPLEX_STANDALONE)
        ldflags+=(-framework CoreAudio -framework CoreMIDI)
    fi

    if [[ $debug == 1 ]]; then
        [[ $hotreload == 0 ]] && build_dir="$build_dir/debug"
        cflags+=(-g -O0)
    else
        [[ $hotreload == 0 ]] && build_dir="$build_dir/release"
        cflags+=(-O3 -flto)
    fi

    [[ $data == 1 ]] && { data_gen; return; }
    [[ -d Source/Data ]] || data_gen

    mkdir -p "$build_dir"
    start_timer
    pushd "$build_dir" >/dev/null

    if [[ $hotreload == 0 ]]; then
        rm -rf ./*
        mkdir -p "$outfile"
        mkdir -p "$outfile/Contents"
        mkdir -p "$outfile/Contents/MacOS"
        mkdir -p "$outfile/Contents/Resources"

        pushd "$outfile/Contents" >/dev/null
        # unquote all strings in the config file because it will interfere with the plist values
        tr -d '"' < "$TOP/Source/Third Party/cplug/config.h" > config.h
        "$CC" -E -P -x c -Wno-trigraphs -include "./config.h" -DCPLUG_PLUGIN_EXECUTABLE_NAME="$outfile" -DCPLUG_BUNDLE_TYPE="$bundle_type" "$TOP/Helpers/Info.plist.in" -o Info.plist
        rm ./config.h
        pushd "MacOS" >/dev/null
    fi

    "$CC" "${cflags[@]}" -std=c99 -o unity_extern.o -c "${c_sources[@]}"
    "$CXX" "${cflags[@]}" -std=c++20 "${sources[@]}" unity_extern.o -o "$outfile" "${ldflags[@]}"
    rm -f ./*.o

    if [[ $hotreload == 0 ]]; then
        popd >/dev/null
        popd >/dev/null
    fi

    echo "$PWD"
    popd >/dev/null
    stop_timer
}

if [[ $full == 1 ]]; then
    debug=0
    reloadable=0
    vst=1; project_compile; vst=0
    clap=1; project_compile; clap=0
elif [[ $release == 1 ]]; then
    debug=0
    project_compile
else
    release=0
    project_compile
fi

echo