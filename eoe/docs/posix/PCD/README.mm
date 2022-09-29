`\" pcd/README.mm 1.6 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.if '\*(cC'YES'  \{
.PH "'POSIX CONFORMANCE DOCUMENT''99/01/20'"
.PF "''(C) Copyright 1999 Silicon Graphics, Inc.'- % -'"
.bp
.2 1 "Overview"
This document is a template for the document required
for conformance to IEEE Std. 1003.1.
By setting certain options in the nroff/troff source files
this document can be formatted first as a fill-in-the-blanks
workbook to be used by technical staff to compile the needed data,
and can later be
modified to produce an actual POSIX conformance document.
.2 2 "Printing"
You can print this document using \fBnroff\fR or \fBtroff\fR.
The Makefile in the source directory contains targets that
should be easily adaptable by the addition of any system-specific options.
.P
This document uses the \fBtbl\fR utility and the \fBmm\fR macros as well as the
standard \fBnroff/troff\fR commands. In addition, it uses a series of macros
contained in the file \fIpcdmacros\fR.  
Some of these macros format text, whereas others divide the documentation
in categories.  Each of the categories has one or more
flags associated with it.
.P
These macros define seven categories of conformance documentation:
.AL 1 "" 1
.LI
rP -- A required paragraph.
.LI
iD -- Required because it describes an implementation-defined feature.
.LI
rF -- Required if the extended feature is present.
.LI
jC -- Required if POSIX_JOB_CONTROL is not supported.
.LI
mV -- Recommended for an "implementations may vary" feature/behavior.
.LI
uD -- Optional for an "undefined" feature/behavior.
.LI
uS -- Optional for an "unspecified" feature/behavior.
.LE
.P 
You can find the appropriate printing flags 
near the beginning of the
\fIpcdmacros\fR file. These flags come set to print all instructions and types
of conformance documentation. These flags are documented in the file and
should be self-explanatory.
.P
Modify the cN and sN strings in the 'pcdmacros' file
so that they print your company and system name.
.P
Before you print the template document you may wish
to suppress the printing of any of the following three
categories: mV (for "may vary"), uD (for "undefined"), or uS (for
"unspecified").  If your system has job control,
set the jX string to NO to suppress printing of
paragraphs that are needed only if job control is not supported.
.H 1 "Editing for the final document"
To prepare this document for the final version, you will need to edit the
template to reflect the responses from the appropriate engineers. As you
do so, remember that you will, 
in general, not have to remove the macro
commands. Rather, you control these by the use of flags in the macro file.
.P
You will, however, have to delete some text and add text at other places.
You can locate those places where the template document defines options by
identifying the appropriate .oP/.oE pair. Between these macro commands,
you should find text separated by a slash, with choices on either side.
You will need to delete one of these options and the slash as determined
by the appropriate engineer.
.P
In other cases, the template document prints out blank lines for use by
the engineers. These blank lines are controlled by the bL macro. All you
need to do is enter appropriate text after the .bL macro command. You need
not issue a .P (paragraph) command since this is included in
the category macro (.rP, .rF, etc.).
.P
You will also need to edit the blank tables, filling in the appropriate
fields. If you choose not to use a given table, but to enter, for example,
lists instead, you will need to delete the table macros and, if present,
the .rN macro command which defines space needs requests. 
.P
You may also need to delete certain sections that the engineers determine
are not relevant. This may be by category, in which case you control the
deletion of these by use of the flags provided at the head
of the \fIpcdmacros\fR
file.  In other cases, however, you will need
to delete paragraphs on a case-by-case basis.
This is particularly true if you decide not to use the
flags provided to delete all mV, uD, or uS categories.
.P 
When deleting on a case-by-case basis, delete from the macro command
(e.g., .mV) to the beginning of the next macro command that is associated
with an assertion category, except for any header (.H) commands.
\}
