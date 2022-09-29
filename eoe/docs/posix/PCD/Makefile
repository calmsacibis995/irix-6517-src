# pcd/Makefile 1.9 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.

SRCS = pcdmacros cover.mm chap02.mm chap03.mm chap04.mm \
	chap05.mm chap06.mm chap07.mm chap08.mm chap09.mm \
	chap10.mm toc.mm

pcd.ps: ${SRCS}
	tbl ${SRCS} | psroff -mm -t > pcd.ps

pcd.nr: ${SRCS}
	tbl ${SRCS} | nroff -mm -u -Tlp > pcd.nr

nroff:
	-rm pcd.nr
	make pcd.nr

psroff: pcd.ps

deledit:
	sccs deledit Makefile README ${SRCS}

archive:
	shar Makefile README.mm ${SRCS} > pcd.88.shar
