#!/bin/sh
PATH=$PATH:.
genfile file.1b 1
genfile file.1k 1024
genfile file.4k 4096
genfile file.8k 8192
genfile file.16k 16384
genfile file.37k `expr 37 \* 1024`
genfile file.87k `expr 87 \* 1024`
genfile file.32k 32768
genfile file.32k+1b 32769
genfile file.64k 65536
genfile file.64k+1b 65537
genfile file.1m `expr 1024 \* 1024`
genfile file.96k `expr 96 \* 1024`
cat file.1m file.1m file.1m file.1m file.1m file.1m file.1m file.1m > file.8m
cat file.1m file.1m > file.2m
