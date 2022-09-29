/*
** Test
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

TST_BEGIN(schedAttr)
{
	pthread_attr_t		pta;
	struct sched_param	sp;
	int			policy;

#define Tpolicy(test_policy)						\
	TstTrace("Policy " #test_policy "\n");				\
	ChkInt( pthread_attr_init(&pta), == 0 );			\
	ChkInt( pthread_attr_setschedpolicy(&pta, test_policy), == 0 ); \
	policy = -1;							\
	ChkInt( pthread_attr_getschedpolicy(&pta, &policy), == 0 );	\
	ChkExp( policy == test_policy,					\
		("Could not set policy %d [%d]\n", policy, test_policy) );\
	Tpri(test_policy, sched_get_priority_min(test_policy));		\
	Tpri(test_policy, sched_get_priority_max(test_policy));	

#define Tpri(test_policy, pri)						\
	TstTrace("\tPriority %d\n", pri);				\
	sp.sched_priority = pri;					\
	ChkInt( pthread_attr_setschedparam(&pta, &sp), == 0 );	\
	sp.sched_priority = -1;						\
	ChkInt( pthread_attr_getschedparam(&pta, &sp), == 0 );	\
	ChkExp( sp.sched_priority == pri,				\
		("Could not set priority for policy %d %d [%d]\n",	\
		 test_policy, sp.sched_priority, pri) );

	Tpolicy(SCHED_OTHER);
	Tpolicy(SCHED_RR);
	Tpolicy(SCHED_FIFO);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
check_explicit_attr(void *junk)
{
	struct sched_param	sp;
	int			pol;
	int			pri = sched_get_priority_max(SCHED_FIFO);

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &sp), == 0 );
	ChkExp( pol == SCHED_FIFO, ("Policy wrong %d [%d]\n", pol, SCHED_FIFO));
	ChkExp( sp.sched_priority == pri,
		("Priority wrong %d [%d]\n", sp.sched_priority, pri) );

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(schedAttrCreate)
{
	pthread_attr_t		pta;
	struct sched_param	sp;
	pthread_t		pt;
	void			*ret;
	void			*check_explicit_attr(void *);

	sp.sched_priority = sched_get_priority_min(SCHED_RR);
	ChkInt( pthread_setschedparam(pthread_self(), SCHED_RR, &sp), == 0 );

	ChkInt( pthread_attr_init(&pta), == 0 );
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ChkInt( pthread_attr_setschedpolicy(&pta, SCHED_FIFO), == 0 );
	ChkInt( pthread_attr_setschedparam(&pta, &sp), == 0 );

	ChkInt( pthread_attr_setinheritsched(&pta, PTHREAD_EXPLICIT_SCHED),
		== 0 );

	ChkInt( pthread_create(&pt, &pta, check_explicit_attr, 0), == 0 );
	ChkInt( pthread_join(pt, &ret), == 0 );
	ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );

	TST_RETURN(0);
}


/* ------------------------------------------------------------------ */

void *
check_inherit_attr(void *junk)
{
	struct sched_param	sp;
	int			pol;
	int			pri = sched_get_priority_min(SCHED_RR);

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &sp), == 0 );
	ChkExp( pol == SCHED_RR, ("Policy wrong %d [%d]\n", pol, SCHED_RR) );
	ChkExp( sp.sched_priority == pri,
		("Priority wrong %d [%d]\n", sp.sched_priority, pri) );

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(schedAttrInherit)
{
	pthread_attr_t		pta;
	struct sched_param	sp;
	pthread_t		pt;
	void			*ret;
	void			*check_inherit_attr(void *);

	sp.sched_priority = sched_get_priority_min(SCHED_RR);
	ChkInt( pthread_setschedparam(pthread_self(), SCHED_RR, &sp), == 0 );

	ChkInt( pthread_attr_init(&pta), == 0 );
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ChkInt( pthread_attr_setschedpolicy(&pta, SCHED_FIFO), == 0 );
	ChkInt( pthread_attr_setschedparam(&pta, &sp), == 0 );

	ChkInt( pthread_attr_setinheritsched(&pta, PTHREAD_INHERIT_SCHED),
		== 0 );

	ChkInt( pthread_create(&pt, &pta, check_inherit_attr, 0), == 0 );
	ChkInt( pthread_join(pt, &ret), == 0 );
	ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Scheduling attribute assignment" )

	TST( schedAttr, "Set/ Get pthread scheduling attributes",
		"for each policy"
		"set/get attr policy,"
		"set/get min pri for policy,"
		"set/get max pri for policy." ),

	TST( schedAttrCreate,
		"Create pthread with explicit scheduling attributes",
		"Set current policy RR,"
		"set current priority min,"
		"create attribute set (FIFO, max)"
		"set explicit attribute"
		"create pthread with attributes"
		"verify attributes in new thread." ),

	TST( schedAttrInherit,
		"Create pthread inheriting scheduling attributes",
		"Set current policy RR,"
		"set current priority min,"
		"create attribute set (FIFO, max)"
		"set inherit attribute"
		"create pthread with attributes"
		"verify attributes in new thread." ),

TST_FINISH
