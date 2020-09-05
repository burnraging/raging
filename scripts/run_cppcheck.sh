#!/bin/bash

## remove files
rm -rf cppcheck/check.files

## create file list
find -name "*.c" > cppcheck/check.files

cppcheck -j16 --quiet --std=c99 --inline-suppr --inconclusive \
         --includes-file=cppcheck/include.files --enable=warning,performance,information,style \
         --file-list=cppcheck/check.files \
         --template='{file}({line}): {severity} ({id}) {message}' \
         --suppressions-list=cppcheck/suppression.files \
         -DDEBUG --check-config
         
cppcheck --quiet --std=c99 --inline-suppr --inconclusive \
         --includes-file=cppcheck/include.files --enable=unusedFunction \
         --file-list=cppcheck/check.files \
         --template='{file}({line}): {severity} ({id}) {message}' \
         --suppressions-list=cppcheck/suppression.files \
         -DDEBUG --check-config         
         
