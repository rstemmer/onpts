#!/usr/bin/env bash

echo -e "onpts  Copyright (C) 2017  Ralf Stemmer <ralf.stemmer@gmx.net>"
echo -e "This program comes with \e[1;33mABSOLUTELY NO WARRANTY\e[0m."
echo -e "This is free software, and you are welcome to redistribute it"
echo -e "under certain conditions.\n"

PREFIX=/usr/local

# sets suid-bit
install -m 4755 -v -s -g root -o root onpts   -D $PREFIX/bin/onpts
install -m  644 -v    -g root -o root onpts.1 -D $PREFIX/share/man/man1/onpts.1

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

