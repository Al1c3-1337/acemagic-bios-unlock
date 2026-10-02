/* SPDX-License-Identifier: MIT */
#ifndef TOOLKIT_CORE_H
#define TOOLKIT_CORE_H
#include "rx16_memory_core.h"
enum { RX16_UNLOCK=1, RX16_MEMORY=2 };

/* Each mode starts from the original package. Do not stack modes in one boot. */
API int rx16_prepare(int mode,const u8 *src,u64 size,u8 *dst,u64 capacity,u64 *result_size) {
    if(!result_size) return -30;
    *result_size=0;
    if(mode==RX16_MEMORY) return rx16_memory(src,size,dst,capacity,result_size);
    if(mode!=RX16_UNLOCK) return -31;
    int state=rx16_hii((u8*)src,size,0);
    if(state!=1) return state;
    *result_size=size;
    if(!dst) return 1;
    if(capacity<size||dst==src) return -32;
    for(u64 i=0;i<size;i++) dst[i]=src[i];
    return rx16_hii(dst,size,1);
}
API int rx16_tables(int mode,u8 *image,u64 size,int apply) {
    if(mode==RX16_UNLOCK) return rx16_menu(image,size,apply);
    if(mode==RX16_MEMORY) return rx16_memory_menu(image,size,apply);
    return -31;
}
#endif
