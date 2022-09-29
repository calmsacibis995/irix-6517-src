/* FDDI PCM state machine for Motorola ELM
 *
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.7 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/systm.h"

#include "ether.h"
#include "sys/fddi.h"
#include "sys/smt.h"


/* do "PC_Signal"
 */
#define moto_pc_signal(smt,n) smt->smt_ops->setvec(smt, smt->smt_pdata, n)



/* run PCM Psuedo-Code
 */
static void
motopcm_code(struct smt *smt)
{
	switch (smt->smt_st.n) {
	default:
#ifdef DEBUG
		panic("unknown FDDI PCM bit number");
#endif
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
			if (smt->smt_st.npc_type != PC_B)
				smt->smt_st.flags |= PCM_CON_U;
			break;
		case PC_B:
			smt->smt_st.t_val |= PCM_BIT(3);
			if (smt->smt_st.npc_type != PC_A)
				smt->smt_st.flags |= PCM_CON_U;
			break;
		case PC_S:
			smt->smt_st.t_val |= PCM_BIT(3);
			if (smt->smt_st.npc_type == PC_A
			    || smt->smt_st.npc_type == PC_B)
				smt->smt_st.flags |= PCM_CON_U;
			break;
		case PC_M:
			if (smt->smt_st.npc_type == PC_M) {
				smt->smt_st.flags |= PCM_CON_X;
			} else {
				smt->smt_st.t_val |= PCM_BIT(3);
			}
			break;
		}
		/* send and receive bit 3 */
		moto_pc_signal(smt,1);
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

		/* PCM_BIT(6)=0 for no MAC during LCT */

		/* send and receive bits 4,5, and 6 */
		moto_pc_signal(smt, 3);
		break;

	case 7:				/* LCT and LCT results */
		if (!PCM_SET(smt, PCM_TD_FLAG)) {
			static ulong next7[4] = {
				LC_SHORT,
				LC_MEDIUM,
				LC_LONG,
				LC_EXTENDED,
			};
			int rcv, xmt;

			xmt = (((smt->smt_st.t_val>>5) & 1)
			       + ((smt->smt_st.t_val>>(4-1)) & 2));
			rcv = (((smt->smt_st.r_val>>5) & 1)
			       + ((smt->smt_st.r_val>>(4-1)) & 2));
			if (xmt > rcv)
				rcv = xmt;
			smt->smt_ops->lct_on(smt, smt->smt_pdata);
			smt->smt_st.flags |= PCM_TD_FLAG;
			TPC_RESET(smt);
			smt->smt_st.t_next = next7[rcv];
			SMT_USEC_TIMER(smt, smt->smt_st.t_next);
			break;
		}
		smt->smt_st.flags &= ~PCM_TD_FLAG;

		if (smt->smt_ops->lct_off(smt, smt->smt_pdata)) {
			smt->smt_st.lct_fail++;
			smt->smt_st.lct_tot_fail++;
			smt->smt_st.t_val |= PCM_BIT(7);
			smt->smt_st.flags |= PCM_LEM_FAIL;
		} else {
			smt->smt_st.lct_fail = 0;
		}
		/* send the LCT results */
		moto_pc_signal(smt, 1);
		break;

	case 8:
		if (0 != (PCM_BIT(7) & smt->smt_st.t_val)
		    || 0 != (PCM_BIT(7) & smt->smt_st.r_val)) {
			/* PC_Start if LCT failed */
			smt->smt_ops->off(smt, smt->smt_pdata);
			break;
		}
		/* never a MAC for Local Loop */
		moto_pc_signal(smt, 1);
		break;

	case 9:				/* MAC on PHY output */
		if (!PCM_SET(smt, PCM_TD_FLAG)
		    && 0 != (smt->smt_st.r_val & PCM_BIT(8))) {
			smt->smt_ops->lct_on(smt, smt->smt_pdata);
			smt->smt_st.flags |= PCM_TD_FLAG;
			smt->smt_st.t_next = SMT_T_NEXT9;
			TPC_RESET(smt);
			SMT_USEC_TIMER(smt, SMT_T_NEXT9);
			break;
		}
		smt->smt_st.flags &= ~PCM_TD_FLAG;

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
		moto_pc_signal(smt, 1);
		break;

	case 10:
		/* Signal PC_Join
		 */
		SMTDEBUGPRINT(smt,0,("%s t=%x r=%x joining\n",
				     PC_LAB(smt), smt->smt_st.t_val,
				     smt->smt_st.r_val));
		smt->smt_ops->pc_join(smt,smt->smt_pdata);
		smt->smt_st.tls = T_PCM_HLS;
		SMT_LSLOG(smt,T_PCM_HLS);
		smt->smt_st.flags &= ~PCM_LEM_FAIL;
		break;
	}
}



