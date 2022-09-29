/*
 *  @(#) $Id: rsvp_proto.h,v 1.4 1998/11/25 08:43:36 eddiem Exp $
 */
bitmap compute_route(struct in_addr group, struct in_addr src_addr, struct sockaddr_in* from, int dst_flags, int *in_ifp);
int get_vifs();
int if_lookup(struct in_addr group_ad, struct in_addr in_ad);
int if_list_init(void);

int cmp_flowspec(FlowSpec * s1, FlowSpec * s2);
FlowSpec* merge_flowspecs(FlowSpec * s1, FlowSpec * s2);
int min(int x, int y);
int max(int x, int y);

void dump_ds(int force);
void log_destaddr(int, struct in_addr, u_int16_t);
void dump_dest(Session *dest);
void dump_PSB(PSB *send);
void dump_df(RSB *df);
void dump_ff(RSB *ff);
void dump_wf(RSB *wf);
void dump_kf(TCSB *kf, int vif);
char* flg_to_st(int flags);
void print_rsvp(struct packet *);
void log(int, int, const char *, ...);
void log_event(int, char *, SESSION *, const char *, ...);
void hexf(FILE *, char *, int);
char *strtcpy(char *, char *, int);
char *inet_fmt(u_int32_t, char *);

void kill_df(RSB *df, Session *dst);
void kill_wf(RSB *wf, Session *dst);
void kill_ff(RSB *ff, Session *dst);

FlowDesc* find_flowdesc(FlowDesc * descriptors, int n);
int match_filter(FILTER_SPEC * f1, FILTER_SPEC * f2);
Session* locate_session(SESSION *);
PSB* locate_PSB(Session *, SENDER_TEMPLATE *, int, RSVP_HOP *);
int kill_PSB(Session *, PSB *);
int kill_session(Session * );
int refresh_path(Session *);

int accept_path(int in_vif, struct packet *pkt);
int accept_resv(int in_if, struct packet *);
int accept_resp(int in_if, struct packet *);
int accept_path_err(int in_if, struct packet *);
int accept_resv_err(int inp_if, struct packet *);
int accept_path_tear(int inp_if, struct packet *);
int accept_resv_tear(int in_if, struct packet *);
int accept_confirm(int in_if, struct packet *);

int check_hops(struct in_addr phop, int* phops, int nphop);
int clean_up_resv(Session * dst);
int refresh_fixed_sender(PSB * s2, FlowDesc * flow, Session * dst);
int refresh_wild(Session * dst, struct packet *);
TCSB *locate_TCSB(Session *, int, FiltSpecStar *, style_t);
int init_timer(int q_size);
int timer_checks(struct packet * pkt);
int adjust_timer(unsigned long ival);
void add_to_timer(char* user_ptr, int user_data, int delay);
void timer_print(void);
void mmap(void);
void rsvp_path_err(int in_if, int e_code, int e_value, struct packet *);
void rsvp_resv_err(int e_code, int e_value, int flags,
					FiltSpecStar *, struct packet *);
void init_sockets(int vif_num, int if_num);
void init_api(void);
int test_alive(void);
void reset_log(int init);
int timer_signal(char* user_ptr, int user_data);
void multicast(Session *dest, struct packet *pkt);
int main(int argc, char** argv);
void api_PathErr_notify(struct packet *);
void api_ResvErr_notify(struct packet *);
char* rmsuffix(char* cp);
u_long resolve_name(char* namep);
void mcast_rsvp_state(void);
int kern_rsvpresv(int in_if);
int rsvp_config(void);

char *fmt_flowspec(FlowSpec *flowsp);
int send_pkt_out(RSVP_HOP *, struct packet *);
void move_object(Object_header *oldp, Object_header *newp);
char *rsvp_timestamp(void);
void ntoh_VarPart(char *vp, int n);
void hton_VarPart(char *vp, int n);
int start_UDP_encap(int if_no);
void bcast_beacon_pkt(void);
void reset_Encap_beacon(void);

void multicast_path_tear(Session *dest, struct packet *pkt);
int update_state(Session *dp);
void delete_resv(Session *, PSB *);
int send_pkt_out_vif(int, Session *, FILTER_SPEC *, struct packet *);
int flow_reservation(Session *, struct packet *, int, FLOWSPEC *,
							FiltSpecStar *);
int update_TC(Session *, RSB *);
int refresh_resv(Session *);
void common_resv_header(struct packet *, Session *dst);
void resv_tear_flow(Session *, struct packet *, FlowDesc *, int);
int send_resv_tear(Session *, struct packet *, FlowDesc *, int);
void ntoh_flowspec(FlowSpec *);
void hton_flowspec(FlowSpec *);
int cleanup_path_state(Session *);
int cleanup_resv_state(Session *);
void del_from_timer(char *, int);
#ifdef DEBUG
int verify_resv(Session *);
#endif
int send_Encap_beacon(void);
int RandomDelay(int);
int set_if_flag(char *, int);
void init_igmp(void);		/* xxx should be in mrouted header */
unsigned long next_event_time(void);
int raw_input(int ifd, int inp_if);
int udp_input(int ifd, int inp_if);

int api_input(int fd);
void api_path_notify(Session *dest, int type);
int refresh_api(int);
int api_refresh_delay(int);

char *fmt_filtspec(FILTER_SPEC *);
int check_mrouted(void);
void ntoh_packet(struct packet *);
void hton_packet(struct packet *);
void hton_object(Object_header *);
void ntoh_object(Object_header *);
void timer_wakeup(int);
u_int16_t in_cksum(u_int8_t *, int);
void send_igmp(u_int32_t, u_int32_t, int, int, u_int32_t, int);
void fmt_policy1(POLICY_DATA * polp, char * buf, int max);
char *fmt_policy(POLICY_DATA *polp);
int refresh_resp(Session *dest);

