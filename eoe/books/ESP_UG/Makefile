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

TITLE  = ESP_UG
FULL_TITLE = "Embedded Support Partner User Guide"
VERSION = 007-4065-002

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
BOOK_ICON = sgi_admin
# BOOK_LANG = C
# DTD = SGIDOC
# DOC_TYPE = MANUAL
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

BOOK_FILES = FEFront.doc FEnew.doc FEIntro.doc Chap1.doc Chap2.doc Chap3.doc \
	    Chap4.doc Chap5.doc Chap6.doc Chap7.doc Chap8.doc \
	    Chap9.doc Chap10.doc Chap11.doc

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
# PRINT_BW = figure1.rgb figure2.rgb \

PRINT_BW = ascii-001.rgb \
	ascii-002.rgb \
	ascii-003.rgb \
	ascii-004.rgb \
	ascii-005.rgb \
	ascii-007.rgb \
	ascii-008.rgb \
	ascii-009.rgb \
	ascii-010.rgb \
	ascii-011.rgb \
	ascii-012.rgb \
	ascii-013.rgb \
	ascii-014.rgb \
	ascii-015.rgb \
	ascii-016.rgb \
	ascii-017.rgb \
	ascii-018.rgb \
	ascii-019.rgb \
	ascii-020.rgb \
	ascii-022.rgb \
	ascii-023.rgb \
	ascii-024.rgb \
	ascii-025.rgb \
	ascii-026.rgb \
	ascii-027.rgb \
	ascii-028.rgb \
	ascii-029.rgb \
	ascii-030.rgb \
	ascii-031.rgb \
	ascii-032.rgb \
	ascii-033.rgb \
	ascii-034.rgb \
	ascii-035.rgb \
	ascii-039.rgb \
	ascii-040.rgb \
	ascii-041.rgb \
	ascii-042.rgb \
	ascii-043.rgb \
	ascii-044.rgb \
	ascii-045.rgb \
	ascii-046.rgb \
	ascii-047.rgb \
	ascii-048.rgb \
	ascii-049.rgb \
	ascii-051.rgb \
	ascii-052.rgb \
	ascii-053.rgb \
	ascii-054.rgb \
	ascii-055.rgb \
	ascii-056.rgb \
	ascii-057.rgb \
	ascii-058.rgb \
	ascii-059.rgb \
	ascii-060.rgb \
	ascii-061.rgb \
	ascii-062.rgb \
	ascii-db.rgb \
	ascii-info-002.rgb \
	ascii-info-003.rgb \
	ascii-info-004.rgb \
	ascii-info-005.rgb \
	ascii-info-009.rgb \
	ascii-info-010.rgb \
	ascii-info-011.rgb \
	ascii-info-012.rgb \
	ascii-info-013.rgb \
	ascii-info-014.rgb \
	ascii-info-015.rgb \
	ascii-info-016.rgb \
	ascii-info-017.rgb \
	ascii-info-018.rgb \
	ascii-info-019.rgb \
	ascii-info-024.rgb \
	ascii-info-025.rgb \
	ascii-info-026.rgb \
	ascii-info-028.rgb \
	ascii-info-029.rgb \
	ascii-info-030.rgb \
	ascii-info-031.rgb \
	ascii-info-032.rgb \
	ascii-info-033.rgb \
	ascii-info-034.rgb \
	ascii-info-035.rgb \
	ascii-info-036.rgb \
	ascii-info-037.rgb \
	ascii-info-038.rgb \
	ascii.rgb \
	console-message.rgb \
	email-message.rgb \
	email-not-example.rgb \
	email-not-example2.rgb \
	gui-new-.rgb \
	gui-new-001.rgb \
	gui-new-002.rgb \
	gui-new-003.rgb \
	gui-new-004.rgb \
	gui-new-005.rgb \
	gui-new-006.rgb \
	gui-new-007.rgb \
	gui-new-008.rgb \
	gui-new-009.rgb \
	gui-new-010.rgb \
	gui-new-011.rgb \
	gui-new-012.rgb \
	gui-new-013.rgb \
	gui-new-014.rgb \
	gui-new-015.rgb \
	gui-new-016.rgb \
	gui-new-017.rgb \
	gui-new-018.rgb \
	gui-new-019.rgb \
	gui-new-020.rgb \
	gui-new-022.rgb \
	gui-new-023.rgb \
	gui-new-024.rgb \
	gui-new-025.rgb \
	gui-new-026.rgb \
	gui-new-027.rgb \
	gui-new-028.rgb \
	gui-new-029.rgb \
	gui-new-030.rgb \
	gui-new-031.rgb \
	gui-new-033.rgb \
	gui-new-034.rgb \
	gui-new-035.rgb \
	gui-new-036.rgb \
	gui-new-037.rgb \
	gui-new-038.rgb \
	gui-new-040.rgb \
	gui-new-041.rgb \
	gui-new-042.rgb \
	gui-new-043.rgb \
	gui-new-045.rgb \
	gui-new-046.rgb \
	gui-new-047.rgb \
	gui-new-048.rgb \
	gui-new-049.rgb \
	gui-new-050.rgb \
	gui-new-051.rgb \
	gui-new-052.rgb \
	gui-new-053.rgb \
	gui-new-054.rgb \
	gui-new-055.rgb \
	gui-new-056.rgb \
	gui-new-057.rgb \
	gui-new-058.rgb \
	icon_catalog.rgb \
	newchap2-.rgb \
	newupdate.rgb \
	newupdate2.rgb \
	sing-gui-.rgb \
	sing-gui-001.rgb \
	sing-gui-002.rgb \
	sing-gui-003.rgb \
	sing-gui-004.rgb \
	sing-gui-005.rgb \
	sing-gui-006.rgb \
	sing-gui-007.rgb \
	sing-gui-008.rgb \
	sing-gui-009.rgb \
	sing-gui-010.rgb \
	sing-gui-011.rgb \
	sing-gui-012.rgb \
	sing-gui-013.rgb \
	sing-gui-015.rgb \
	sing-gui-016.rgb \
	sing-gui-017.rgb \
	sing-gui-018.rgb \
	sing-gui-019.rgb \
	sing-gui-020.rgb \
	sing-gui-021.rgb \
	sing-gui-022.rgb \
	sing-gui-023.rgb \
	sing-gui-024.rgb \
	sing-gui-025.rgb \
	sing-gui-026.rgb \
	sing-gui-027.rgb \
	sing-gui-028.rgb \
	sing-gui-029.rgb \
	sing-gui-030.rgb \
	sing-gui-031.rgb \
	sing-gui-032.rgb \
	sing-gui-033.rgb \
	sing-gui-034.rgb \
	sing-gui-035.rgb \
	sing-gui-036.rgb \
	sing-gui-037.rgb \
	sing-gui-038.rgb \
	sing-gui-039.rgb \
	sing-gui-040.rgb \
	sing-gui-041.rgb \
	sing-gui-043.rgb \
	sing-gui-044.rgb \
	sing-gui-045.rgb \
	sing-gui-046.rgb \
	sing-gui-047.rgb \
	sing-gui-048.rgb \
	sing-gui-049.rgb \
	sing-gui-050.rgb \
	sing-gui-051.rgb \
	sing-gui-052.rgb \
	sing-gui-053.rgb \
	sing-gui-054.rgb \
	sing-gui-055.rgb \
	sing-gui-056.rgb \
	sing-gui-057.rgb \
	sing-gui-058.rgb \
	sing-gui-059.rgb \
	sing-gui-060.rgb \
	sing-gui-061.rgb \
	sing-gui-062.rgb \
	sing-gui-063.rgb \
	sing-gui-064.rgb \
	sing-gui-065.rgb \
	sing-info-001.rgb \
	sing-info-002.rgb \
	sing-info-003.rgb \
	sing-info-004.rgb \
	sing-info-006.rgb \
	sing-info-007.rgb \
	sing-info-008.rgb \
	sing-info-010.rgb \
	sing-info-012.rgb \
	sing-info-014.rgb \
	sing-info-015.rgb \
	sing-info-018.rgb \
	sing-info-019.rgb \
	sing-info-020.rgb \
	sing-info-021.rgb \
	sing-info-023.rgb \
	sing-info-024.rgb \
	sing-info-025.rgb \
	sing-info-026.rgb \
	sing-info-027.rgb \
	sing-info-028.rgb \
	sing-info-029.rgb \
	sing-info-030.rgb \
	sing-info-031.rgb \
	sing-info-032.rgb \
	sing-info-033.rgb \
	sing-info-034.rgb \
	sing-info-035.rgb \
	sing-info-036.rgb \
	sing-info2-.rgb \
	sing-info2-001.rgb \
	sing-info2-002.rgb \
	sing-info2-003.rgb \
	sing-info2-004.rgb \
	starting-.rgb \
	starting-001.rgb \
	starting-002.rgb \
	starting-005.rgb \
	starting-006.rgb \
	starting-007.rgb \
	updated-.rgb \
	updated-004.rgb \
	updated-005.rgb \
	updated-006.rgb \
	updated-007.rgb \
	updated-010.rgb \
	updated-011.rgb \
	updated-012.rgb \
	updated-013.rgb \
	user_iface.rgb \
	xwindow-message.rgb \
	ascii-diagres003.rgb \
	ascii-diagres004.rgb \
	new.rgb \
	new001.rgb \
	new004.rgb \
	new005.rgb


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

PostScript = sys.group.mgmt.blk.diag.ai \
	page.send.process.ai

# Location 7: The GIF Variable
#
# List the name of each .gif figure in the orig directory that
# should be kept GIF in both the print and online directories.
#
# GIF = figure7.gif

GIF = toolchest.gif

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
