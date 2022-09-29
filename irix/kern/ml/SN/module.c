#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/nodepda.h>
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/SN/module.h>

#define LDEBUG		0

#define DPRINTF		if (LDEBUG) printf

module_t	       *modules[MODULE_MAX];
int			nummodules;

#define SN00_SERIAL_FUDGE	0x3b1af409d513c2
#define SN0_SERIAL_FUDGE	0x6e

void
encode_int_serial(uint64_t src,uint64_t *dest)
{
    uint64_t val;
    int i;

    val = src + SN00_SERIAL_FUDGE;


    for (i = 0; i < sizeof(long long); i++) {
	((char*)dest)[i] =
	    ((char*)&val)[sizeof(long long)/2 +
			 ((i%2) ? ((i/2 * -1) - 1) : (i/2))];
    }
}


void
decode_int_serial(uint64_t src, uint64_t *dest)
{
    uint64_t val;
    int i;

    for (i = 0; i < sizeof(long long); i++) {
	((char*)&val)[sizeof(long long)/2 +
		     ((i%2) ? ((i/2 * -1) - 1) : (i/2))] =
	    ((char*)&src)[i];
    }

    *dest = val - SN00_SERIAL_FUDGE;
}


void
encode_str_serial(const char *src, char *dest)
{
    int i;

    for (i = 0; i < MAX_SERIAL_NUM_SIZE; i++) {

	dest[i] = src[MAX_SERIAL_NUM_SIZE/2 +
		     ((i%2) ? ((i/2 * -1) - 1) : (i/2))] +
	    SN0_SERIAL_FUDGE;
    }
}

void
decode_str_serial(const char *src, char *dest)
{
    int i;

    for (i = 0; i < MAX_SERIAL_NUM_SIZE; i++) {
	dest[MAX_SERIAL_NUM_SIZE/2 +
	    ((i%2) ? ((i/2 * -1) - 1) : (i/2))] = src[i] -
	    SN0_SERIAL_FUDGE;
    }
}


module_t *module_lookup(moduleid_t id)
{
    int			i;

    DPRINTF("module_lookup: id=%d\n", id);

    for (i = 0; i < nummodules; i++)
	if (modules[i]->id == id) {
	    DPRINTF("module_lookup: found m=0x%x\n", modules[i]);
	    return modules[i];
	}

    return NULL;
}

/*
 * module_add_node
 *
 *   The first time a new module number is seen, a module structure is
 *   inserted into the module list in order sorted by module number
 *   and the structure is initialized.
 *
 *   The node number is added to the list of nodes in the module.
 */

module_t *module_add_node(moduleid_t id, cnodeid_t n)
{
    module_t	       *m;
    int			i;

    DPRINTF("module_add_node: id=%d node=%d\n", id, n);

    if ((m = module_lookup(id)) == 0) {
	m = kmem_zalloc_node(sizeof (module_t), KM_NOSLEEP, n);
	ASSERT_ALWAYS(m);

	DPRINTF("module_add_node: m=%x\n", m);

	m->id = id;
	spinlock_init(&m->lock, "modulelock");

	initnsema(&m->thdcnt, 0, "thdcnt");
	elsc_init(&m->elsc, COMPACT_TO_NASID_NODEID(n));
	spinlock_init(&m->elsclock, "elsclock");

	/* Insert in sorted order by module number */

	for (i = nummodules; i > 0 && modules[i - 1]->id > id; i--)
	    modules[i] = modules[i - 1];

	modules[i] = m;
	nummodules++;
    }

    m->nodes[m->nodecnt++] = n;

    DPRINTF("module_add_node: module %d now has %d nodes\n", id, m->nodecnt);

    return m;
}

