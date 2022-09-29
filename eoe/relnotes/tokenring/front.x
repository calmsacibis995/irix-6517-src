.\" This file is the source file for the cover and credits pages included
.\" in printed release notes.  It is also automagically included when each
.\" chapter file is formated (online and printed).  This makes it possible
.\" to define strings of your choice in this file and use them in chapter
.\" files.
.\"
.\" The lines below should be editted to match your product by replacing
.\" the words in angle brackets (<>) with your text.
.\" The document part number should be obtained from the Tech Pubs folks, if
.\" you do not know it.  It normally changes from release to release.
.\"
.\"	first line of Release Notes title (<= 4.25 inches long)
.ds TI 3.1 IRIS Token Ring Release Notes 
.\"	second and third lines of title (uncomment if used)
.\".ds TJ <second line of title>
.\".ds TK <third line of title>
.\"	set IM to 1 if you want the default (title and doc number only) cover
.\"	page, set it to 2 if you want to include an Important box
.nr IM <1>
.\"	if you use an Important box, uncomment and fill in five lines below
.\" 	TO is the distance between the top of the paper and the top of the box,
.\"	WI is the width of the box, both are in inches and must end with i
.\".nr TO <4.5>i
.\".nr WI <3.75>i
.\".de NO
.\"<important note text and (optional) formatting commands>
.\"..
.\"     document part number (xxx-xxxx-xxx)
.\" .ds DN 007-1710-010 
.\"	set credits page type: 1 for most release notes, 2 for system software
.\"	release notes.  When 2 is used, the "... by" strings below are ignored.
.nr CR <1>
.\"	list of names after "Written by" on the credits page
.\"	(separate names by commas, use \ to escape newlines the list is long)
.ds WB Irene Kuffel 
.\"	name(s) after "Production by" (names are comma-separated)
.\".ds PB Cynthia Kleinfeld 
.\"	list of names after "Engineering contributions by"
.\"	(separate names by commas, use \ to escape newlines if the list is long)
.ds EG Jong Kim, Irene Kuffel, and Bill Calvert 
.\"	copyright year for the credits page (e.g. 1992)
.ds YR 1994
.\"	trademark information (use as many lines as you need)
.de TR
Silicon Graphics, the Silicon Graphics logo, and IRIS 
are registered trademarks and IRIX, GIO Bus, 
IRIS Indigo,
Personal IRIS, IRIS Crimson, POWER Series, Professional Series, CHALLENGE, 
and Onyx
are trademarks
of Silicon Graphics, Inc.
VMEbus is a trademark of Motorola Corporation.
NFS is a registered trademark of Sun Microsystems, Inc.
..
