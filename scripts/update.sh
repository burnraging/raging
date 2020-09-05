#!/bin/bash

#   Remove all existing files that we use to search
rm -rf *.files *.out tags

#   Build all the tags
ctags -R --c++-kinds=+p --fields=+iaS --extras=+q ../

# find all files for cscope to process
find ../ -name "*.c" -o -name "*.cpp" -o -name "*.S" -o -name "*.s" -o -name "*.h" -o -name "*.hpp" -o -name "*.inc" > cscope.files

#   Generate lookup and reverse lookup databases
cscope -b -q -k

