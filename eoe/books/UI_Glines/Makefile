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
# FULL_TITLE = Silicon Surf Help Book
# VERSION = 1.0


TITLE = UI_Glines
FULL_TITLE = "IRIX Interactive User Interface Guidelines"
VERSION = 007-2167-005, 5/98

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
# changes stylesheet-defined strings in the book to the specified language.
# The supported values are C (the default), which is English; de for
# German; and fr for French.  No other values are supported.
#
# The DTD variable defines the Document Type Definition that the
# SGML conforms to.  Currently available options are SGIDOC for the
# standard SGI DTD; XHELP for the XHelp DTD; DOCBOOK for the Davenport
# DocBook DTD, and OTHER for any customized DTD.
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
# The SORT_ORDER variable dfines the order this book should appear on
# the bookshelf.

# The HIDDEN variable defines whether this book will be displayed when
# this book is accessed via the InSight library browser rather than the
# SGIHelp browser from the application. The default is to keep XHELP
# based books hidden and all other's not hidden.
#
# If you need to define a value other than the default, uncomment the
# appropriate line and define the value.
#
# BOOKSHELF = /usr/share/Insight/library/SGI_bookshelves/SGI_EndUser
# BOOK_ICON = generic
# BOOK_LANG = C
# DTD = SGIDOC
# DOC_TYPE = MANUAL
# SORT_ORDER = 999
# HIDDEN = FALSE

BOOKSHELF = /usr/share/Insight/library/SGI_bookshelves/SGI_Developer
BOOK_ICON = sgi_developer
#BOOK_LANG = C
#DTD = SGIDOC
DOC_TYPE = MANUAL
#SORT_ORDER = 004250
HIDDEN = FALSE

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

#BOOK_FILES = ui_glines.sgm

BOOK_FILES = 	front.doc \
		about.doc \
		part1.integrate.doc \
		ch01.deskoverview.doc \
		ch02.icons.doc \
		ch03.windows.doc \
		ch04.services.doc \
		ch05.xchange.doc \
	EndPart\
		part2.components.doc \
		ch06.windows.doc \
		ch07.focus.doc \
		ch08.menus.doc \
		ch09.controls.doc \
		ch10.dialogs.doc \
		ch11.feedback.doc \
	EndPart\
		part3.3Dstyle.doc\
		ch12.3Dintro.doc\
		ch13.3Dview.doc\
		ch14.3Dselect.doc\
		ch15.3Dmanip.doc\
		appa.doc


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

