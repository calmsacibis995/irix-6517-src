/* Spans for Calligraphy logos.
 */

#ident "$Revision: 1.17 $"

#if IP20
#include <logo/indigo.h>
#elif IP24
#include <logo/indy.h>
#elif IP22
#include <logo/indigo2.h>
#elif IP26
#include <logo/i2tfp.h>
#elif IP28
#include <logo/t5i2.h>
#elif IP19 || IP21 || IP25 || IP27
#include <logo/onyx.h>
#elif IP30
#include <logo/racer.h>
#elif IP32
#include <logo/o2.h>
#else					/* use unknown image */
#include <logo/generic.h>
#endif
