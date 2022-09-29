#!smake -r
# Makefile for making a book
#
# Building books and figures:
# To build the print figures for the book issue the command 'make print',
#    this must be done in order to populate the print directory with your
#    figures - your files must reference the figures from the print
#    directory not the orig directory.
# To build the book issue the command 'make book'.
#
# Cleaning up files:
# To remove the master sgml file and books directory use 'make clean_book'.
# To remove the *.sgm files issue 'make clean_sgm'.
# To clean up all the figures use 'make clean_figures'.
#   Do this only if you really want to clean up your figures directories and
#   you haven't done any figure conversions by hand.
# To clean up everything issue 'make clobber' - this includes the figures.

# Edit this file in the locations specified.

# Location 1: The TITLE, FULL_TITLE, and VERSION Variables
#
# TITLE defines the short title of the book.  Please verify the
# short title with your production editor.  
#
# This title must be a valid directory name and not contain any special
# characters or a leading '.'  Do not have a trailing space on the TITLE line.
#
# FULL_TITLE defines the full title for the book as it should appear
# next to the icon on an InSight bookshelf.
#
# VERSION defines the version number for this release of the product
# and is displayed when the 'Product Info' item off the 'Help' menu is
# selected.
#
# TITLE = SiliconSurf
# FULL_TITLE = "Silicon Surf Help Book"
# VERSION = 007-1234-567

TITLE = IA_Resource
FULL_TITLE = "IRIX Admin: Resource Administration"
VERSION = 007-3700-002

include  ${ROOT}/usr/include/make/commondocdefs

# Location 2: The BOOKSHELF, BOOK_ICON, BOOK_LANG, DTD, NUMBERED_HEADS, 
# DOC_TYPE, HIDDEN, and FEEDBACK_ADDR Variables
#
# The BOOKSHELF variable defines the bookshelf that the book will
# appear on in the Insight library view. Valid entries for SGI
# Tech Pubs are SGI_EndUser (the default), SGI_Developer, and 
# SGI_Admin. 
#
# The BOOK_ICON variable defines the icon the book will have on the
# bookshelf.  The values available are 'sgi_end_user' for End User books
# 'sgi_admin' for Administrator books, 'sgi_developer' for Developer books,
# 'binder' for Support and other "binder" type books, and 'generic' for all
# other books.  The default is 'generic'.
#
# The BOOK_LANG variable defines the base language of the book and
# changes stylesheet-defined strings in the book to the specified
# language.  The supported values are C (the default) for English, de for
# German, fr for French, es for Spanish, ja_JP.EUC for Japanese EUC,
# ja_JP.SJIS for Japanese SJIS, zh_CN.ugb for Simplified Chinese,
# zh_CN.gbk for Simplified Chinese (GBK), zh_TW.ucns for Traditional
# Chinese, zh_TW.big5 for Traditional Chinese (BIG5), and ko_KR.euc for
# Korean. Case of the letters is important. No other values are supported
# at this time.
#
# Support exists for building a localized Back of the Book Index. To disable
# building the index define NO_BOB_INDEX.
#
# The DTD variable defines the Document Type Definition that the
# SGML conforms to.  Currently available options are SGIDOCBK for the
# new SGI DOCBOOK-style DTD; SGIDOC for the old SGI DTD; XHELP for the
# XHelp DTD; DOCBOOK for the Davenport DocBook DTD, and OTHER for any
# customized DTD.  Case of the letters is important.
#
# If OTHER is specified on the DTD variable then DTD specific
# stylesheets must be created and placed in a directory called 'styles',
# a booklist.txt file must be created, an EBT specific DTD must be
# created and called TITLE.dtd, and if the files needs a MIF to SGML
# translator then that must be specified on a variable called
# OTHER_TRANSLATOR - specifying any other arguments that that translator
# might need - other than the file to translate.
#
# All other DTD's automatically set up these for you.  The default
# is to use the new SGIDOCBK DTD.
#
# Define NUMBERED_HEADS to be Y if you wish your titles to have numbered
# heads otherwise set the value to N for no numbered heads.  The default
# when not specified is no numbered heads.
#
# The DOC_TYPE variable defines the SGML tag type for the contents of
# the book.  All Tech Pubs books are MANUAL (which is the default) while
# CSD Support books should be SUPPORT.  No other values are supported.
#
# The HIDDEN variable defines whether this book will be displayed when
# this book is accessed via the InSight library browser rather than the
# SGIHelp browser from the application. The default is to keep XHELP
# based books hidden and all other's not hidden.
#
# The FEEDBACK_ADDR variable defines a mail address for feedback for
# this book.  This address is normally defined by Pubs management.
# It's possible that it may differ depending on the group owning the
# book. The address supplied is the default address for SGI techpubs
# feedback.  To define multiple addresses, use the form: "foo@blah.com,
# bo@zo.com, do@little.com".
#
# If you need to define a value other than the default, uncomment the
# appropriate line and define the value.
#
BOOKSHELF = /usr/share/Insight/library/SGI_bookshelves/SGI_Admin
BOOKICON = sgi_admin
BOOK_LANG = C
DTD = SGIDOCBK
# NUMBERED_HEADS = N
# DOC_TYPE = MANUAL
SORT_ORDER = 000045
# HIDDEN = FALSE
# FEEDBACK_ADDR = techpubs@sgi.com

