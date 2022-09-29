/* FDDI host PCM state machine
 *
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.8 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/systm.h"

#include "ether.h"
#include "sys/fddi.h"
#include "sys/smt.h"




/* do "PC_Signal" */
static void
pcm_pc_signal(struct smt *smt)
{
	smt->smt_ops->setls(smt, smt->smt_pdata,
			    (0 != (smt->smt_st.t_val
				   & PCM_BIT(smt->smt_st.n))
			     ? R_PCM_HLS
			     : R_PCM_MLS));

	smt->smt_st.flags &= ~PCM_LS_FLAG;
	smt_new_st(smt, PCM_ST_SIGNAL);
	TPC_RESET(smt);
	SMT_USEC_TIMER(smt,SMT_T_OUT);
}


/* run PCM Psuedo-Code
 */
static void
pcm_code(struct smt *smt,
	 int t)				/* 0=receive, 1=transmit */
{
	TPC_RESET(smt);

	switch (smt->smt_st.n) {
	case 0:				/* escape bit */
		pcm_pc_signal(smt);
		break;

	case 1:				/* PC_Type */
		if (smt->smt_conf.pc_type == PC_M
		    || smt->smt_conf.pc_type == PC_S)
			smt->smt_st.t_val |= PCM_BIT(1);
		pcm_pc_signal(smt);
		break;

	case 2:				/* PC_Type */
		if (smt->smt_conf.pc_type == PC_B
		    || smt->smt_conf.pc_type == PC_M)
			smt->smt_st.t_val |= PCM_BIT(2);
		pcm_pc_signal(smt);
		break;

	case 3:				/* PHY compatibility */
		if (PCM_R_BIT(smt,1)) {
			if (PCM_R_BIT(smt,2))
				smt->smt_st.npc_type = PC_M;
			else
				smt->smt_st.npc_type = PC_S;
		} else {
			if (PCM_R_BIT(smt,2))
				smt->smt_st.npc_type = PC_B;
			else
				smt->smt_st.npc_type = PC_A;
		}
		smt->smt_st.flags &= ~(PCM_CON_U | PCM_CON_X);
		switch (smt->smt_conf.pc_type) {
		case PC_A:
			smt->smt_st.t_val |= PCM_BIT(3);
			if (smt->smt_st.npc_type != PC_B) {
				smt->smt_st.flags |= PCM_CON_U;
			}
			break;
		case PC_B:
			smt->smt_st.t_val |= PCM_BIT(3);
			if (smt->smt_st.npc_type != PC_A) {
				smt->smt_st.flags |= PCM_CON_U;
			}
			break;
		case PC_S:
			smt->smt_st.t_val |= PCM_BIT(3);
			if (smt->smt_st.npc_type == PC_A
			    || smt->smt_st.npc_type == PC_B) {
				smt->smt_st.flags |= PCM_CON_U;
			}
			break;
		case PC_M:
			if (smt->smt_st.npc_type == PC_M) {
				smt->smt_st.flags |= PCM_CON_X;
			} else {
				smt->smt_st.t_val |= PCM_BIT(3);
			}
			break;
		}
		pcm_pc_signal(smt);
		break;

	case 4:				/* LCT Duration */
		if (0 == ((smt->smt_st.r_val | smt->smt_st.t_val)
			  & PCM_BIT(3)))
			smt->smt_st.flags |= PCM_WITHHOLD;
		else
			smt->smt_st.flags &= ~PCM_WITHHOLD;

		if (PCM_SET(smt, PCM_WITHHOLD)
		    || (smt->smt_conf.pc_type == PC_A
			&& (PCM_SET(smt, PCM_WA_FLAG)
			    || (PCM_SET(smt, PCM_WAT_FLAG)
				&& PC_MODE_T(smt))))) {
			smt->smt_st.t_val |= (PCM_BIT(4)|PCM_BIT(5));
		} else if (PCM_SET(smt, PCM_LEM_FAIL)) {
			smt->smt_st.t_val |= PCM_BIT(4);
		} else if (smt->smt_st.lct_tot_fail != 0) {
			smt->smt_st.t_val |= PCM_BIT(5);
		}
		pcm_pc_signal(smt);
		break;

	case 5:				/* more LCT Duration */
		pcm_pc_signal(smt);
		break;

	case 6:				/* no MAC for LCT */
		pcm_pc_signal(smt);
		break;

	case 7:				/* LCT results */
		if (!t) {
			static ulong next7[4] = {
				LC_SHORT,
				LC_MEDIUM,
				LC_LONG,
				LC_EXTENDED,
			};
			int rcv, xmt;

			smt->smt_ops->lct_on(smt, smt->smt_pdata);
			smt->smt_st.flags |= PCM_TD_FLAG;
			xmt = (((smt->smt_st.t_val>>5) & 1)
			       + ((smt->smt_st.t_val>>(4-1)) & 2));
			rcv = (((smt->smt_st.r_val>>5) & 1)
			       + ((smt->smt_st.r_val>>(4-1)) & 2));
			if (xmt > rcv)
				rcv = xmt;
			smt->smt_st.t_next = next7[rcv];
			SMT_USEC_TIMER(smt, next7[rcv]);
		} else {
			if (smt->smt_ops->lct_off(smt, smt->smt_pdata)) {
				smt->smt_st.lct_fail++;
				smt->smt_st.lct_tot_fail++;
				smt->smt_st.t_val |= PCM_BIT(7);
				smt->smt_st.flags |= PCM_LEM_FAIL;
			} else {
				smt->smt_st.lct_fail = 0;
			}
			pcm_pc_signal(smt);
		}
		break;

	case 8:
		if (0 != (PCM_BIT(7) & smt->smt_st.t_val)
		    || 0 != (PCM_BIT(7) & smt->smt_st.r_val)) {
			/* PC_Start if LCT failed */
			smt->smt_ops->off(smt, smt->smt_pdata);
			break;
		}
		pcm_pc_signal(smt);
		break;			/* never a MAC for Local Loop */

	case 9:				/* MAC on PHY output */
		if (!t && 0 != (smt->smt_st.r_val & PCM_BIT(8))) {
			smt->smt_ops->lct_on(smt, smt->smt_pdata);
			smt->smt_st.flags |= PCM_TD_FLAG;
			smt->smt_st.t_next = SMT_T_NEXT9;
			SMT_USEC_TIMER(smt, SMT_T_NEXT9);
			break;
		}
		if (PCM_SET(smt, PCM_WITHHOLD)
		    || (smt->smt_conf.pc_type == PC_A
			&& (PCM_SET(smt, PCM_WA_FLAG)
			    || (PCM_SET(smt, PCM_WAT_FLAG)
				&& PC_MODE_T(smt))))) {
			SMTDEBUGPRINT(smt,0,("%s t=%x r=%x withholding\n",
					     PC_LAB(smt),
					     smt->smt_st.t_val,
					     smt->smt_st.r_val));
			smt->smt_ops->off(smt, smt->smt_pdata);
			smt->smt_ops->alarm(smt, smt->smt_pdata,
					    SMT_ALARM_CON_W);
			break;
		}

		/* If we are dual-MAC, we will put a MAC on this port.
		 * If we are talking to a concentrator, we will have a MAC.
		 * Otherwise, the single MAC will be on the B port
		 */
		if (smt->smt_conf.type != SM_DAS
		    || smt->smt_conf.pc_type != PC_A
		    || PC_MODE_T(smt)) {
			smt->smt_st.t_val |= PCM_BIT(9);
		}
		pcm_pc_signal(smt);
		break;

	case 10:
		/* Signal PC_Join */
		SMTDEBUGPRINT(smt,0,("%s t=%x r=%x joining\n",
				     PC_LAB(smt), smt->smt_st.t_val,
				     smt->smt_st.r_val));
		smt->smt_ops->setls(smt, smt->smt_pdata, R_PCM_HLS);
		smt->smt_st.flags &= ~PCM_LS_FLAG;
		smt_new_st(smt, PCM_ST_JOIN);
		SMT_USEC_TIMER(smt,SMT_T_OUT);
		break;
	}
}



