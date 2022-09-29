.\" Common troff macros for PCP release notes
.\" $Id: common.nr,v 1.3 1999/07/01 22:28:06 kenmcd Exp $
.\"
.\" Start example
.deEx
.in+4n
.ftCW
.nf
..
.\"
.\" End example
.deEe
.fi
.ftR
.in
..
.\"
.\" TODO note
.deTO
.br
\l'6i'
.sp0.5v
.in+0.5i
.ti-0.5i
.ta0.5i
\fBTODO\fR\t\\$2
.br
.in-0.5i
\l'6i'
.sp0.5v
..
.\"
.\" Some customization is used in pcp_eoe relnotes, as this product
.\" is built in IRIX for IRIX 6.5 and in PCP for other IRIX versions
.\" Set number register PC to 1 if part of PCP product, 0 for part of
.\" IRIX product.
.nrPC0
.\"
.\" Current PCP version
.dsVn"2.1
.dsBn"
