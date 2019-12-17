#!/usr/bin/env bash

[ $(which clang-format-3.9) ] && CLANG_FORMAT=clang-format-3.9 || CLANG_FORMAT=clang-format
for DIRECTORY in src include tool tests
do
    echo "Formatting code under $DIRECTORY/"
    find "$DIRECTORY" \( -name '*.h' -or -name '*.c' -or -name '*.cpp' \) -print0 | xargs -0 $CLANG_FORMAT -i
done
