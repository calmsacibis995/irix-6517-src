#ifndef _DSM_EVENTS_H
#define _DSM_EVENTS_H

#define DSM_CHECK_STATE(Pev)                                  \
{                                                             \
   if(state == INITIALIZATION_STATE)                          \
     continue;                                                \
   if(state == SSDB_ARCHIVING_STATE)                          \
   {                                                          \
	if((Pev)->event_class == SSS_INTERNAL_CLASS &&        \
	   (Pev)->event_type == ERROR_EVENT_TYPE)             \
          continue;                                           \
   }                                                          \
}


#endif

