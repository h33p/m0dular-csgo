#!/bin/bash

CROSS_FILE_ENDING='_meson.txt'
CROSS_FILE="$1$CROSS_FILE_ENDING"
BUILD_TYPE=$2
ARGS=$3

if [ -z "$BUILD_TYPE" ]; then
	BUILD_TYPE="release"
fi

echo $CROSS_FILE
echo $BUILD_TYPE

rm -rf build && meson build --cross-file $CROSS_FILE -Dmsvc_dir=$WBUILD --buildtype=$BUILD_TYPE $ARGS
