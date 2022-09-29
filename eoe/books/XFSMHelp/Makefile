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
# VERSION = 1.0

TITLE  = XFSMhelp
FULL_TITLE = "XFS Manager Help Cards"
VERSION = 007-2874-001, 9/95

include  ${ROOT}/usr/include/make/commonbookdefs

# Location 2: The BOOKSHELF, BOOK_LANG, DOC_TYPE, and HIDDEN Variables
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
# changes translator-defined strings in the book to the specified language.
# The supported values are C (the default), which is English; de for
# German; fr for French; and ja_JP.EUC for Japanese.  Case of the letters
# is important. No other values are supported other than C, de, fr, and
# ja_JP.EUC.
#
# The DTD variable defines the Document Type Definition that the
# SGML conforms to.  Currently available options are SGIDOC for the
# standard SGI DTD; XHELP for the XHelp DTD; DOCBOOK for the Davenport
# DocBook DTD, and OTHER for any customized DTD.  Case of the letters
# is important.
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
# is to use the SGIDOC DTD.
#
# The DOC_TYPE variable defines the SGML tag type for the contents of
# the book.  All Tech Pubs books are MANUAL (which is the default) while
# CSD Support books should be SUPPORT.  No other values are supported.
#
# The SORT_ORDER variable defines the order this book should appear on
# the bookshelf.  It is a numeric value that can be used to sort the
# books on a bookshelf.  This number should be gotten from your
# production editor.  A default of 999 is used if none is specified.
#
# The HIDDEN variable defines whether this book will be displayed when
# this book is accessed via the InSight library browser rather than the
# SGIHelp browser from the application. The default is to keep XHELP
# based books hidden and all other's not hidden.
#
# If you need to define a value other than the default, uncomment the
# appropriate line and define the value.
#
BOOKSHELF = /usr/share/Insight/library/SGI_bookshelves/Help
#BOOK_ICON = sgi_admin 
BOOK_LANG = C
DTD = SGIDOC
DOC_TYPE = MANUAL
#SORT_ORDER = 999
HIDDEN = TRUE

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

BOOK_FILES = xfsm.front.doc ch01.doc ch02.doc ch03.doc ch04.doc 

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

PRINT_BW = ch02.dialog.btns.rgb       ch03.unex.fs.rgb \
ch02.disk.part.rgb         ch03.unmount.rgb \
ch02.diskinfo.rgb          ch03.volpart.rgb \
ch02.hostdisplay.rgb       ch03.volsearch.rgb \
ch02.hostsearch.rgb        ch03.xfsm.rgb \
ch02.mod.graphic.rgb       ch04.attach.rgb \
ch02.param.fields.rgb      ch04.attachplex.rgb \
ch02.part.graphic.rgb      ch04.createplex.rgb \
ch02.part.matrix.rgb       ch04.createplexfields.rgb \
ch02.view.btns.rgb         ch04.createve.rgb \
ch02.xdkm.rgb              ch04.createvol.rgb \
ch02.xdkm.save.rgb         ch04.createvolfields.rgb \
ch02.xdkm.temp.rgb         ch04.crplexdatamatrix.rgb \
ch03.cfs.adv.rgb           ch04.crve.fields.rgb \
ch03.cfs.buttons.rgb       ch04.curhost.rgb \
ch03.cfs.efs.adv.rgb       ch04.datainputmatrix.rgb \
ch03.cfs.export.rgb        ch04.detach.rgb \
ch03.cfs.fstype.rgb        ch04.hostsearch.rgb \
ch03.cfs.hostname.rgb      ch04.partsel.rgb \
ch03.cfs.mntpnt.rgb        ch04.partsrch.rgb \
ch03.cfs.xfs.adv.rgb       ch04.partsrchres.rgb \
ch03.createfs.rgb          ch04.plexgraphi.rgb \
ch03.errormsg.rgb          ch04.plexinfo.rgb \
ch03.export.rgb            ch04.remove.rgb \
ch03.getinfo.rgb           ch04.template.rgb \
ch03.hostdisplay.rgb       ch04.veinfo.rgb \
ch03.hostsearch.rgb        ch04.volinfo.rgb \
ch03.mount.adv.rgb         ch04.volinfobtn.rgb \
ch03.mount.rgb             ch04.volumegraphic.rgb \
ch03.partsearch.rgb        ch04.xlvm.pop.rgb \
ch03.remove.rgb            ch04.xlvm.rgb \
ch03.unex.dir.rgb 	   ch02.scale.rgb \
ch03.optmenu.rgb	   ch04.icons.rgb \
ch02.mpt1.rgb		   ch02.mpt4.rgb \
ch02.mpt2.rgb		   ch02.mpt5.rgb \
ch02.mpt3.rgb



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

# Location 6: The PostScript Variable
#
# List the name of each PostScript figure in the orig directory that
# should be remain PostScript in the print directory and convert to 
# GIF online.
#
# PostScript = figure5.ps figure6.ai

PostScript =

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

INLINE_MEDIA = 

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

include  ${COMMONBOOKRULES}
include  ${COMMONPREPRULES}
