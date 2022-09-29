#ident	"$Revision: 1.3 $"

#
# Makefile for a manual page source file directory.
# See $(ROOT)/usr/lib/doc/doc/make_targets for a list of available make
# targets.
#

#	IDB_PATH is the directory that (formatted) manual pages in this 
#	directory are installed in.
IDB_PATH=/usr/catman/u_man/cat1

include $(ROOT)/usr/include/make/mandefs
include $(ROOT)/usr/include/make/manrules

#	To build language-specific versions of manual pages (source must use
#	the .Op macro), uncomment LANGOPT and replace null with a language
#	(a, c, f, or p).  When building manual pages from Atria, uncomment 
#	LANGOPT and replace null with atria.
#LANGOPT = null

#	To specify a string to be included in each manual page header, set
#	OPTHEADER to the string (no quoting required).  Normally, this
#	variable is not set.
#OPTHEADER = 

#	To specify a string to be included in each manual page footer, set
#	OPTFOOTER to the string (no quoting required).  Normally, this
#	variable is not set.
#OPTFOOTER =

#	To build printed manual pages that have headers and footers formatted
#	for odd and even pages, set ODDEVEN to -rt2.  For generic headers
#	and footers, don't set ODDEVEN at all, or set it to -rt1.
#ODDEVEN = -rt1

#	To build a printed manual page with a starting page number other 
#	than 1, set PAGENUM to -nN where N is the starting page number.
#PAGENUM = -n1


all:	ktools default

ktools: ktools.z
	pod2man ktools.pod | pack > ktools.z
