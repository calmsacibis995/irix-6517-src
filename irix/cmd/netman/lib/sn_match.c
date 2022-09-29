/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snooper expression evaluation functions.
 */
#include "protocol.h"
#include "snooper.h"

/*
 * Return 1 if sp matches sn's expression filter, 0 otherwise.
 * If sn_expr is null the filter is promiscuous.
 */
int
sn_match(Snooper *sn, SnoopPacket *sp, int len)
{
	if (sn->sn_expr == 0)
		return 1;
	return SN_TEST(sn, sn->sn_expr, sp, len);
}

/*
 * Functional form of SN_TEST.
 */
int
sn_test(Snooper *sn, struct expr *ex, SnoopPacket *sp, int len)
{
	return SN_TEST(sn, ex, sp, len);
}

/*
 * Functional form of SN_EVAL.
 */
int
sn_eval(Snooper *sn, struct expr *ex, SnoopPacket *sp, int len,
	struct exprerror *err, struct expr *rex)
{
	return SN_EVAL(sn, ex, sp, len, err, rex);
}
