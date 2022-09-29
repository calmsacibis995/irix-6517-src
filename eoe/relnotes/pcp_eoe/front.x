.\" This file is the source file for the cover and credits pages included
.\" in printed release notes.  It is also automagically included when each
.\" chapter file is formatted (online and printed).  This makes it possible
.\" to define strings of your choice in this file and use them in chapter
.\" files.
.\"
.\" The lines below should be edited to match your product by replacing
.\" the words in double angle brackets (<<>>) with your text.
.\"
.\"	first line of Release Notes title (<= 4.25 inches long)
.ds TI "Performance Co-Pilot (EOE)
.\"	second and third lines of title (uncomment if used; TK doesn't 
.\"	appear in page footers)
.\".ds TJ <<second line of title>>
.\".ds TK <<third line of title>>
.\"	set IM to 1 if you want the default cover
.\"	page, set it to 2 if you want to include an Important box
.nr IM 1
.\"	if you use an Important box, uncomment and fill in the five lines below
.\"	(starting with ".nr TO <<4.5>>i" and ending with "..") 
.\" 	TO is the distance from the top of the paper to the top of the box and
.\"	WI is the width of the box; both are in inches and must end with i
.\".nr TO <<4.5>>i
.\".nr WI <<3.75>>i
.\".de NO
.\"<<important note text and (optional) formatting commands>>
.\"..
.\"	document part number (uncomment if document has a part number; get
.\"	part numbers from your Production Editor or Cindy Kleinfeld)
.\".ds DN <<xxx-xxxx-xxx>>
.\"	set credits page type: 1 for most release notes, 2 for system software
.\"	release notes.  When 2 is used, the "... by" strings below are ignored.
.nr CR 1
.\"	list of names after "Written by" on the credits page
.\"	(separate names by commas, use \ to escape newlines the list is long)
.ds WB "David Chatterton, \
Mark Goodwin, \
Ken McDonell, \
Ania Milewska, \
Nathan Scott, \
Tim Shimmin
.\"	name(s) after "Edited by" (names are comma-separated)
.ds EB 
.\"	name(s) after "Illustrated by" (names are comma-separated)
.ds IL 
.\"	name(s) after "Production by" (names are comma-separated)
.ds PB 
.\"	list of names after "Engineering contributions by"
.\"	(separate names by commas, use \ to escape newlines if the list is long)
.ds EG \*(WB
.\"	copyright year for the credits page (e.g. 1993)
.ds YR 1998
.\"	additional copyright line (C3 - C9 work, too)
.\".ds C2 <<year, company>>
.\"	trademark information (use as many lines as you need)
.de TR
Silicon Graphics, IRIS, and the Silicon Graphics logo are registered trademarks
and IRIX is a trademark of Silicon Graphics, Inc.
Performance Co-Pilot is a trademark of Silicon Graphics, Inc.
..
