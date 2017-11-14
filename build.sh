#!/usr/bin/env bash

SOURCE=$(find . -type f -name "*.c")
HEADER="-I."
LIBS="-L."

for c in $SOURCE ;
do    
    echo -e "\e[1;34mCompiling $c …\e[0m"
    clang -DxDEBUG -g -Wno-multichar --std=gnu99 $HEADER -O2 -g -c -o "${c%.*}.o" $c
    if [[ $? -ne 0 ]] ; then
        echo -e "\e[1;31mfailed\e[0m"
    fi
done


OBJECTS=$(find . -type f -name "*.o")

echo -e "\e[1;34mLinking …\e[0m"
clang -o onpts $OBJECTS $LIBS
if [[ $? -ne 0 ]] ; then
    echo -e "\e[1;31mfailed\e[0m"
else
    echo -e "\e[1;32mdone\e[0m"
fi

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

