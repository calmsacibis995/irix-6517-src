# ident $Revision: 1.1 $
#
# Makefile for release notes.
# See $(ROOT)/usr/lib/doc/doc/make_targets for a list of available make
# targets.
#
DEPTH= ../..
include ${DEPTH}/Makedefs

#	Edit the CMM and AMM macros to contain the list of chapters and 
#	appendices in this book.  Do not use abbreviations (i.e. ch[1-8].cmm).
#	The sort order of the file names must be the same as the chapter
#	order, so if there are more than nine chapters, name them ch01.cmm, etc.
CMM= ch1.cmm ch2.cmm ch3.cmm ch4.cmm ch5.cmm 
AMM=

#	Set RELPROD to the short installation name for your product -- 
#	your release notes subsystem will be called RELPROD.man.relnotes
RELPROD= netvis

include $(ROOT)/usr/include/make/relnotesrules

#	Set IDB according to Makedefs
IDB_TAG= ${RELNOTESIDB}
