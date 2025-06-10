#!/bin/sh

set -xe

gcc -Wall -Wextra -std=c11 -pedantic src/*.c -o jis
