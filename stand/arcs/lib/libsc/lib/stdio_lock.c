#include <alocks.h>

/* Include in seperate file as some machines need to declare arcs_ui_lock to
 * not be in the .data section.
 */

lock_t arcs_ui_lock = 0;        /* lock routines won't use until initialized */
