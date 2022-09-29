/*
 * FILE: eoe/cmd/miser/cmd/miser/debug.C
 *
 * DESCRIPTION:
 *	Implementation and definition of the debug symbols and operators. 
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "debug.h"
#include "string.h"
#include <assert.h>


id_type_t 
read_id(istream& is) 
{
	char* buf;
	char* buffer = new char[sizeof(id_type_t)];
	buf = buffer;

	memset(buffer,0,sizeof(id_type_t));
	is >> buf;
	id_type_t ret_val;
	memcpy(&ret_val, buffer,strlen(buffer));

	return ret_val;

} /* read_id */

   
ostream& 
operator<< (ostream& os, miser_seg_t& segment)
{
	os << "Segment Time [";
	os << segment.ms_rtime << "] ctime [";
	os << segment.ms_etime << "] ncpus [";
	os << segment.ms_resources.mr_cpus << "] multiple [";
	os << segment.ms_multiple << "] memory [";
	os << segment.ms_resources.mr_memory << "] priority [";
	os << segment.ms_priority << "] ";

	return os;

} /* operator<< */ 


istream& 
operator>> (istream& is, miser_seg_t& segment)
{
	is >> segment.ms_rtime;
	is >> segment.ms_etime;
	is >> segment.ms_resources.mr_cpus;
	is >> segment.ms_multiple;
	is >> segment.ms_priority;

	return is;

} /* operator>> */ 


ostream& 
operator<< (ostream& os, vector<miser_space>& queue)
{
	os << "space vector";

	for(int i = 0; i <queue.size();i++) {

		if ((i % 5) == 0) {
			os << endl;
		}

		os << " [" << i << "] " << queue[i].mr_memory << " -  ";
		os << queue[i].mr_cpus;
	}

	return os;

} /* operator<< */


istream&
operator>> (istream& is, miser_qseg_t& t)
{
	is >> t.mq_stime;
	is >> t.mq_etime;
	is >> t.mq_resources.mr_cpus;

	return is;

} /* operator>> */


ostream& 
operator<< (ostream& os, miser_qseg_t& t)
{
	os << "Resource { STARTTIME [";
	os << t.mq_stime << "] ENDTIME [";
	os << t.mq_etime << "] MEMORY [";
	os << t.mq_resources.mr_memory << "] NCPUS [";
	os << t.mq_resources.mr_cpus << "] }";

	return os;

} /* operator<< */
