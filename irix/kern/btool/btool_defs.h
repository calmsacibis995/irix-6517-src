/*  These definitions are not normally needed outside of btool_lib.c.
 *  They are here for the convenience of people who have to modify the 
 *  library routines to work in embedded systems.
 */

/* The data kept for a single branch. */
typedef struct
{
    int true_count;
    int false_count;
} btool_branch_t;

/* The type of an entire in-core log.  This is a typedef because that's 
   needed by the log-extraction routines used at Motorola UDC. */

typedef btool_branch_t btool_log_t[NUM_BRANCHES];
extern btool_log_t Btool_branches;

