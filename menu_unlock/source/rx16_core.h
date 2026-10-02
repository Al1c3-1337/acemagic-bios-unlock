#ifndef RX16_CORE_H
#define RX16_CORE_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#ifdef RX16_HOST_TEST
#define API __declspec(dllexport)
#else
#define API
#endif
static const u8 setup_formset[16] = {0x4a,0x10,0x59,0x7b,0x0d,0xc0,0x58,0x41,0x87,0xff,0xf0,0x4d,0x63,0x96,0xa9,0x15};
static const u8 setup_var[28] = {0x24,0x1c,0x43,0xd6,0x87,0xec,0xa4,0xeb,0xb5,0x4b,0xa1,0xe5,0x3f,0x3e,0x36,0xb2,0x0d,0xa9,0x01,0xf1,0xc9,0,0x53,0x65,0x74,0x75,0x70,0};
static const u8 hide_question[17] = {0x05,0x91,0xec,0x03,0xed,0x03,0x08,0,0x01,0xf1,0x02,0,0x10,0x10,0,1,0};
static int equal(const void *aa,const void *bb,u64 n) {
    const u8 *a=aa,*b=bb; for(u64 i=0;i<n;i++) if(a[i]!=b[i]) return 0; return 1;
}
static u16 rd16(const u8 *p) { return (u16)(p[0]|((u16)p[1]<<8)); }
static u32 rd32(const u8 *p) { return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); }
static u64 fnv(const u8 *p,u64 n) {
    u64 h=0xcbf29ce484222325ULL; for(u64 i=0;i<n;i++) h=(h^p[i])*0x100000001b3ULL; return h;
}
/* All validation completes before any byte is changed. 1=original, 2=patched. */
API int rx16_hii(u8 *buf,u64 n,int apply) {
    u64 edits[23],pos=20; unsigned count=0,oldcmp=0,newcmp=0,oldhide=0,newhide=0,vars=0,forms=0;
    int end_seen=0;
    if(!buf||n<24||n>0x1000000||rd32(buf+16)!=n) return -1;
    while(pos<n) {
        if(n-pos<4) return -2;
        u32 plen=rd32(buf+pos)&0xffffff; u8 type=buf[pos+3];
        if(plen<4||plen>n-pos) return -3;
        if(type==0xdf) { if(plen!=4||pos+plen!=n) return -4; end_seen=1; }
        if(type==2) {
            u64 q=pos+4,limit=pos+plen; int active=0; unsigned depth=0,formdepth=0;
            while(q<limit) {
                if(limit-q<2) return -5;
                u8 op=buf[q],len=buf[q+1]&0x7f,scope=buf[q+1]>>7;
                if(len<2||len>limit-q) return -6;
                if(op==0x0e) {
                    if(len<23||!scope||active) return -7;
                    active=equal(buf+q+2,setup_formset,16);
                    if(active) { forms++; formdepth=depth+1; }
                }
                if(active) {
                    if(op==0x24 && len==sizeof(setup_var) && equal(buf+q,setup_var,sizeof(setup_var))) vars++;
                    if(op==0x12 && len==6 && !scope && rd16(buf+q+2)==8) {
                        if(rd16(buf+q+4)==1) { if(count>=23) return -8; edits[count++]=q+4; oldcmp++; }
                        else if(rd16(buf+q+4)==2) newcmp++;
                    }
                    /* Only unhide this one question, not every SuppressIf in the BIOS. */
                    if(op==0x0a && len==2 && scope && limit-q>=4+sizeof(hide_question)
                       && buf[q+3]==2 && equal(buf+q+4,hide_question,sizeof(hide_question))) {
                        if(buf[q+2]==0x46) { if(count>=23) return -8; edits[count++]=q+2; oldhide++; }
                        else if(buf[q+2]==0x47) newhide++;
                    }
                }
                if(op==0x29) {
                    if(len!=2||scope||!depth) return -9;
                    if(active&&depth==formdepth) active=0;
                    depth--;
                } else if(scope) depth++;
                q+=len;
            }
            if(depth||active) return -10;
        }
        pos+=plen;
    }
    if(!end_seen||forms!=1||vars!=1) return -11;
    if(oldcmp==0&&newcmp==22&&oldhide==0&&newhide==1) return 2;
    if(oldcmp!=22||newcmp||oldhide!=1||newhide||count!=23) return -12;
    if(apply) for(unsigned i=0;i<count;i++) buf[edits[i]]=(buf[edits[i]]==0x46)?0x47:2;
    return 1;
}
API int rx16_image(const u8 *p,u64 n,int amitse) {
    u64 size=amitse?0x69c80:0x32500,tsize=amitse?0x532e0:0x14440;
    u64 hash=amitse?0x1f7814f7c779b74bULL:0x33051ed4bd1116b9ULL;
    if(!p||n!=size||p[0]!='M'||p[1]!='Z') return 0;
    u32 pe=rd32(p+0x3c);
    if(pe>n-0x108||rd32(p+pe)!=0x4550||rd16(p+pe+4)!=0x8664||rd16(p+pe+24)!=0x20b) return 0;
    return fnv(p+0x2c0,tsize)==hash;
}
API int rx16_menu(u8 *p,u64 n,int apply) {
    static const u16 full[7]={0x2711,0x2713,0x2714,0x2715,0x2716,0x2717,0x2712};
    static const u16 restricted[5]={0x2711,0x2712,0x2715,0x2716,0x2717};
    if(!rx16_image(p,n,1)) return -1;
    for(unsigned i=0;i<7;i++) if(!equal(p+0x54970+32*i,setup_formset,16)||rd16(p+0x54980+32*i)!=full[i]) return -2;
    int already=rd16(p+0x54bf0)==0x2713;
    for(unsigned i=0;i<5;i++) if(!equal(p+0x54bc0+32*i,setup_formset,16)
        ||rd16(p+0x54bd0+32*i)!=(i==1&&already?0x2713:restricted[i])) return -3;
    if(apply) p[0x54bf0]=0x13;
    return already?2:1;
}
#endif
