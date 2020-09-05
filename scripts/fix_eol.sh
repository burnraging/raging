#!/bin/bash

find . -name "*.c" -type f -exec unix2dos {} \;
find . -name "*.h" -type f -exec unix2dos {} \;
find . -name "*.s" -type f -exec unix2dos {} \;

find . -name "*.sh" -type f -exec dos2unix {} \;