PRINT_BW = 06.addprinter.rgb \
	06.layout1.rgb \
	06.warning.rgb \
	07.location.cursor.rgb \
	08.complete.menubar.rgb \
	08.editmenu.rgb	\
	08.filemenu.rgb \
	08.menubar.rgb \
	08.popup.rgb \
	09.list.rgb \
	10.product.info.rgb \
	arrowleft.rgb \
	catapult.cascade.rgb \
	catapult.rgb \
	dial.pointer.rgb \
	dial.rgb \
	error.host.rgb \
	fig.1-1.rgb	\
	fig.1-2.rgb	\
	fig.15-5.rgb	\
	fig.15-6.rgb	\
	fig.2-2.rgb	\
	fig.2-4.rgb	\
	fig.2-6.rgb	\
	fig.2-7.rgb	\
	fig.2-8.rgb	\
	fig.2-9.rgb	\
	fig.2-10.rgb	\
	fig.2-11.rgb	\
	fig.2-13.rgb	\
	fig.3-1.rgb	\
	fig.3-2.rgb	\
	fig.3-3.rgb	\
	fig.3-4.rgb	\
	fig.3-7.rgb	\
	fig.3-8.rgb	\
	fig.3-9.rgb	\
	fig.3-10.rgb	\
	fig.3-11.rgb	\
	fig.3-12.rgb	\
	fig.3-13.rgb	\
	fig.3-14.rgb	\
	fig.3-15.rgb	\
	fig.3-16-17.rgb	\
	fig.4-1.rgb	\
	fig.4-2.rgb	\
	fig.4-3.rgb	\
	fig.4-5.rgb	\
	fig.4-10.rgb	\
	fig.5-1a.rgb	\
	fig.5-1b.rgb	\
	fig.5-2.rgb	\
	fig.5-3.rgb	\
	fig.5-4.rgb	\
	fig.6-10.rgb	\
	fig.6-9.rgb	\
	fig.8-11.rgb	\
	fig.8-12.rgb	\
	fig.9-3.rgb	\
	fig.9-4.rgb	\
	fig.9-5.rgb	\
	fig.9-7.rgb	\
	fig.10-1.rgb	\
	fig.findicon.rgb \
	fig.helpmenu.iconcat.nosub.rgb \
	fig.iconcatalog2.rgb \
	fig.launch.rgb \
	fig.minim.broken.rgb	\
	fig.minim.idle.rgb	\
	fig.minim.inproc.rgb	\
	fig.set.desk.rgb	\
	fig.set.mouse.rgb	\
	fig.set.lang.rgb	\
	figure3.5a.rgb \
	figure3.5b.rgb \
	figure3.5c.rgb \
	figure3.5d.rgb \
	fileseldialog.rgb \
	finder.rgb \
	ibeam.rgb \
	optionbutton.rgb \
	optionmenu.rgb \
	primaryside.rgb \
	primarytool.rgb \
	primarytwopanes.rgb \
	scaleaspercent.rgb \
	slider.rgb \
	taskoriented.help.rgb \
	thumbwheel.rgb \
	warning.overwrite.rgb \
	warning.revert.rgb \
	warning.save.rgb \
	wkingpercent.dialog.rgb \
	09.buttons.rgb \
	arrowright.rgb \
	crosshair.rgb \
	fourdirection.rgb \
	question.rgb \
	resize.rgb \
	watch.rgb \
	fig.buffy.ex2B.rgb \
	fig.gizmo.big.rgb \
	fig.gizmo.def.rgb \
	fig.gizmo.min.rgb \
	fig.help.windows.keys.rgb \
	fig.help.windows.rgb \
	fig.iconcomp.rgb \
	fig.iconsmith.palette.rgb \
	edit_cursor.rgb\
	seek_cursor.rgb\
	sidle_cursor.rgb\
	tilt_cursor.rgb\
	walk_cursor.rgb\
	pan_cursor.rgb\
	dolly_cursor.rgb\
	tumble_cursor.rgb\
	fixtures.rgb\
	leadmanip.rgb\
	group.rgb\
	ungroup.rgb\
	transmanip.rgb\
	trans123.rgb	\
	trans1.rgb\
	trans2.rgb\
	trans3.rgb\
	transS1.rgb\
	transS2.rgb\
	transS3.rgb\
	transC123.rgb	\
	transC1.rgb\
	transC2.rgb\
	transC3.rgb\
	rotS1.rgb\
	rotS2.rgb\
	scalemanip.rgb\
	scale1.rgb\
	scale2.rgb\
	scaleA1.rgb\
	scaleA2.rgb\
	changecenter1.rgb\
	changecenter2.rgb\
	changecenterA1.rgb\
	changecenterA2.rgb\
	scaleC1.rgb\
	scaleC2.rgb\
	scaleS1.rgb\
	scaleS2.rgb\
	scaleS3.rgb\
	twobox.rgb\
	manip.rgb
	

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

PostScript = 	dollying.ai\
		panning.ai\
		roam.ai\
		seeking.ai\
		tilt.ai\
		tumbling.ai\
		view_intro.ai\
		zooming.ai

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
# MISC_FILES = figure1.rgb.hot

# MISC_FILES = 

include  ${COMMONBOOKRULES}
include  ${COMMONPREPRULES}










