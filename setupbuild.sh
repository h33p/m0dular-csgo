#!/bin/sh

BUILD_TARGET=$1
shift
CROSS_FILE_ENDING='_meson.txt'
CROSS_FILE="$BUILD_TARGET$CROSS_FILE_ENDING"
BUILD_TYPE=$1
shift
ARGS=$@

if [ -z "$BUILD_TYPE" ]; then
	BUILD_TYPE="release"
fi

echo $CROSS_FILE
echo $BUILD_TYPE

if [ -d build_$BUILD_TARGET ]; then
    meson reconfigure build_$BUILD_TARGET --cross-file $CROSS_FILE -Dmsvc_dir=$WBUILD --buildtype=$BUILD_TYPE $ARGS
else
    meson build_$BUILD_TARGET --cross-file $CROSS_FILE -Dmsvc_dir=$WBUILD --buildtype=$BUILD_TYPE $ARGS
fi
