#!/bin/bash

./autogen.sh
./configure.sh
rm -f cscope.*
make clean all cscope