# Location 3: The BOOK_FILES Variable
#
# List the content files in *CORRECT* order that they appear in the book.
# DO NOT list any generated files (i.e., TOC, LOT, IX, etc).
# Use trailing '\' for continuing onto the next line.
# Don't end the last line of file names with trailing '\'.
# Each line of filenames after the first line should have a leading 
# tab character.
#
# BOOK_FILES = 	front.doc		\
#		ch1.doc  ch2.doc	\
#		appa.doc appb.doc

BOOK_FILES = book.sgm

# NOTES ON FIGURES: 
#
# Every figure you want to appear in paper and online MUST appear
# only once on the following lines.
#
# All figure names must be listed as they appear in the 'orig' directory.
#
# For any composite figures - list each file on the PRINT_BW line.

# Location 4: The PRINT_BW Variable
#
# List the name of each .rgb figure from the orig directory that 
# should be converted to black and white in the print directory.  
# These figures will be converted to color GIF online.
#
# PRINT_BW = figure1.rgb figure2.rgb

PRINT_BW =

# Location 5: The PRINT_COLOR Variable
#
# List the name of each .rgb figure from the orig directory that should
# be kept in color in the print directory.  These figures will be 
# converted to color GIF online.  
#
# Also list any .rgb figures in the orig directory that have been 
# processed to b/w using tobw. Note that such figures will appear 
# in black and white online. 
#
# If you don't want a figure converted to GIF online, but kept in 
# 24-bit RGB, use the RGB variable instead of the PRINT_COLOR variable.
#
# PRINT_COLOR = figure3.rgb figure4.rgb

PRINT_COLOR =

# Location 6: The EPS Variable
#
# List the name of each EPS figure in the orig directory that
# should be remain PostScript in the print directory and convert to 
# GIF online.  ".eps" is the preferred file suffix, though ".ai"
# is generally used for Illustrator files, and ".ps" is # supported
# for backwards compatibility
#
# EPS = figure5.eps figure6.ai figure7.ps

EPS = job.initiators.eps \
        limits.domains.eps 

# Location 7: The GIF Variable
#
# List the name of each .gif figure in the orig directory that
# should be kept GIF in both the print and online directories.
#
# GIF = figure7.gif

GIF =

# Location 8: The RGB Variable
#
# List the name of each .rgb figure from the orig directory that
# should be kept 24-bit RGB in both the print and online directories.
#
# RGB = figure8.rgb

RGB =

# Location 9: The XWD Variable
#
# List the name of each .xwd figure from the orig directory that
# should be kept in XWD format in the print directory and converted 
# to GIF online.
#
# XWD = figure9.xwd

XWD =

#
# Location 10: The BW Variable
#
# List the name of each .bw figure from the orig directory that
# has already been run through tobw and is referenced in your book
# by it's .bw suffix
#.
# The files should not appear on any of the above PRINT_ lines.
# These images will remain in black and white online as well.
#
# BW = figure10.bw

BW =

# Location 11: The INLINE_MEDIA Variable
#
# List the name of any inline media objects from the orig directory
# that are referenced by your book.  These could be movie, audio, and
# Inventor files.
#
# INLINE_MEDIA = movie.mv

# Location 12: The MISC_FILES Variable
#
# List the name of any other files that you want to be installed in the
# figures directory.  These files must exist in the orig directory and
# would be files like the hot-spot data files, auxillary files for
# the INLINE_MEDIA files, C program text, and any other files that
# you absoluletly want to have installed with the book.
#
# NOTE: No QA testing will be done with these figures since they may
# not be referenced directly by the SGML source file but rather indirectly
# via other files.  These files will be copied down and shipped with the
# book so make sure you don't list any cruft on this line.
#
# NOTE: If you do NOT have any figures that should be copied down - then
# do NOT uncomment this line as an empty definition will break your build.
# 
# MISC_FILES = figure1.rgb.hot

# MISC_FILES =

include  ${COMMONDOCRULES}
include  ${COMMONPREPRULES}
