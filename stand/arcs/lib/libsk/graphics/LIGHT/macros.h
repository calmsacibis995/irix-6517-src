#ident  "lib/libsk/graphics/LIGHT/macros.h:  $Revision: 1.3 $"

/*
 * macros.h - macros to talk to the LIGHT hardware
 */

#define RexWait(hw)	       	while ((hw->p1.set.configmode) & CHIPBUSY)

#define RexSetConfig(hw, reg, val)	hw->p1.set.reg = val;

#ifdef IP20
#define IP20DELAY	IP20lg1delay()
void IP20lg1delay(void);
#else
#define IP20DELAY
#endif

#define VC1SetByte(base, data) \
    base->p1.set.rwvc1 = data; \
    base->p1.go.rwvc1 = data;

#define VC1SetAddr(hw, addr, configaddr) \
    hw->p1.go.configsel = VC1_ADDR_HIGH; \
    VC1SetByte(hw, (addr) >> 8 & 0xff); \
    hw->p1.go.configsel = VC1_ADDR_LOW; \
    VC1SetByte(hw, (addr) & 0xff); \
    hw->p1.go.configsel = configaddr;

#define VC1SetWord(hw, data) \
    VC1SetByte(hw, (data) >> 8 & 0xff); \
    VC1SetByte(hw, (data) & 0xff);

#define VC1GetByte(hw, data) \
    data = hw->p1.go.rwvc1; \
    IP20DELAY; \
    data = hw->p1.set.rwvc1;

#define VC1GetWord(hw, data) { \
    unsigned vc1_get_word_tmp; \
    VC1GetByte(hw, vc1_get_word_tmp); \
    VC1GetByte(hw, data); \
    data = ((vc1_get_word_tmp << 8) & 0xff00) | (data & 0xff); \
}

#define btSet(hw, addr, val) \
    hw->p1.set.configsel = addr; \
    hw->p1.set.rwdac = val; \
    hw->p1.go.rwdac = val;

#define btGet(hw, addr, val) \
    hw->p1.set.configsel = addr; \
    val = hw->p1.go.rwdac; \
    IP20DELAY; \
    val = hw->p1.set.rwdac;
