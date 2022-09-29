#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_swconfig.h,v 1.1 1999/02/23 20:38:33 tjm Exp $"

/* Structure that maps to fields in the btnode_s struct, to allow
 * access to type specific data (e.g., sw_component_s pointers for
 * both left and right childeren).
 */
typedef struct swcmp_s {
	struct sw_component_s  *s_left;
	struct sw_component_s  *s_right;
	struct sw_component_s  *s_parent;
	char  				   *s_name;
	int  				    s_height;
} swcmp_t;

typedef struct sw_component_s {
	union {
		btnode_t			l_btnode;
		swcmp_t				l_swcmp;
		element_t			l_queue;
	} sw_link;
	k_uint_t				sw_sys_id;
	char				   *sw_description;
	uint					sw_version;
	int						sw_key;
	time_t					sw_install_time;
	time_t					sw_deinstall_time;
	int						sw_flags;
	int						sw_state;
} sw_component_t;

/* For accessing sw_link members in various ways
 */
#define sl_bt sw_link.l_btnode
#define sl_swcmp sw_link.l_swcmp
#define sl_queue sw_link.l_queue

/* For accessing sw_link members in ways that are appropriate for
 * sw_component_s structs.
 */
#define sw_left sl_swcmp.s_left
#define sw_right sl_swcmp.s_right
#define sw_parent sl_swcmp.s_parent
#define sw_name sl_swcmp.s_name
#define sw_height sl_swcmp.s_height

/* For use when the sw_component is on a linked list
 */
#define sw_next sw_link.l_queue.next
#define sw_prev sw_link.l_queue.prev

/* Some flags
 */
#define SW_BTNODE	1
#define SW_QUEUE	2

typedef struct swconfig_s {
	int                     s_flags;      /* K_PERM/K_TEMP, etc. */
	k_uint_t                s_sys_id;     /* system ID */
	int                     s_sys_type;   /* IP type of system */
	time_t                  s_date;       /* if 0 then hwconfig is current */
	sw_component_t         *s_swcmp_root;
	int                     s_swcmp_cnt;
	string_table_t         *s_st;
} swconfig_t;
