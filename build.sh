#!/bin/sh

mkdir release/ > /dev/null 2>&1
cmake -B release/

make -C release/