/* Run the PCM state machine with a Motorola ELM.
 *	Only the states that are certain to be seen by the host are
 *	handled here.
 */
void
moto_pcm(struct smt *smt)
{
	long tpc;
	enum pcm_state st;
	struct smt *osmt;


	SMTDEBUGPRINT(smt,2,("%sPC%d ", PC_LAB(smt), smt->smt_st.pcm_state));

	do {
		tpc = smt->smt_ops->get_tpc(smt, smt->smt_pdata);
		st = smt->smt_st.pcm_state;

		switch (st) {
		case PCM_ST_OFF:
			smt_off(smt);
			break;


		case PCM_ST_BREAK:
			if (PCM_SET(smt, PCM_PHY_BUSY))
				break;

			if ((smt->smt_st.connects >= smt_max_connects
			     || PCM_SET(smt,PCM_DRAG))
			    && tpc < SMT_DRAG) {
				/* If we have tried to connect but failed,
				 * give it a rest.
				 */
				smt->smt_st.flags |= PCM_DRAG;
				smt_ck_timeout(smt, SMT_DRAG-tpc);

			} else {
				smt->smt_st.flags &= ~(PCM_BS_FLAG
						       | PCM_DRAG
						       | PCM_PHY_BUSY
						       | PCM_PC_START
						       | PCM_TD_FLAG
						       | PCM_NE);

				/* get ready to send bits 0,1, and 2
				 */
				smt->smt_st.connects++;
				smt->smt_st.n = 0;
				smt->smt_st.r_val = 0;
				smt->smt_st.t_val = 0;

				/* PC_Type */
				if (smt->smt_conf.pc_type == PC_M
				    || smt->smt_conf.pc_type == PC_S)
					smt->smt_st.t_val |= PCM_BIT(1);
				if (smt->smt_conf.pc_type == PC_B
				    || smt->smt_conf.pc_type == PC_M)
					smt->smt_st.t_val |= PCM_BIT(2);

				moto_pc_signal(smt, 3);
			}
			break;


		case PCM_ST_CONNECT:
			/* PS_START if we receive MLS */
			if (smt->smt_st.rls == PCM_MLS)
				smt_off(smt);
			break;


		case PCM_ST_TRACE:
			if (!PCM_SET(smt, PCM_SELF_TEST)
			    && tpc < SMT_TRACE_MAX)
				break;
			SMTDEBUGPRINT(smt,0, ("%strace finished\n",
					      PC_LAB(smt)));
			osmt = smt->smt_ops->osmt(smt,smt->smt_pdata);
			smt_fin_trace(smt,osmt);
			break;



		case PCM_ST_NEXT:
			/* PC(41b) */
			if (PCM_SET(smt,PCM_NE)) {
				smt_off(smt);
				break;
			}

			/* wait if the ELM is still signalling
			 */
			if (PCM_SET(smt, PCM_PHY_BUSY))
				break;

			/* wait if LCT is not finished */
			if (PCM_SET(smt,PCM_TD_FLAG)
			    && smt->smt_st.rls != PCM_HLS
			    && smt->smt_st.rls != PCM_MLS
			    && tpc < smt->smt_st.t_next) {
				smt_ck_timeout(smt, smt->smt_st.t_next-tpc);
				break;
			}
			motopcm_code(smt);
			break;


		case PCM_ST_SIGNAL:
		case PCM_ST_JOIN:
		case PCM_ST_VERIFY:
			/* PC(51b), PC(61b), PC(71b) */
			if (PCM_SET(smt,PCM_NE))
				smt_off(smt);
			break;


		case PCM_ST_ACTIVE:
			/* PC(81b) */
			if (PCM_SET(smt,PCM_NE)) {
				smt_off(smt);
				break;
			}
			if (!PCM_SET(smt,PCM_CF_JOIN)) {
				smt->smt_st.connects = 0;
				smt->smt_st.flags &= ~(PCM_PHY_BUSY
						       | PCM_PC_START);
				smt->smt_ops->cfm(smt, smt->smt_pdata);
			}
			break;


		case PCM_ST_MAINT:
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
						     ? T_PCM_QLS
						     : T_PCM_REP));
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
			break;
		}
	} while (st != smt->smt_st.pcm_state);

#undef PC_START
}
