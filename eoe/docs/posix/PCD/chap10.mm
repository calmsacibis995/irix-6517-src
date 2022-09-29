'\" pcd/chap10.mm 1.6 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 10 "Data Interchange Format"
.2 1 "Archive/Interchange Format"
.iD
On
\*(sN
systems, the \fIformat-reading\fP and \fIformat-creating utilities\fP 
are named tar and cpio.  See the man pages tar(1) and cpio(1) for information
about these utilities.
.rC "(Give a specific citation to the manual page.)"
.90 10.1 "Frames (blocks) on the media are not specified."
.3 1 "Extended \fItar\fP Format"
.mV
In
\*(sN
the only explicitly illegal character values in file name components
represented in tar archive headers are the slash (/), which is the pathname 
component separator; and the null character, which is the string delimiter. 

Any other 8 bit character value may in principle be part of a filename.

However, pathnames containing containing
non-printing characters, spaces or tabs, or starting with the minus sign 
character will give rise to files which
are difficult or impossible to manipulate with the normal
\*(sN
utilities.  For portability, restricting pathnames to printable ASCII 
characters is strongly recommended.
.iD
If a file name is found on the medium that would create an invalid file
name on the system, the data from the file 
is not stored on the file hierarchy. 
The utility ignores such files and prints an error message indicating
that the file is being ignored.
.mV
If the following mode bits that are not
mentioned elsewhere in
.pN
exist in the archive
\*(sN 
will behave as indicated below:
.P
If the TSVTX bit is set, this bit will be set in the mode of the extracted
file.
.P
Any other bits apart from those mentioned in
.pN
are ignored.
.uS
If the
.a typeflag
field is set to CHRTYPE, BLKTYPE, or FIFOTYPE, the
.a size
field is ignored.
.uS
On
\*(sN
systems, for character special and block special files 
(represented by ASCII digits '3' and '4'), the
.a devmajor
and
.a devminor
fields each contain digits which are a representation, in octal base,
of an eight bit number (from 0 to 255).  These numbers are the device
major and minor number respectively.
.3 2 "Extended \fIcpio\fP Format"
.4 1 "Header"
.uS
On
\*(sN
systems, the values of
.a c_dev
contains the device number identifying the device containing a directory
entry for the file, and
.a c_ino
contains the unique inode number identifying the file on that device.
.iD
For character or block special files, 
.a c_rdev
contains a number representing the device id of the file, as understood by 
\*(sN.
In the current implementation this is a (16 bit) signed short; the more 
significant byte is the device major number, and the less significant byte
is the device minor number.
.P
Note that IRIX's cpio has the -K option which can represent the full 
32 bit dev_t.
.4 2 "File Name"
.iD
On
\*(sN
systems, 
if a file name is found on the medium that would create an invalid file
name, the data from the file is not stored on the file hierarchy, and
an error message will be printed warning that the file is ignored.
.4 4 "Special Entries"
.uS
For special files other than FIFO special files, 
directories, and the trailer, 
.a c_filesize
has no significance and is ignored.
.4 5 "\fIcpio\fP Values"
.mV
On 
\*(sN
systems, other than those file types defined in Table 10-3 of
.pN , 
no further file types are defined for cpio archives:
.3 3 "Multiple Volumes"
.iD
On
\*(sN
systems, 
the \fItar\fP and \fIcpio\fP utilities support
multi-volume archives only on actual tape devices, and not on regular
files.
At the end of each partial tape of a multi-volume set, the utilities prompt the
user to insert a new tape.
.P
cpio prompts for a new device name, thus allowing work to continue on a
different device.
.P
tar assumes that the device name has not changed, so succeeding tapes must be
inserted in the same tape derive as the first tape of the archive.
.nP
.dM
