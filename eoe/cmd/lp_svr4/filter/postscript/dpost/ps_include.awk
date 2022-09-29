#
# |-----------------------------------------------------------|
# | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
# | All Rights Reserved                                       |
# |-----------------------------------------------------------|
# |          Restricted Rights Legend                         |
# | Use, duplication, or disclosure by the Government is      |
# | subject to restrictions as set forth in                   |
# | subparagraph (c)(1)(ii) of the Rights in Technical        |
# | Data and Computer Software Clause of DFARS 252.227-7013.  |
# |         MIPS Computer Systems, Inc.                       |
# |         950 DeGuigne Avenue                               |
# |         Sunnyvale, California 94088-3650, USA             |
# |-----------------------------------------------------------|
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/dpost/RCS/ps_include.awk,v 1.1 1992/12/14 13:14:56 suresh Exp $
/^->/ {
	if(ndef)
		printf("\t0\n};\n\n")
	printf("static char *%s[] = {\n", $2)
	ndef++
	next
}
/^#/ {next}
$0 != "" {printf("\t\"%s\",\n", $0); next}
END {printf("\t0\n};\n")}
