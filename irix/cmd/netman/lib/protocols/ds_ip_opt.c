/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Encode or decode an IP option, not including any trailing bytes counted
 * in ipo->ipo_len but not decoded into struct ip_route or ip_timestamp.
 */
#include "datastream.h"
#include "protocols/ip.h"

int
ds_ip_opt(DataStream *ds, struct ip_opt *ipo)
{
	if (!ds_u_char(ds, &ipo->ipo_opt)) {
		if (ds->ds_direction == DS_DECODE)
			ipo->ipo_len = 0;
		return 0;
	}
	if (ds->ds_direction == DS_DECODE)
		ipo->ipo_len = 1;

	switch (ipo->ipo_opt) {
	  case IPOPT_EOL:
	  case IPOPT_NOP:
		break;

	  case IPOPT_RR:
	  case IPOPT_LSRR:
	  case IPOPT_SSRR: {
		struct ip_route *ipr;
		int len;
		struct in_addr *hop;

		ipr = (struct ip_route *) ipo;
		if (!ds_u_char(ds, &ipr->ipr_len)
		    || !ds_u_char(ds, &ipr->ipr_off)) {
			return 0;
		}
		len = ipr->ipr_len;
		if (len > IP_MAXOPTLEN)
			len = IP_MAXOPTLEN;
		len -= IPOPT_MINOFF - 1;
		len /= sizeof *hop;
		ipr->ipr_cnt = len;
		for (hop = ipr->ipr_addr; --len >= 0; hop++)
			if (!ds_in_addr(ds, hop))
				return 0;
		break;
	  }

	  case IPOPT_TS: {
		struct ip_timestamp *ipt;
		long bits;
		int len;

		ipt = (struct ip_timestamp *) ipo;
		if (!ds_u_char(ds, &ipt->ipt_len)
		    || !ds_u_char(ds, &ipt->ipt_ptr)) {
			return 0;
		}
		if (!ds_bits(ds, &bits, 4, DS_ZERO_EXTEND))
			return 0;
		ipt->ipt_oflw = bits;
		if (!ds_bits(ds, &bits, 4, DS_ZERO_EXTEND))
			return 0;
		ipt->ipt_flg = bits;
		len = ipt->ipt_len;
		if (len > IP_MAXOPTLEN)
			len = IP_MAXOPTLEN;
		len -= IPOPT_MINOFF;
		switch (ipt->ipt_flg) {
		  case IPOPT_TS_TSONLY: {
			n_long *tp;

			len /= sizeof *tp;
			for (tp = ipt->ipt_timestamp.ipt_time; --len >= 0; tp++)
				if (!ds_n_long(ds, tp))
					return 0;
			break;
		  }
		  case IPOPT_TS_TSANDADDR:
		  case IPOPT_TS_PRESPEC: {
			struct ipt_ta *ta;

			len /= sizeof *ta;
			for (ta = ipt->ipt_timestamp.ipt_ta; --len >= 0; ta++)
				if (!ds_in_addr(ds, &ta->ipt_addr)
				    || !ds_n_long(ds, &ta->ipt_time)) {
					return 0;
				}
		  }
		}
		break;
	  }
	}
	return 1;
}
