#!/bin/bash

script_dir=$(cd $(dirname ${BASH_SOURCE:-$0}); pwd)

cd $script_dir/test
mkdir -p build
cd build
cmake ..
make
./unit_test
