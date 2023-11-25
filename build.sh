#!/bin/sh

mkdir release/ > /dev/null 2>&1
cmake -D BUILD_SHARED_LIBS=OFF -B release/

make -C release/
