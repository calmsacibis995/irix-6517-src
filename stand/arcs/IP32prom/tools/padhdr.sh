#!/bin/sh 

cat > $1pad.s << EOF
.set noreorder
.set nomacro
.repeat 0
nop
.endr