#define BREAK_REQ(tpc,smt) ((smt)->smt_st.rls == PCM_QLS \
			    || (!PCM_SET(smt,PCM_LS_FLAG) \
				&& tpc > SMT_T_OUT) \
			    || PCM_SET(smt, PCM_NE))

/* run the PCM state machine
 */
void
host_pcm(struct smt *smt)
{
#	define PC_START() {smt_off(smt); goto again;}
	long tpc;
	struct smt *osmt;

	if (PCM_NONE == smt->smt_conf.pcm_tgt) {
		smt_off(smt);
		smt->smt_ops->cfm(smt,smt->smt_pdata);
		return;
	}

again:;
	SMTDEBUGPRINT(smt,2,("%sPC%d ", PC_LAB(smt), smt->smt_st.pcm_state));
	tpc = smt->smt_ops->get_tpc(smt, smt->smt_pdata);

	switch (smt->smt_st.pcm_state) {
	case PCM_ST_OFF:
		smt_off(smt);
		if (smt->smt_st.pcm_state != PCM_ST_OFF)
			goto again;
		break;


	case PCM_ST_BREAK:
		/* PC(13) */
		if (tpc >= SMT_TB_MIN
		    && (smt->smt_st.rls == PCM_HLS
			|| smt->smt_st.rls == PCM_QLS)) {
			/* If we have tried to connect but failed,
			 * give it a rest.
			 */
			if ((smt->smt_st.connects >= smt_max_connects
			     || PCM_SET(smt,PCM_DRAG))
			    && tpc < SMT_DRAG) {
				smt->smt_st.flags |= PCM_DRAG;
				smt_ck_timeout(smt, SMT_DRAG-tpc);
				break;
			}

			smt->smt_st.connects++;
			smt->smt_ops->setls(smt, smt->smt_pdata, R_PCM_HLS);
			smt->smt_st.flags &= ~(PCM_BS_FLAG
					       | PCM_LS_FLAG
					       | PCM_DRAG
					       | PCM_PHY_BUSY
					       | PCM_PC_START);
			smt_new_st(smt, PCM_ST_CONNECT);
			TPC_RESET(smt);
			goto again;
		}

		/* PC(11) */
		if (!PCM_SET(smt,PCM_BS_FLAG)) {
			if (tpc >= SMT_TB_MAX) {
				smt->smt_st.flags |= PCM_BS_FLAG;
				smt->smt_ops->alarm(smt, smt->smt_pdata,
						    SMT_ALARM_BS);
			} else {
				smt_ck_timeout(smt, SMT_TB_MAX);
			}
		}
		break;


	case PCM_ST_TRACE:
		if (smt->smt_st.rls != PCM_QLS
		    && smt->smt_st.rls != PCM_HLS
		    && tpc < SMT_TRACE_MAX)
			break;
		SMTDEBUGPRINT(smt,0, ("%strace finished\n", PC_LAB(smt)));
		osmt = smt->smt_ops->osmt(smt,smt->smt_pdata);
		smt_fin_trace(smt, osmt);
		goto again;


	case PCM_ST_CONNECT:
		if (!PCM_SET(smt,PCM_LS_FLAG)) {
			/* PC(33) */
			if (smt->smt_st.rls == PCM_HLS) {
				smt->smt_st.flags |= PCM_LS_FLAG;
				TPC_RESET(smt);
				SMT_USEC_TIMER(smt,SMT_C_MIN);
				break;
			}

			/* PC(31) */
			if (smt->smt_st.rls == PCM_ILS
			    || smt->smt_st.rls == PCM_MLS)
				PC_START();

		} else {
			if (smt->smt_st.rls == PCM_MLS
			    || smt->smt_st.rls == PCM_QLS) {
				PC_START();
			}

			/* PC(34) */
			if (tpc >= SMT_C_MIN) {
				smt->smt_ops->setls(smt,
						    smt->smt_pdata,
						    R_PCM_ILS);
				smt->smt_st.flags &= ~(PCM_LS_FLAG
							 | PCM_RC_FLAG
							 | PCM_TC_FLAG
							 | PCM_TD_FLAG
							 | PCM_NE);
				smt->smt_st.r_val = 0;
				smt->smt_st.t_val = 0;
				smt->smt_st.n = 0;
				smt_new_st(smt, PCM_ST_NEXT);
				TPC_RESET(smt);
				SMT_USEC_TIMER(smt,SMT_T_OUT);
				goto again;
			}
			smt_ck_timeout(smt, SMT_C_MIN-tpc);
		}
		break;


	case PCM_ST_NEXT:
		/* PC(44a) */
		if (!PCM_SET(smt, PCM_LS_FLAG)
		    && smt->smt_st.rls == PCM_ILS) {
			smt->smt_st.flags |= PCM_LS_FLAG;
			TPC_RESET(smt);
			tpc = 0;
		}

		/* PC(41b) */
		if (smt->smt_st.rls == PCM_QLS
		    || PCM_SET(smt, PCM_NE))
			PC_START();
		if (!PCM_SET(smt,PCM_LS_FLAG)
		    && smt->smt_st.n == 0) {
			if (tpc > SMT_T_OUT) {
				PC_START();
			} else {
				smt_ck_timeout(smt, SMT_T_OUT-tpc);
			}
		}

		/* PC(44b) */
		if (PCM_SET(smt, PCM_LS_FLAG)
		    && !PCM_SET(smt, PCM_RC_FLAG)) {
			if (tpc < SMT_TL_MIN) {
				SMT_USEC_TIMER(smt,SMT_TL_MIN);
				break;
			}
			smt->smt_st.flags |= PCM_RC_FLAG;
			pcm_code(smt,0);
			goto again;
		}

		/* PC(44d) */
		if (PCM_TD_FLAG == (smt->smt_st.flags
				    & (PCM_TD_FLAG | PCM_TC_FLAG))) {
			if (smt->smt_st.rls == PCM_HLS
			    || smt->smt_st.rls == PCM_MLS
			    || tpc >= smt->smt_st.t_next) {
				smt->smt_st.t_next = 0;
				smt->smt_st.flags |= PCM_TC_FLAG;
				pcm_code(smt,1);
				goto again;
			} else {
				smt_ck_timeout(smt, smt->smt_st.t_next-tpc);
			}
		}
		break;


	case PCM_ST_SIGNAL:
		/* PC(51) */
		if (BREAK_REQ(tpc,smt)) {
			PC_START();
		}

		/* PC(55a) PC(55b) */
		if (!PCM_SET(smt,PCM_LS_FLAG)
		    && (smt->smt_st.rls == PCM_HLS
			|| smt->smt_st.rls == PCM_MLS)) {
			smt->smt_st.flags |= PCM_LS_FLAG;
			if (smt->smt_st.rls == PCM_HLS)
				smt->smt_st.r_val|=PCM_BIT(smt->smt_st.n);
			TPC_RESET(smt);
			tpc = 0;
		}


		/* PC(54) */
		if (PCM_SET(smt,PCM_LS_FLAG)) {
			if (tpc < SMT_TL_MIN) {
				SMT_USEC_TIMER(smt,SMT_TL_MIN);
				break;
			}
			smt->smt_ops->setls(smt, smt->smt_pdata, R_PCM_ILS);
			smt->smt_st.flags &= ~(PCM_LS_FLAG
						 | PCM_RC_FLAG
						 | PCM_TC_FLAG
						 | PCM_TD_FLAG);
			smt->smt_st.n++;
			smt_new_st(smt, PCM_ST_NEXT);
			TPC_RESET(smt);
			SMT_USEC_TIMER(smt,SMT_T_OUT);
			goto again;
		}
		smt_ck_timeout(smt, SMT_T_OUT-tpc);
		break;


	case PCM_ST_JOIN:
		/* PC(66) */
		if (!PCM_SET(smt, PCM_LS_FLAG)
		    && smt->smt_st.rls == PCM_HLS) {
			smt->smt_st.flags |= PCM_LS_FLAG;
			TPC_RESET(smt);
			tpc = 0;
		}

		/* PC(61) */
		if (BREAK_REQ(tpc,smt)
		    || (smt->smt_st.rls == PCM_ILS
			&& PCM_SET(smt, PCM_LS_FLAG))) {
			PC_START();
		}

		/* PC(67) */
		if (PCM_SET(smt, PCM_LS_FLAG)) {
			if (tpc < SMT_TL_MIN) {
				SMT_USEC_TIMER(smt,SMT_TL_MIN);
				break;
			}
			smt->smt_ops->setls(smt, smt->smt_pdata, R_PCM_MLS);
			smt->smt_st.flags &= ~PCM_LS_FLAG;
			smt_new_st(smt, PCM_ST_VERIFY);
			TPC_RESET(smt);
			SMT_USEC_TIMER(smt,SMT_T_OUT);
			goto again;
		}
		smt_ck_timeout(smt, SMT_T_OUT-tpc);
		break;


	case PCM_ST_VERIFY:
		/* PC(77) */
		if (smt->smt_st.rls == PCM_MLS
		    && !PCM_SET(smt, PCM_LS_FLAG)) {
			smt->smt_st.flags |= PCM_LS_FLAG;
			TPC_RESET(smt);
			tpc = 0;
		}

		/* PC(71) */
		if (BREAK_REQ(tpc,smt)
		    || (smt->smt_st.rls == PCM_ILS
			&& !PCM_SET(smt,PCM_LS_FLAG))) {
			PC_START();
		}

		/* PC(78) */
		if (PCM_SET(smt,PCM_LS_FLAG)) {
			if (tpc < SMT_TL_MIN) {
				SMT_USEC_TIMER(smt,SMT_TL_MIN);
				break;
			}
			smt->smt_ops->setls(smt, smt->smt_pdata, R_PCM_ILS);
			smt->smt_st.flags &= ~(PCM_LS_FLAG | PCM_LEM_FAIL);
			smt_new_st(smt, PCM_ST_ACTIVE);
			TPC_RESET(smt);
			SMT_USEC_TIMER(smt,SMT_T_OUT);
			if (PCM_SET(smt,PCM_CON_U)) {
				smt->smt_ops->alarm(smt, smt->smt_pdata,
						    SMT_ALARM_CON_U);
			} else if (PCM_SET(smt,PCM_CON_X)) {
				smt->smt_ops->alarm(smt, smt->smt_pdata,
						    SMT_ALARM_CON_X);
			}
			goto again;
		}
		smt_ck_timeout(smt, SMT_T_OUT-tpc);
		break;


	case PCM_ST_ACTIVE:
		/* PC(88a) */
		if (!PCM_SET(smt, PCM_LS_FLAG)
		    && smt->smt_st.rls == PCM_ILS) {
			smt->smt_st.flags |= PCM_LS_FLAG;
			TPC_RESET(smt);
			tpc = 0;
		}

		/* PC(81) */
		if (!PCM_SET(smt, PCM_TRACE_OTR)
		    && (BREAK_REQ(tpc,smt)
			|| smt->smt_st.rls == PCM_HLS)) {
			PC_START();
		}

		if (!PCM_SET(smt,PCM_LS_FLAG)) {
			smt_ck_timeout(smt, SMT_T_OUT-tpc);
		} else {
			/* PC(88b) */
			if (!PCM_SET(smt,PCM_CF_JOIN)) {
				if (tpc < SMT_TL_MIN) {
					SMT_USEC_TIMER(smt,SMT_TL_MIN);
					break;
				}
				smt->smt_st.connects = 0;
				smt->smt_st.flags &= ~(PCM_PHY_BUSY
						       | PCM_PC_START);
				smt->smt_ops->cfm(smt,smt->smt_pdata);
			}

			/* PC(88c) and PC(82) */
			if (smt->smt_st.rls == PCM_MLS
			    && !PCM_SET(smt, PCM_TRACE_OTR)) {
				/* If we are the MAC that is supposed to
				 * respond to PC_Trace, then do so.
				 * Otherwise, propagate it.
				 *
				 * If we are are single-MAC, dual-attached,
				 * "THRU," on the primary ring, then we are
				 * not the PHY that is being "traced," and
				 * should ask the driver to propagate
				 * the TRACE.
				 * Otherwise, we are the target and should
				 * respond with QLS.
				 *
				 * When the destination MAC responds with
				 * QLS, we will recycle.
				 *
				 * Delay the restart to allow the ring to
				 * do something useful in case we are the
				 * cause, and instead of a real path test.
				 */

				osmt = smt->smt_ops->osmt(smt,smt->smt_pdata);
				if (smt->smt_conf.type == SM_DAS
				    && smt->smt_conf.pc_type == PC_A
				    && PCM_SET(smt,PCM_THRU_FLAG)) {
					SMTDEBUGPRINT(smt,0,
						      ("%sMLS--trace prop\n",
						       PC_LAB(smt)));
					smt->smt_st.flags |= PCM_TRACE_OTR;

					TPC_RESET(osmt);
					SMT_USEC_TIMER(osmt, SMT_TRACE_MAX);
					smt_new_st(osmt, PCM_ST_TRACE);
					osmt->smt_st.flags &= ~PCM_PC_DISABLE;
					osmt->smt_ops->setls(osmt,
							     osmt->smt_pdata,
							     R_PCM_MLS);
				} else {
					SMTDEBUGPRINT(smt,0,
						      ("%sMLS--traced\n",
						       PC_LAB(smt)));
					smt_fin_trace(smt,osmt);
					goto again;
				}
			}
		}
		break;


	case PCM_ST_MAINT:
		if (!PCM_SET(smt, PCM_PC_DISABLE))
			smt_off(smt);
		break;


	case PCM_ST_BYPASS:
		if (tpc < SMT_I_MAX) {
			smt_ck_timeout(smt,SMT_I_MAX-tpc);
			break;
		}
		if (!PCM_SET(smt,PCM_INS_BYPASS))
			smt->smt_ops->setls(smt,smt->smt_pdata,
					    (smt->smt_conf.type == SAS
					     ? T_PCM_QLS : T_PCM_REP));
		if (PCM_SET(smt, PCM_SELF_TEST)) {
			if (tpc < SMT_TEST_DELAY) {
				smt_ck_timeout(smt,SMT_TEST_DELAY-tpc);
				break;
			}
			smt->smt_st.flags &= ~PCM_SELF_TEST;
		}
		osmt = smt->smt_ops->osmt(smt,smt->smt_pdata);
		if (PCM_SET(osmt,PCM_SELF_TEST)) {
			smt_ck_timeout(smt,SMT_TEST_DELAY-tpc);
			break;
		}
		smt_new_st(smt, PCM_ST_OFF);
		goto again;
	}
#undef PC_START
}
