/*
 * @(#)xdr_custom.h	1.1 88/06/08 4.0NFSSRC; from 1.6 88/03/01 D/NFS
 *
 * @(#)xdr_custom.h 1.7 88/02/08 Copyr 1987 Sun Micro
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 *
 */

struct futureslop { char dummy; };
extern bool_t xdr_futureslop();

struct nomedia { char dummy; };
extern bool_t xdr_nomedia();
