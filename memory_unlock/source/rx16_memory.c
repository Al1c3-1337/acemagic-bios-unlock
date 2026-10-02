#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/SimpleFileSystem.h>
#include "rx16_memory_core.h"

static EFI_SYSTEM_TABLE *st;
static EFI_BOOT_SERVICES *bs;
static EFI_FILE_PROTOCOL *logfile;
static EFI_GUID loaded_guid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID hii_guid=EFI_HII_DATABASE_PROTOCOL_GUID;
static EFI_GUID browser_guid=EFI_FORM_BROWSER2_PROTOCOL_GUID;
static EFI_GUID config_guid=EFI_HII_CONFIG_ACCESS_PROTOCOL_GUID;
static EFI_GUID fs_guid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID setup_file={0x899407d7,0x99fe,0x43d8,{0x9a,0x21,0x79,0xec,0x32,0x8c,0xac,0x21}};
static EFI_GUID amitse_file={0xb1da0adf,0x4f77,0x4070,{0xa8,0x8e,0xbf,0xfe,0x1c,0x60,0x52,0x9a}};
static EFI_GUID var_guid={0xec87d643,0xeba4,0x4bb5,{0xa1,0xe5,0x3f,0x3e,0x36,0xb2,0x0d,0xa9}};

void *memcpy(void *d,const void *s,UINTN n) { for(UINTN i=0;i<n;i++) ((u8*)d)[i]=((const u8*)s)[i]; return d; }
void *memset(void *d,int v,UINTN n) { for(UINTN i=0;i<n;i++) ((u8*)d)[i]=(u8)v; return d; }
static void say(CHAR16 *s) {
    st->ConOut->OutputString(st->ConOut,s);
    if(logfile) { UINTN n=0; while(s[n]) n++; n*=2; logfile->Write(logfile,&n,s); logfile->Flush(logfile); }
}
static void hex(UINT64 v) {
    CHAR16 out[19]; const CHAR16 *digits=L"0123456789ABCDEF";
    out[0]='0'; out[1]='x'; for(UINTN i=0;i<16;i++) out[2+i]=digits[(v>>(60-4*i))&15]; out[18]=0; say(out);
}
static void status(CHAR16 *label,EFI_STATUS code) { say(label); hex(code); say(L"\r\n"); }
static CHAR16 key(void) {
    EFI_INPUT_KEY k; UINTN i;
    while(1) {
        if(!EFI_ERROR(st->ConIn->ReadKeyStroke(st->ConIn,&k))) return k.UnicodeChar;
        bs->WaitForEvent(1,&st->ConIn->WaitForKey,&i);
    }
}
static void initlog(EFI_HANDLE self) {
    EFI_LOADED_IMAGE_PROTOCOL *li=0; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=0; EFI_FILE_PROTOCOL *root=0,*old=0;
    if(EFI_ERROR(bs->HandleProtocol(self,&loaded_guid,(VOID**)&li))) return;
    if(EFI_ERROR(bs->HandleProtocol(li->DeviceHandle,&fs_guid,(VOID**)&fs))) return;
    if(EFI_ERROR(fs->OpenVolume(fs,&root))) return;
    if(!EFI_ERROR(root->Open(root,&old,L"RX16_RAM_LOG.txt",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0))) old->Delete(old);
    root->Open(root,&logfile,L"RX16_RAM_LOG.txt",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
    root->Close(root);
    if(logfile) { CHAR16 bom=0xfeff; UINTN n=2; logfile->Write(logfile,&n,&bom); }
}
static int filematch(EFI_LOADED_IMAGE_PROTOCOL *li,const EFI_GUID *g) {
    EFI_DEVICE_PATH_PROTOCOL *dp=li->FilePath;
    for(UINTN walked=0;dp&&walked<4096;) {
        UINTN len=(UINTN)dp->Length[0]+((UINTN)dp->Length[1]<<8);
        if(len<4||len>4096-walked) return 0;
        if(dp->Type==MEDIA_DEVICE_PATH && dp->SubType==MEDIA_PIWG_FW_FILE_DP && len==20 && equal((u8*)dp+4,g,16)) return 1;
        if(dp->Type==END_DEVICE_PATH_TYPE) return 0;
        walked+=len; dp=(EFI_DEVICE_PATH_PROTOCOL*)((u8*)dp+len);
    }
    return 0;
}
static EFI_STATUS images(EFI_LOADED_IMAGE_PROTOCOL **setup,EFI_LOADED_IMAGE_PROTOCOL **ami) {
    EFI_HANDLE *handles=0; UINTN n=0; unsigned sc=0,ac=0;
    EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&loaded_guid,0,&n,&handles);
    if(EFI_ERROR(s)) return s;
    for(UINTN i=0;i<n;i++) {
        EFI_LOADED_IMAGE_PROTOCOL *li=0;
        if(EFI_ERROR(bs->HandleProtocol(handles[i],&loaded_guid,(VOID**)&li))) continue;
        if(filematch(li,&setup_file)) { sc++; *setup=li; }
        if(filematch(li,&amitse_file)) { ac++; *ami=li; }
    }
    bs->FreePool(handles);
    if(sc!=1||ac!=1) return EFI_NOT_FOUND;
    if(!rx16_image((*setup)->ImageBase,(*setup)->ImageSize,0)||!rx16_image((*ami)->ImageBase,(*ami)->ImageSize,1)) return EFI_UNSUPPORTED;
    return EFI_SUCCESS;
}
static void decimal(UINTN v) {
    CHAR16 buf[22]; UINTN p=21; buf[p]=0;
    do { buf[--p]=(CHAR16)('0'+v%10); v/=10; } while(v);
    say(buf+p);
}
static void values(void) {
    u8 data[0xc9]; UINTN size=sizeof(data); UINT32 attrs=0;
    EFI_STATUS s=st->RuntimeServices->GetVariable(L"Setup",&var_guid,&attrs,&size,data);
    status(L"Read Setup status: ",s);
    if(!EFI_ERROR(s)&&size==sizeof(data)) {
        say(L"Setup Item Hide Control (0x02): "); hex(data[2]); say(L"\r\n");
        say(L"OEM Active Memory Timing Settings (Setup 0xAF): "); hex(data[0xaf]); say(L"\r\n");
        say(L"OEM Memory Target Speed (Setup 0xB0), MT/s: "); decimal(rd16(data+0xb0)); say(L"\r\n");
    }
    EFI_GUID cbs_guid={0x3a997502,0x647a,0x4c82,{0x99,0x8e,0x52,0xef,0x94,0x86,0xa2,0x47}};
    u8 cbs[0x67f]; size=sizeof(cbs);
    s=st->RuntimeServices->GetVariable(L"AmdSetupPHX",&cbs_guid,&attrs,&size,cbs);
    status(L"Read AMD CBS status: ",s);
    if(!EFI_ERROR(s)&&size==sizeof(cbs)) {
        say(L"AMD Active Memory Timing Settings (0x5E): "); hex(cbs[0x5e]); say(L"\r\n");
        say(L"AMD Memory Target Speed (0x5F), MT/s: "); decimal(rd16(cbs+0x5f)); say(L"\r\n");
    }
    say(L"Timing switch: FF=Auto; 01=Enabled.\r\n");
}
static EFI_STATUS findforms(EFI_HII_DATABASE_PROTOCOL *db,EFI_HII_HANDLE *handle,VOID **original,UINTN *size) {
    UINTN bytes=0; EFI_HII_HANDLE *handles=0; unsigned matches=0;
    EFI_STATUS s=db->ListPackageLists(db,EFI_HII_PACKAGE_FORMS,0,&bytes,0);
    if(s!=EFI_BUFFER_TOO_SMALL||bytes>65536||bytes%sizeof(*handles)) return EFI_NOT_FOUND;
    s=bs->AllocatePool(EfiBootServicesData,bytes,(VOID**)&handles); if(EFI_ERROR(s)) return s;
    s=db->ListPackageLists(db,EFI_HII_PACKAGE_FORMS,0,&bytes,handles);
    if(EFI_ERROR(s)) { bs->FreePool(handles); return s; }
    for(UINTN i=0;i<bytes/sizeof(*handles);i++) {
        UINTN len=0; VOID *buf=0;
        s=db->ExportPackageLists(db,handles[i],&len,0);
        if(s!=EFI_BUFFER_TOO_SMALL||len<24||len>0x1000000) continue;
        if(EFI_ERROR(bs->AllocatePool(EfiBootServicesData,len,&buf))) continue;
        s=db->ExportPackageLists(db,handles[i],&len,buf);
        u64 target_size=0;
        if(!EFI_ERROR(s) && rx16_memory(buf,len,0,0,&target_size)==1) {
            matches++; if(matches==1) { *handle=handles[i]; *original=buf; *size=len; } else bs->FreePool(buf);
        } else bs->FreePool(buf);
    }
    bs->FreePool(handles);
    if(matches!=1) { if(*original) { bs->FreePool(*original); *original=0; } return EFI_NOT_FOUND; }
    return EFI_SUCCESS;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE self,EFI_SYSTEM_TABLE *table) {
    st=table; bs=table->BootServices; initlog(self);
    say(L"\r\nRX16 A-004 OEM memory-page test v0.2\r\n");
    say(L"Experimental. No flashing and no automatic settings save.\r\n");
    say(L"The menu edits last only until reboot.\r\n\r\n");
    values();
    EFI_LOADED_IMAGE_PROTOCOL *setup=0,*ami=0;
    EFI_STATUS s=images(&setup,&ami);
    status(L"Exact Setup and AMITSE code fingerprints: ",s);
    if(EFI_ERROR(s)) goto finished;
    if(rx16_memory_menu(ami->ImageBase,ami->ImageSize,0)!=1) { say(L"Menu table is not in its expected original state. Stopped.\r\n"); goto finished; }
    EFI_HII_DATABASE_PROTOCOL *db=0;
    s=bs->LocateProtocol(&hii_guid,0,(VOID**)&db); status(L"HII database: ",s); if(EFI_ERROR(s)) goto finished;
    EFI_HII_HANDLE handle=0; VOID *original=0,*modified=0; UINTN size=0;
    s=findforms(db,&handle,&original,&size); status(L"Exactly one matching RX16 form package: ",s); if(EFI_ERROR(s)) goto finished;
    EFI_HANDLE owner=0;
    s=db->GetPackageListHandle(db,handle,&owner); status(L"Existing HII configuration-owner lookup: ",s);
    /* Keep this existing package/owner association; never create a substitute form set. */
    if(EFI_ERROR(s)||!owner) { bs->FreePool(original); goto finished; }
    EFI_HII_CONFIG_ACCESS_PROTOCOL *access=0;
    s=bs->HandleProtocol(owner,&config_guid,(VOID**)&access);
    status(L"Original setup configuration handler: ",s);
    if(EFI_ERROR(s)||!access||!access->RouteConfig) { bs->FreePool(original); goto finished; }
    say(L"Verified: exact OEM memory page and original configuration handler.\r\n");
    say(L"This exposes the OEM timing switch and a numeric speed field.\r\n");
    say(L"RAM changes can prevent boot; recovery may require a CMOS reset.\r\n");
    say(L"Press M to prepare the memory page, or any other key to exit.\r\n");
    CHAR16 k=key(); if(k!='m'&&k!='M') { bs->FreePool(original); goto finished; }
    s=bs->AllocatePool(EfiBootServicesData,size,&modified);
    if(EFI_ERROR(s)) { bs->FreePool(original); goto finished; }
    u64 modified_size=0;
    if(rx16_memory(original,size,modified,size,&modified_size)!=1) {
        say(L"Memory form validation changed; stopped.\r\n"); goto cleanup;
    }
    s=db->UpdatePackageList(db,handle,modified); status(L"Update existing HII menu definitions: ",s);
    if(EFI_ERROR(s)) {
        status(L"Restore original HII package: ",db->UpdatePackageList(db,handle,original)); goto cleanup;
    }
    VOID *readback=0; UINTN checksize=0; EFI_HANDLE newowner=0; int verified=0;
    s=db->ExportPackageLists(db,handle,&checksize,0);
    if(s==EFI_BUFFER_TOO_SMALL && checksize>=24 && checksize<=0x1000000
       && !EFI_ERROR(bs->AllocatePool(EfiBootServicesData,checksize,&readback))) {
        s=db->ExportPackageLists(db,handle,&checksize,readback);
        u64 returned_size=0;
        if(!EFI_ERROR(s)&&checksize==modified_size&&equal(readback,modified,modified_size)
           &&rx16_memory(readback,checksize,0,0,&returned_size)==2
           && !EFI_ERROR(db->GetPackageListHandle(db,handle,&newowner)) && newowner==owner) verified=1;
        bs->FreePool(readback);
    }
    if(!verified) {
        say(L"HII read-back or original handler association did not verify. Restoring.\r\n");
        status(L"Restore: ",db->UpdatePackageList(db,handle,original)); goto cleanup;
    }
    say(L"Read-back verified the complete modified package and its original configuration handler.\r\n");
    if(rx16_memory_menu(ami->ImageBase,ami->ImageSize,1)!=1) {
        say(L"Menu-table validation changed. Restoring HII package.\r\n");
        status(L"Restore: ",db->UpdatePackageList(db,handle,original)); goto cleanup;
    }
    say(L"Temporary edits applied. No BIOS setting has been saved by this tool.\r\n");
    say(L"In Memory Configuration set Active Memory Timing Settings = Enabled.\r\n");
    say(L"Set Memory Target Speed = 3200 MT/s, then save in native BIOS.\r\n");
    say(L"After reboot, enter regular BIOS > AMD CBS > DDR Timing Configuration.\r\n");
    say(L"Enable Active Memory Timing Settings there again, confirm 3200, and save.\r\n");
    say(L"Leave RAM timings and voltages unchanged. Do not load defaults.\r\n");
    for(;;) {
        say(L"\r\nO = open OEM memory page; V = log current values; Q = return; R = undo menu edits\r\n");
        k=key();
        if(k=='o'||k=='O') {
            EFI_FORM_BROWSER2_PROTOCOL *browser=0; EFI_BROWSER_ACTION_REQUEST action=0;
            s=bs->LocateProtocol(&browser_guid,0,(VOID**)&browser);
            status(L"Original FormBrowser2: ",s);
            if(!EFI_ERROR(s)) {
                s=browser->SendForm(browser,&handle,1,(EFI_GUID*)setup_formset,0x279c,0,&action);
                status(L"Original browser returned: ",s); say(L"Browser action request: "); hex(action); say(L"\r\n");
                values();
            }
            say(L"If unsupported, use Q and select Enter Setup WITHOUT rebooting.\r\n");
        } else if(k=='v'||k=='V') {
            values();
        } else if(k=='q'||k=='Q') {
            say(L"Temporary menu edits kept for this boot. Select Enter Setup in the boot menu.\r\n"); break;
        } else if(k=='r'||k=='R') {
            status(L"Restore original top menu: ",rx16_memory_menu(ami->ImageBase,ami->ImageSize,-1)>0?EFI_SUCCESS:EFI_ABORTED);
            status(L"Restore original HII menu definitions: ",db->UpdatePackageList(db,handle,original));
            say(L"This restores menu definitions only; it does not undo settings you saved.\r\n"); break;
        }
    }
cleanup:
    bs->FreePool(modified); bs->FreePool(original);
finished:
    say(L"\r\nLog: RX16_RAM_LOG.txt on this USB. Press any key to return.\r\n"); key();
    if(logfile) logfile->Close(logfile);
    return EFI_SUCCESS;
}
