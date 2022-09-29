typedef struct {
	uint_t	muxid;
	uint_t	inaddr;
	uint_t	subnet_mask;
	char	forward_bdcst;
	char	keepalive;
	short	mtu;
} NET_ADDRS;

#define IP_NET		('I' << 8)
#define IP_NET_ADDR	(IP_NET| 0x01)
