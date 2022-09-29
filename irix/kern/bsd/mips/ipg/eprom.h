;Common definitions for the FDDIXPress, 6U FDDI board firmware
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.9 $"


;The "signature" of a 4211 running Silicon Graphics firmware
	.equ	SGI_SIGN, 0xfdd1

;flags for downloading
	.equ	EPROM_RDY, 0xfffe
	.equ	EPROM_GO, 0xffff

;we promise not to use more than this many bytes
	.equ	EPROM_CODE_SIZE, 0x800


;Shape of the "short I/O space", as the EPROM understands it
;   Notice that these are 32-bit values.
	.set	DSIO_PREV, SIO_BASE+2-SIO_INCR

 .macro	DSIO, nam
	.set	DSIO_PREV, DSIO_PREV+SIO_INCR
 .ifnes	"@nam@",""
	.equ	nam, DSIO_PREV
 .endif
 .endm

 .macro	ASIO, nam, len			;define an "array"
	DSIO	nam
	.set	DSIO_PREV, DSIO_PREV+(len-1)*SIO_INCR
 .endm

	DSIO	SIO_MAILBOX

	DSIO	SIO_SIGN
	DSIO	SIO_VERS

	ASIO	SIO_DWN_VAL,2
	DSIO	SIO_DWN_AD
