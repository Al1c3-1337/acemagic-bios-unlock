/* SPDX-License-Identifier: MIT */
#ifndef RX16_MEMORY_CORE_H
#define RX16_MEMORY_CORE_H
#include "rx16_core.h"
#include "rx16_memory_form.h"
static void wr32(u8 *p,u32 v) { for(unsigned i=0;i<4;i++) p[i]=(u8)(v>>(8*i)); }
/* Validate the complete original Setup package before locating the exact RAM form.
   No settings or code are changed. Output must be a separate allocation. */
API int rx16_memory(const u8 *src,u64 n,u8 *out,u64 capacity,u64 *out_size) {
    if(!out_size) return -20;
    *out_size=0;
    if(rx16_hii((u8*)src,n,0)!=1) return -21;
    u64 pos=20,found=0,package=0; unsigned originals=0,replacements=0;
    while(pos<n) {
        u32 len=rd32(src+pos)&0xffffff;
        if(src[pos+3]==2) {
            for(u64 q=pos+4;q<pos+len;q+=src[q+1]&127) {
                if(src[q]==1 && rd16(src+q+2)==0x279c) {
                    if(pos+len-q>=sizeof(memory_original)&&equal(src+q,memory_original,sizeof(memory_original))) {
                        originals++; found=q; package=pos;
                    } else if(pos+len-q>=sizeof(memory_replacement)&&equal(src+q,memory_replacement,sizeof(memory_replacement))) {
                        replacements++;
                    } else return -22;
                }
            }
        }
        pos+=len;
    }
    if(originals==0&&replacements==1) { *out_size=n; return 2; }
    if(originals!=1||replacements) return -23;
    u64 result=n-sizeof(memory_original)+sizeof(memory_replacement);
    *out_size=result;
    if(!out) return 1;
    if(capacity<result||out==src) return -24;
    for(u64 i=0;i<found;i++) out[i]=src[i];
    for(u64 i=0;i<sizeof(memory_replacement);i++) out[found+i]=memory_replacement[i];
    u64 tail=found+sizeof(memory_original),dest=found+sizeof(memory_replacement);
    for(u64 i=tail;i<n;i++) out[dest+i-tail]=src[i];
    wr32(out+16,(u32)result);
    wr32(out+package,rd32(src+package)-(u32)(sizeof(memory_original)-sizeof(memory_replacement)));
    return 1;
}
API int rx16_memory_menu(u8 *p,u64 n,int apply) {
    static const u16 full[7]={0x2711,0x2713,0x2714,0x2715,0x2716,0x2717,0x2712};
    static const u16 restricted[5]={0x2711,0x2712,0x2715,0x2716,0x2717};
    if(!rx16_image(p,n,1)) return -1;
    int already=rd16(p+0x549a0)==0x279c && rd16(p+0x54bf0)==0x279c;
    for(unsigned i=0;i<7;i++) if(!equal(p+0x54970+32*i,setup_formset,16)
        ||rd16(p+0x54980+32*i)!=(i==1&&already?0x279c:full[i])) return -2;
    for(unsigned i=0;i<5;i++) if(!equal(p+0x54bc0+32*i,setup_formset,16)
        ||rd16(p+0x54bd0+32*i)!=(i==1&&already?0x279c:restricted[i])) return -3;
    if(apply==1) { p[0x549a0]=0x9c; p[0x54bf0]=0x9c; }
    if(apply==-1) { p[0x549a0]=0x13; p[0x54bf0]=0x12; }
    return already?2:1;
}
#endif
