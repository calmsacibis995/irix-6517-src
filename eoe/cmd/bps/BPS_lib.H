/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kao                                                 */
/*    FILE: BPS_lib.H                                           */
/*    ASGN:                                                     */
/*    DATE: Mon Jul 10 12:48:33 1995                            */
/****************************************************************/


#ifndef BPS_LIB_HEADER
#define BPS_LIB_HEADER
#include <assert.h>
#include "bps.h"
#include <iostream.h>
#include <bool.h>
#include <function.h>


struct BPSCookieCmp : binary_function<bps_magic_cookie_t,
                                      bps_magic_cookie_t, bool> {
   bool operator() (const bps_magic_cookie_t& x,
                    const bps_magic_cookie_t& y) const{
                       return bps_magic_cookie_cmp((bps_magic_cookie_t*) &x,
                                                   (bps_magic_cookie_t*) &y);
   }
};

struct BPSPartComp : binary_function<bps_sspartition_id_t, bps_sspartition_id_t,bool>{
   bool operator() (const bps_sspartition_id_t& x,
                    const bps_sspartition_id_t& y) const {
                       return bps_sspartid_greaterthan((bps_sspartition_id_t*) &x,
                                                       (bps_sspartition_id_t*) &y);
                    };
};


template <class Ptr>
class BPSPtrWrapper {
   Ptr* ptr_;
public:
   BPSPtrWrapper(Ptr* ptr = 0) : ptr_(ptr){;}
   BPSPtrWrapper(const BPSPtrWrapper& ptrw) : ptr_(ptrw.ptr_){;}
   ~BPSPtrWrapper(){;}
   BPSPtrWrapper& operator=(const BPSPtrWrapper& ptrw){ptr_ = ptrw.ptr_; 
                                                    return *this;}
   const Ptr* operator() () const {return ptr_;}
   Ptr* operator() () {return ptr_;}
   Ptr* operator->() {return ptr_;}
};
template<class T>
bool operator==(const BPSPtrWrapper<T>& ptrw1,
                const BPSPtrWrapper<T>& ptrw2){
   return ( ptrw1.operator()() && ptrw2.operator()() ? *ptrw1() == *ptrw2() : false);
};
template <class Ptr>
bool operator <(const BPSPtrWrapper<Ptr>& ptrw1,
                const BPSPtrWrapper<Ptr>& ptrw2){
   retrurn (ptrw1() && ptrw2 ? *ptrw1() < *ptrw2() : false);
};
template <class Ptr>
bool operator>(const BPSPtrWrapper<Ptr>& ptrw1,
               const BPSPtrWrapper<Ptr>& ptrw2){
   return (ptrw1() && ptrw2() ? !(*ptrw1() < *ptrw2()) : false);
p};
template <class Ptr>
void print(const BPSPtrWrapper<Ptr>& ptrw1){
   cout << " " << (*ptrw1())() ;
};

template<class T>
void varAlloc(T*& array, int size){
   array =(T*)malloc(sizeof(array)+sizeof(T)*size-1);
   array->size = size;
};
void varAlloc(partition_ids_t*& array, int size);
void varAlloc(cpus_used_t*& array, int size);
void varAlloc(memory_used_t*& array,int size);
const int USEC_PER_SEC = 1000000;
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000;
#endif
#define tstoS(fts)\
    ((fts).tv_sec + (fts).tv_usec/USEC_PER_SEC)
#define Stots(ts,sec)\
        ts.tv_sec = sec; \
        ts.tv_usec = 0;

#endif
     


 

