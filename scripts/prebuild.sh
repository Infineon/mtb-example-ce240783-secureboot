#!/bin/bash

#delete existing hex and elf files, to avoid double header/signature during postbuild

#input params
COMPILED_HEX=$1
#COMPILED_HEX_UNSIGNED=$2

if [ -e "$COMPILED_HEX.hex" ]; then
    rm "$COMPILED_HEX.hex"
    rm "$COMPILED_HEX.elf"
    echo "$COMPILED_HEX.hex and $COMPILED_HEX.elf deleted"
fi

#if [ -e "$COMPILED_HEX_UNSIGNED.hex" ]; then
#    rm "$COMPILED_HEX_UNSIGNED.hex"
#    echo "$COMPILED_HEX_UNSIGNED.hex deleted"
#fi