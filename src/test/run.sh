#!/bin/bash
source ~/.bashrc
bin=test
if [[ $# -lt 1 ]]; then
    echo "usage: conf [mode]"
    exit
else
    if [[ x$2 == "xsync" ]]; then
        bin=test_bounded_delay
    fi
fi
./$bin $1
