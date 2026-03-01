#! /usr/bin/bash
set -e

find src/ -name *.cpp -o -name *.hpp | parallel -j 4 clang-tidy {} -p build/compile_commands.json

echo clang-tidy checks passed
