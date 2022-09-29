/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "pqueue.h"

queue_t*
queue_create()
{
        queue_t* q;

        /*
         * basic structure
         */
        q = (queue_t*)malloc(sizeof(queue_t));
        if (q == NULL) {
                return (NULL);
        }

        /*
         * header
         */
        q->hd = (qelem_t*)malloc(sizeof(qelem_t));
        if (q->hd == NULL) {
                free(q);
                return (NULL);
        }
        q->hd->pdesc = (pdesc_t*)QHDPDESC;
        q->hd->fp = q->hd;
        q->hd->bp = q->hd;

        /*
         * elements
         */
        q->nelem = 0;

        /*
         * Mutex semaphore
         */
        q->mutex = sema_create();
        if (q->mutex == NULL) {
                return (NULL);
        }
        sema_v(q->mutex);
    
        /*
         * Count semaphore
         */
        q->count = sema_create();
        if (q->count == NULL) {
                return (NULL);
        }

        return (q);
}

static void
queue_check_consistency(queue_t* q, int maxelem)
{
        qelem_t* p;
        int count;

        assert(q != 0);
        assert(q->hd != 0);
        assert(q->mutex != 0);
        assert(q->count != 0);
        assert(q->hd->pdesc == QHDPDESC);
    
        for (p = q->hd->fp, count = 0; p != q->hd; p = p->fp) {
                assert(p != 0);
                assert(p->fp != 0);
                assert(p->bp != 0);
                assert(p->fp->bp == p);
                assert(p->bp->fp == p);
                count++;
                assert(count < maxelem);
                assert(count <= q->nelem);
        }
        assert(count == q->nelem);

    
        for (p = q->hd->bp, count = 0; p != q->hd; p = p->bp) {
                assert(p != 0);
                assert(p->fp != 0);
                assert(p->bp != 0);
                assert(p->fp->bp == p);
                assert(p->bp->fp == p);
                count++;
                assert(count < maxelem);
                assert(count <= q->nelem);
        }
        assert(count == q->nelem);
}

void
queue_enqueue(queue_t* q, qelem_t* qe)
{
        assert(q != 0);
        assert(qe != 0);
        assert(q->hd != 0);
        assert(q->mutex != 0);
        assert(q->count != 0);
        assert(q->hd->pdesc == QHDPDESC);
    
    
        sema_p(q->mutex);
        queue_check_consistency(q, QMAXELEM);
        qe->fp = q->hd;
        qe->bp = q->hd->bp;
        q->hd->bp->fp = qe;
        q->hd->bp = qe;
        q->nelem++;
        queue_check_consistency(q, QMAXELEM);
        sema_v(q->mutex);
#if DEBUG > 4
        printf(">>>> V(count) 0x%x\n", q->count);fflush(stdout);
#endif    
        sema_v(q->count);
}

qelem_t*
queue_dequeue(queue_t* q)
{
        qelem_t* qe;
    
        assert(q != 0);
        assert(q->hd != 0);
        assert(q->mutex != 0);
        assert(q->count != 0);
        assert(q->hd->pdesc == QHDPDESC);

#if DEBUG > 4    
        printf(">>>> P(count) 0x%x\n", q->count); fflush(stdout);
#endif    
        sema_p(q->count);
#if DEBUG > 4    
        printf("<<<< Acquired P 0x%x\n", q->count);
#endif    

        sema_p(q->mutex);
        assert(q->nelem > 0);
        assert(q->hd->fp != 0);    
        assert(q->hd->fp != q->hd);
        assert(q->hd->fp->pdesc != 0);
        assert(q->hd->fp->pdesc != QHDPDESC);
        queue_check_consistency(q, QMAXELEM);
        qe = q->hd->fp;
        q->hd->fp = qe->fp;
        qe->fp->bp = q->hd;
        qe->bp = 0;
        qe->fp = 0;
        q->nelem--;
        queue_check_consistency(q, QMAXELEM);
        sema_v(q->mutex);

        return (qe);
}

void
queue_destroy(queue_t* q)
{
        qelem_t* p;
        qelem_t* v;
    
        assert(q != 0);
        assert(q->hd != 0);
        assert(q->mutex != 0);
        assert(q->count != 0);
        assert(q->hd->pdesc == QHDPDESC);

        for (p = q->hd->fp; p != q->hd;) {
                v = p;
                p = p->fp;
                assert(v != 0);
                assert(v->pdesc != 0);
                pdesc_destroy(v->pdesc);
                free(v);
        }

        sema_destroy(q->count);
        sema_destroy(q->mutex);
        free(q->hd);
        free(q);
}

 
    
    
   
    








