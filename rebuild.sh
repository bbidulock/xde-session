#!/bin/bash

rm -f cscope.*
./autogen.sh
./configure.sh
make cscope
cscope -b
make V=0 clean all
