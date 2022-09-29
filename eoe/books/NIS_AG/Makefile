#!smake -r
# Makefile for making a book
#
# Building books and figures:
# To build the print figures for the book issue the comman 'make print',
#    this must be done in order to populate the print directory with your
#    figures - your files must reference these figures from the print
#    directory not the orig directory.
# To build the book issue the command 'make book'.
#
# Cleaning up files:
# To remove the master sgml file and books directory use 'make clean_book'.
# To remove the *.sgm files issue 'make clean_sgm'.
# To clean up all the figures use 'make clean_figures',
#   only do this if you really want to clean up your figures directories and
#   you haven't done any figure conversions by hand.
# To clean up everything issue 'make clobber' - this includes the figures.

# Edit this file in the locations specified.

# Location : 1
# Please give the short title of the book in the following line.
# Do not have a trailing space on the TITLE line.
#
# TITLE = WorkSpace

TITLE  = NIS_AG
FULL_TITLE = "NIS Administrator's Guide"
VERSION = 007-2161-004

include  ${ROOT}/usr/include/make/commonbookdefs

# Location : 2
# This allows production to place the installed book on the specified
# bookshelf using the BOOKSHELF variable.  The default BOOKSHELF is
# /usr/share/Insight/library/SGI_bookshelves/SGI_EndUser.
#
# The DOC_TYPE variable defines the SGML tag type for the contents of
# the book.  All Tech Pubs books are MANUAL (which is the default) while
# CSD Support books should be SUPPORT.  No other values are supported in the
# stylesheet yet, so use only MANUAL or SUPPORT.
#
BOOKSHELF = /usr/share/Insight/library/SGI_bookshelves/SGI_Admin
# DOC_TYPE = MANUAL

# Location : 3
# Please give the list of files in *CORRECT* order as they form the book.
# Use trailing '\' for continuing onto the next line.
# Don't end the last line of file names with trailing '\'.
# Successive lines that have file names should have a leading tab character.
#
# BOOK_FILES = 	front.doc		\
#		ch1.doc  ch2.doc	\
#		appa.doc appb.doc

BOOK_FILES = 	frnt.doc whatsnew.doc \
		RR.doc intr.doc \
		ch01.doc ch02.doc \
		ch03.doc ch04.doc \
		ch05.doc ch06.doc
 
include  ${COMMONBOOKRULES}

# NOTE: every figure you want to appear in your book and online MUST appear
# once on either the PRINT_BW or PRINT_COLOR line.
#
# All figure names must be the name as it appears in the 'orig' directory.

# Location : 4
# List the figure name from the orig directory for all figures that should 
# be converted to black and white in the print directory here.  These
# figures must be rgb format figures and MUST end in '.rgb'.  These figures
# will also be converted to gif for the online book.
#
# PRINT_BW = figure1.rgb figure2.rgb

PRINT_BW = 

# Location : 5
# List the figure name from the orig directory for all figures that should 
# be kept in color in the print directory here.  This is also where you
# list any figures that are already in Black&White so don't need converting
# to bw.  Also, if you want to leave a figure in full 24-bit RGB in the hard
# copy book then place it on this line.  These figures will also get converted
# to gif for the online book.  If you don't want a figure converted to gif,
# bug kept RGB, then add it also on the ONLINE_RGB line below.
#
# PRINT_COLOR = figure3.rgb figure4.rgb

PRINT_COLOR = 

# Location : 6
# List the figure name from the orig directory for all PostScript figures that
# should be in the online directory.
#
# PostScript = figure5.ps figure6.ai

PostScript = fig1-2.ai fig1-3.ai

# Location : 7
# List the figure name from the orig directory for all GIF figures that
# should be in the online directory.
#
# GIF = figure7.gif

GIF = 

# Location : 8
# List the figure name from the orig directory for all RGB figures that
# should be in the online directory.
#
# RGB = figure8.rgb

RGB = 

# Location : 9
# List the figure name from the orig directory for all XWD figures that
# should be in the online directory.
#
# XWD = figure9.xwd

XWD = 

include  ${COMMONPREPRULES}
