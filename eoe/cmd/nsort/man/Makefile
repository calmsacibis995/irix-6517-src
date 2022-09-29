#!smake
#
# Makefile for Ordinal's nsort man directory
#
# XXX	Should this directory exists at all?
# XXX	Just put vsort.man into irix/man/man.1?
# XXX	Should the user guide should be put into OnlineBooks format?
#
# The main man page targets are:
#
# default, install, clean and clobber (standard SGI Makefile targets)
#
# print-list             # writes a list of the hardcopy (.p) targets 
#                        #  for the current directory to stdout
#
# There are also targets that can be used with a specific manpage:
# 
# <name>.xpsv            # view page with $(XPSVIEW) (no file created)
# <name>.dit             # view page with $(DITVIEW) (no file created)
# <name>.p               # print page (no file created)
# <name>.ps              # create PostScript for page
# <name>.z               # create online packed version; view with
#                        # man -d <name>.z
#
# $Revision: 1.2 $

# destination for man pages of this directory
IDB_PATH=/usr/nsort/man
IDB_TAG=dba.man.nsort

ODDEVEN=-rt2

default:

include $(ROOT)/usr/include/make/mandefs
include $(ROOT)/usr/include/make/manrules

rmtargets:
