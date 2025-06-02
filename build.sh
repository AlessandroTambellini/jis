#!/bin/sh

set -xe

gcc -Wall -Wextra -std=c11 -pedantic lua.c tokenizer.c parser.c array.c -o lua
