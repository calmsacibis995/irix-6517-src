'\" pcd/chap08.mm 1.5 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 8 "Language-Specific Services for the C Programming Language" \
"Language Services for C"
.rF
\*(sN
conforms to C Language Binding (Common Usage C Language-Dependent System
Support).  
The semantics of the functions covered by this
.pN
section are the same as those specified in ANSI-C.
.2 1 "Referenced C Language Routines"
.3 1 "Extensions to Time Functions"
.iD
For the 
.f setlocale
function,
the default values for the required categories and those categories 
specific to 
\*(sN
are defined below:
.rN 10
.TS
tab(|), allbox;
cw(2i) cw(2i)
l l .
Category|Default Value
LC_CTYPE|C
LC_COLLATE|C
LC_TIME|C
LC_NUMERIC|C
LC_MONETARY|C
.TE
.iD
If the environment variable corresponding to a specific category is not
set or is set to the empty string, and unless the \fBLANG\fP environment
variable is set and valid, 
.f setlocale
defaults to the "C" locale .
.iD
If the \fBLANG\fP environment variable is not set or is set to the empty
string, 
\fIsetlocale(category,"")\fP
defaults to the "C" locale .
.2 2 "FILE-Type C Language Functions"
.3 1 "Map a Stream Pointer to a File Descriptor"  
.4 4 "Errors"
.iD
On
\*(sN
systems, the
.f fileno
function does not an explicitly detect any errors.
However, if the
.a stream
argument does not point to a valid file stream, the return will be
meaningless and the calling process may experience a segmentation violation.
.90 8.2.1 "Was impl-def; now is unspecified."
.3 2 "Open a Stream on a File Descriptor"
.4 2 "Description"
.iD
On
\*(sN
systems, the
.a type
argument to the
.f fdopen
function contains no additional values.
.4 4 "Errors"
.uS
On 
\*(sN
systems, the
.f fdopen
function detects
no errors; however, if the
.a fildes
argument is not a valid file descriptor, errors will occur if any attempt
is made to access the resulting FILE.
.3 3 "Interactions of Other FILE-Type C Functions"
.uD
On
\*(sN
systems, if the active handle ceases to be accessible before the requirements
on the first handle have been met, the file remains accessible via other
handles.  However, the effective file position in other handles is
undefined.
.uD
On
\*(sN
systems, for function calls involving two or more file handles,
when the actions are not co-ordinated as described in section 
8.2.3 of
.pN ,
processes attempting to access file data sequentially may encounter 
unpredicatable discontinuities in the order of the data.
.90 8.3.2.10 "If the stream is opened in append mode or if the O_APPEND \
flag is set as a consequence of dealing with other handles on the file, \
the result of ftell on that stream is:"
.2 3 "Other C Language Functions"
.3 2 "Set Time Zone"
.4 2 "Description"
.iD
On
\*(sN
systems,
if the environment variable \fBTZ\fP is absent, time conversion routines
default to GMT.
.nP
.dM
