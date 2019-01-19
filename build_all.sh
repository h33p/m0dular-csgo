#!/bin/sh

BUILD_TYPE=$1
shift
ARGS=$@
RECONFIGURE=0

if [ $1=="reconfigure" ]; then
    shift
    ARGS=$@
    RECONFIGURE=1
fi

if [ -z "$BUILD_TYPE" ]; then
	  BUILD_TYPE="release"
fi

./setupbuild.sh windows $BUILD_TYPE $ARGS

if [ -d build ]; then
    meson --reconfigure build --buildtype=$BUILD_TYPE $ARGS
else
    meson build --buildtype=$BUILD_TYPE $ARGS
fi

ninja -C build
ninja -C build_windows
