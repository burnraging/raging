#!/bin/bash

#   Tell Cscope to use gvim ( this will allow multiple files to be open )
export EDITOR=gvim

#   Search with the just built databases
cscope -d