int module_probe_snum(module_t *m, nasid_t nasid)
{
    lboard_t	       *board;
    klmod_serial_num_t *comp;

    board = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
			KLTYPE_MIDPLANE8);

    if (! board || KL_CONFIG_DUPLICATE_BOARD(board))
	return 0;

    comp = GET_SNUM_COMP(board);

    if (comp) {
#if defined (SN0)
	if (private.p_sn00) {
	    DPRINTF("********found sn00 module with "
		    "id %d and serial 0x%llx\n",
		    m->id, comp->snum.snum_int);

	    if (comp->snum.snum_int != 0) {
		m->snum.snum_int = comp->snum.snum_int;
		m->snum_valid = 1;
	    }

	}
	else
#endif /* SN0 */
	{
#if LDEBUG
	    int i;

	    printf("********found module with id %d and string", m->id);

	    for (i = 0; i < MAX_SERIAL_NUM_SIZE; i++)
		printf(" %x ", comp->snum.snum_str[i]);

	    printf("\n");	/* Fudged string is not ASCII */
#endif

	    if (comp->snum.snum_str[0] != '\0') {
		bcopy(comp->snum.snum_str,
		      m->snum.snum_str,
		      MAX_SERIAL_NUM_SIZE);
		m->snum_valid = 1;
	    }
	}
    }

    if (m->snum_valid)
	return 1;
    else {
	cmn_err(CE_WARN | CE_MAINTENANCE,
		"Invalid serial number for module %d, "
		"possible missing or invalid NIC.", m->id);
	return 0;
    }
}

void
module_init(void)
{
    cnodeid_t		node;
    lboard_t	       *board;
    nasid_t		nasid;
    int			nserial;
    module_t	       *m;

    DPRINTF("*******module_init\n");

    nserial = 0;

    for (node = 0; node < numnodes; node++) {
	nasid = COMPACT_TO_NASID_NODEID(node);

	board = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid),
			    KLTYPE_IP27);
	ASSERT(board);

	m = module_add_node(board->brd_module, node);

	if (! m->snum_valid && module_probe_snum(m, nasid))
	    nserial++;
    }

    DPRINTF("********found total of %d serial numbers in the system\n",
	    nserial);

    if (nserial == 0)
	cmn_err(CE_WARN, "No serial number found.");
}

elsc_t *get_elsc(void)
{
    return &NODEPDA(cnodeid())->module->elsc;
}

int
get_kmod_info(cmoduleid_t cmod, module_info_t *mod_info)
{
    int i;

    if (cmod < 0 || cmod >= nummodules)
	return EINVAL;

    if (! modules[cmod]->snum_valid)
	return ENXIO;

    mod_info->mod_num = modules[cmod]->id;

#if defined (SN0)
    if (private.p_sn00) {
	int i;
	uint64_t snum64;
	uint32_t snum32;
	char *cur;

	decode_int_serial(modules[cmod]->snum.snum_int,
			  &snum64);

	/* if this is an invalid serial number return an error */
	if ((snum64 == 0) || (snum64 > 0x8ffffffffff))
	    return ENXIO;

	/* this chops off the top two bytes */
	snum32 = snum64;

	mod_info->serial_num = snum32;

	/* convert to 8 hex characters */
	mod_info->serial_str[8] = '\0';
	cur = mod_info->serial_str + 7;

	for (i = 0; i < 8; i++) {
	    *cur-- = "0123456789ABCDEF"[snum32%16];
	    snum32 /= 16;
	}

	DPRINTF("********int get info, 0x%llx decoded to 0x%llx, "
		"string is %s\n",
		modules[cmod]->snum.snum_int,
		mod_info->serial_num, mod_info->serial_str);
    }
    else
#endif /* SN0 */
    {
	char temp[MAX_SERIAL_NUM_SIZE];

	decode_str_serial(modules[cmod]->snum.snum_str, temp);

	/* if this is an invalid serial number return an error */
	if (temp[0] != 'K')
	    return ENXIO;

	mod_info->serial_num = 0;

	for (i = 0; i < MAX_SERIAL_NUM_SIZE && temp[i] != '\0'; i++) {
	    mod_info->serial_num <<= 4;
	    mod_info->serial_num |= (temp[i] & 0xf);

	    mod_info->serial_str[i] = temp[i];
	}

	mod_info->serial_str[i] = '\0';
    }

    return 0;
}
