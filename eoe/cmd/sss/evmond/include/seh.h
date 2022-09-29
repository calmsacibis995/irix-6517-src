#ifndef __SEH_H_
#define __SEH_H_

/*
 * Init stuff.
 */
extern __uint64_t seh_check_arguments(int argc,char **argv);
extern __uint64_t seh_initialize(struct event_private *Pev);
extern void seh_clean_everything();
extern void seh_loop_init(struct event_private *Pev,
			  struct event_record *Pev_rec);
			

__uint64_t seh_thread_initialize(struct event_private *Pev);

     
/*
 * API stuff.
 */
extern __uint64_t seh_api_setup_interfaces();
extern __uint64_t seh_tell_result_api(struct event_private *Pev,
				      struct event_record *Pev_rec,
				      __uint64_t err);
extern __uint64_t seh_get_event_api(struct event_private *Pev);
extern void seh_api_clean();

/*
 * DSM stuff.
 */
extern __uint64_t seh_dsm_setup_interfaces();
extern void seh_dsm_clean();
extern __uint64_t seh_tell_event_dsm(struct event_private *Pev,
				     struct event_record *Pev_rec);

/*
 * SSDB stuff.
 */
extern __uint64_t seh_setup_interfaces_ssdb();
extern __uint64_t seh_get_num_events_ssdb(int *Pnum_events);
extern __uint64_t seh_update_event_record_ssdb(struct event_private *Pev,
					       struct event_record *Pev_rec);
extern void seh_clean_ssdb();
extern __uint64_t seh_init_all_events_ssdb();
extern __uint64_t seh_time_throttle_events_ssdb(time_t cur_time);
extern __uint64_t seh_update_all_events_ssdb(__uint64_t sys_id);
extern __uint64_t seh_update_all_events_byclass_ssdb(__uint64_t sys_id, 
								  int class);
extern __uint64_t seh_update_all_events_bytype_ssdb(__uint64_t sys_id,int type);
extern __uint64_t seh_read_sysinfo(SGMLicense_t *);

/*
 * Event stuff.
 */
extern __uint64_t seh_delete_event(struct event_private *Pev);
extern __uint64_t seh_add_event(struct event_private *Pev);
extern __uint64_t seh_find_event(int class,int type, __uint64_t sys_id,
				 struct event_private **PPev);
extern __uint64_t seh_init_event(struct event_private *Pev);
extern __uint64_t seh_init_each_event(struct event_private *Pev);
extern __uint64_t seh_throttle_event(struct event_private *ev, int);
extern __uint64_t seh_init_all_events();
extern __uint64_t seh_syslog_event(struct event_private *Pev,char *msg,...);
extern __uint64_t seh_initialize_error_event(struct event_private *Pev,
						    __uint64_t sys_id);
extern __uint64_t seh_initialize_config_event(struct event_private *Pev, 
						   __uint64_t sys_id);
extern __uint64_t seh_reconfig_event(struct event_private *Pev);
extern char *seh_form_config_event_record(struct event_private *);
extern __uint64_t seh_dsm_sendable_event(struct event_private *Pev,
					 __uint64_t err);
extern __uint64_t seh_api_sendable_event(struct event_private *Pev,
					 __uint64_t err);

extern void seh_events_clean();
extern void seh_events_alarm_handler();
extern void seh_free_event(struct event_private *Pev,void *flag);
extern __uint64_t seh_initialize_both_archive_events();

/*
 * DSM stuff.
 */
extern void dsm_delete_event(struct event_private *Pev);
extern void put_escape_char_before(char *Psrc,char *Pdest,char c);

/*
 * Archive stuff.
 */
extern __uint64_t seh_receive_archive_port(int d,char **Pb,int *l,int *p,
					   __uint64_t *a);
extern __uint64_t seh_send_archive_port(int d,char *b,int l,int p,
					__uint64_t *a);
extern __uint64_t seh_archive_setup_interfaces(int daemon);
extern void seh_archive_clean();

/*
 * string stuff.
 */
extern __uint64_t stringtab_clean();
extern __uint64_t stringtab_init();
extern __uint64_t string_free(char *s,int maxlen);
extern char *string_dup(char *s,int maxlen);

#define SEH_CHECK_STATE(Pev)                                  \
{                                                             \
   switch(state) {                                            \
	case SEH_ARCHIVING_STATE:                             \
	/*                                                    \
	 * Signal everyone that SEH is ready for archiving.   \
	 */                                                   \
                  SSDB_ARCHIVE_STATE();                       \
        case SSDB_ARCHIVING_STATE:                            \
        case INITIALIZATION_STATE:                            \
		  return 0;                                   \
        case EVENT_HANDLING_STATE:                            \
       default:                                               \
                  break;                                      \
   }                                                          \
}           

#define ALARM_CHECK_STATE()                                   \
{                                                             \
  switch(state) {                                             \
        case SEH_ARCHIVING_STATE:                             \
        case SSDB_ARCHIVING_STATE:                            \
 	  if(i++ == ARCHIVE_TIMEOUT)                          \
            {						      \
	      EVENT_STATE();	/* reset to event state. */   \
              i=0;					      \
            }                                                 \
	  break;                                              \
  default:                                                    \
    break;                                                    \
  }                                                           \
}

#endif /* __SEH_H_ */
