#!smake
# Makefile for the fsdump log rotation utility.
#
include ../commondefs

CFILES=rotatelogs.c
TARGETS=rotatelogs

default:${TARGETS}

include ../commonrules

install: default
	${INSTALL} ${USR_GRP} -F /var/rfindd -m 755 rotatelogs
