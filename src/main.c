/* SPDX-License-Identifier: MIT */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/SimpleFileSystem.h>
#include "toolkit_core.h"
#include "version.h"

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
    if(!EFI_ERROR(root->Open(root,&old,L"RX16_TOOLKIT_LOG.txt",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0))) old->Delete(old);
    root->Open(root,&logfile,L"RX16_TOOLKIT_LOG.txt",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
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
static EFI_STATUS findforms(EFI_HII_DATABASE_PROTOCOL *db,int mode,EFI_HII_HANDLE *handle,VOID **original,UINTN *size) {
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
        if(!EFI_ERROR(s) && rx16_prepare(mode,buf,len,0,0,&target_size)==1) {
            matches++; if(matches==1) { *handle=handles[i]; *original=buf; *size=len; } else bs->FreePool(buf);
        } else bs->FreePool(buf);
    }
    bs->FreePool(handles);
    if(matches!=1) { if(*original) { bs->FreePool(*original); *original=0; } return EFI_NOT_FOUND; }
    return EFI_SUCCESS;
}
static int verify_package(EFI_HII_DATABASE_PROTOCOL *db,EFI_HII_HANDLE handle,
                          EFI_HANDLE owner,const VOID *expected,UINTN expected_size) {
    VOID *readback=0; UINTN size=0; EFI_HANDLE current_owner=0; int ok=0;
    EFI_STATUS s=db->ExportPackageLists(db,handle,&size,0);
    if(s==EFI_BUFFER_TOO_SMALL && size==expected_size
       && !EFI_ERROR(bs->AllocatePool(EfiBootServicesData,size,&readback))) {
        s=db->ExportPackageLists(db,handle,&size,readback);
        if(!EFI_ERROR(s)&&size==expected_size&&equal(readback,expected,size)
           && !EFI_ERROR(db->GetPackageListHandle(db,handle,&current_owner))
           && current_owner==owner) ok=1;
        bs->FreePool(readback);
    }
    return ok;
}
static int restore_package(EFI_HII_DATABASE_PROTOCOL *db,EFI_HII_HANDLE handle,
                           EFI_HANDLE owner,VOID *original,UINTN size) {
    EFI_STATUS s=db->UpdatePackageList(db,handle,original);
    status(L"Restore original menu definitions: ",s);
    if(EFI_ERROR(s)||!verify_package(db,handle,owner,original,size)) {
        say(L"Restore did not verify. Reboot before entering BIOS or running a mode again.\r\n");
        return 0;
    }
    return 1;
}
static void instructions(int mode) {
    if(mode==RX16_UNLOCK) {
        say(L"In Main, set Setup Item Hide Control = Disabled. Save and reboot.\r\n");
        say(L"Check the advanced menus in regular BIOS without this USB.\r\n");
    } else {
        say(L"In Memory Configuration, set Active Memory Timing Settings = Enabled.\r\n");
        say(L"For the tested underclock, set Memory Target Speed = 3200 MT/s.\r\n");
        say(L"Save and reboot. Then enter regular BIOS > AMD CBS > UMC Common Options\r\n");
        say(L"> DDR Options > DDR Timing Configuration > Accept. Enable Active Memory\r\n");
        say(L"Timing Settings there again, confirm 3200, then save again.\r\n");
        say(L"Leave timings and voltages unchanged. Do not load defaults.\r\n");
    }
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE self,EFI_SYSTEM_TABLE *table) {
    st=table; bs=table->BootServices; initlog(self);
    say(L"\r\nRX16 Toolkit " RX16_VERSION_W L"\r\n");
    say(L"ACEMAGIC RX16 / AKN79C_H / A-004 firmware only.\r\n");
    say(L"Temporary menu changes; save settings yourself in the original BIOS.\r\n");
    say(L"RAM settings can prevent boot. Removing the USB does not undo saved settings.\r\n\r\n");
    if(!logfile) say(L"USB log unavailable. Photograph any error shown here.\r\n");
    values();
    EFI_LOADED_IMAGE_PROTOCOL *setup=0,*ami=0;
    EFI_STATUS s=images(&setup,&ami);
    status(L"Exact Setup and AMITSE code fingerprints: ",s);
    if(EFI_ERROR(s)) goto finished;
    int mode=0; CHAR16 k;
    for(;;) {
        say(L"\r\nU = unlock advanced menus\r\nM = RAM controls (unlock menus first)\r\n");
        say(L"D = read saved settings again\r\nQ = exit without changes\r\n");
        k=key();
        if(k=='u'||k=='U') { mode=RX16_UNLOCK; break; }
        if(k=='m'||k=='M') { mode=RX16_MEMORY; break; }
        if(k=='d'||k=='D') values();
        if(k=='q'||k=='Q') goto finished;
    }
    status(L"Selected mode: ",(EFI_STATUS)mode);
    if(rx16_tables(mode,ami->ImageBase,ami->ImageSize,0)!=1) {
        say(L"Unexpected menu state. Reboot before running another tool or mode.\r\n"); goto finished;
    }
    EFI_HII_DATABASE_PROTOCOL *db=0;
    s=bs->LocateProtocol(&hii_guid,0,(VOID**)&db);
    status(L"HII database: ",s); if(EFI_ERROR(s)) goto finished;
    EFI_HII_HANDLE handle=0; VOID *original=0,*modified=0; UINTN size=0;
    s=findforms(db,mode,&handle,&original,&size);
    status(L"Exactly one matching original form package: ",s);
    if(EFI_ERROR(s)) goto finished;
    EFI_HANDLE owner=0;
    s=db->GetPackageListHandle(db,handle,&owner);
    status(L"Existing configuration owner: ",s);
    if(EFI_ERROR(s)||!owner) goto cleanup;
    EFI_HII_CONFIG_ACCESS_PROTOCOL *access=0;
    s=bs->HandleProtocol(owner,&config_guid,(VOID**)&access);
    status(L"Original configuration handler: ",s);
    if(EFI_ERROR(s)||!access||!access->RouteConfig) goto cleanup;
    s=bs->AllocatePool(EfiBootServicesData,size,&modified);
    status(L"Allocate temporary menu copy: ",s);
    if(EFI_ERROR(s)) goto cleanup;
    u64 modified_size=0;
    if(rx16_prepare(mode,original,size,modified,size,&modified_size)!=1) {
        say(L"Menu validation failed; stopped before applying changes.\r\n"); goto cleanup;
    }
    s=db->UpdatePackageList(db,handle,modified);
    status(L"Update existing menu definitions: ",s);
    if(EFI_ERROR(s)||!verify_package(db,handle,owner,modified,(UINTN)modified_size)) {
        say(L"Update/read-back failed. Restoring original definitions.\r\n");
        restore_package(db,handle,owner,original,size); goto cleanup;
    }
    if(rx16_tables(mode,ami->ImageBase,ami->ImageSize,1)!=1) {
        say(L"Menu-table validation changed. Restoring original definitions.\r\n");
        restore_package(db,handle,owner,original,size); goto cleanup;
    }
    say(L"Menu changes verified. No BIOS settings have been saved by the tool.\r\n");
    instructions(mode);
    for(;;) {
        say(L"\r\nO = open original BIOS; D = read settings; Q = return; R = undo menus\r\n");
        k=key();
        if(k=='o'||k=='O') {
            EFI_FORM_BROWSER2_PROTOCOL *browser=0; EFI_BROWSER_ACTION_REQUEST action=0;
            s=bs->LocateProtocol(&browser_guid,0,(VOID**)&browser);
            status(L"Original form browser: ",s);
            if(!EFI_ERROR(s)) {
                UINT16 form=(mode==RX16_UNLOCK)?0x2711:0x279c;
                s=browser->SendForm(browser,&handle,1,(EFI_GUID*)setup_formset,form,0,&action);
                status(L"Original browser returned: ",s);
                status(L"Browser action request: ",action); values();
            }
            say(L"If the page did not open, use Q then Enter Setup in the SAME boot.\r\n");
        } else if(k=='d'||k=='D') {
            values();
        } else if(k=='q'||k=='Q') {
            say(L"Temporary menus remain for this boot. Reboot before changing modes.\r\n"); break;
        } else if(k=='r'||k=='R') {
            int restored=rx16_tables(mode,ami->ImageBase,ami->ImageSize,-1);
            status(L"Restore original menu table: ",restored>0?EFI_SUCCESS:EFI_ABORTED);
            restore_package(db,handle,owner,original,size);
            say(L"Only menu definitions were restored; settings you saved remain.\r\n"); break;
        }
    }
cleanup:
    if(modified) bs->FreePool(modified);
    if(original) bs->FreePool(original);
finished:
    say(L"\r\nLog: RX16_TOOLKIT_LOG.txt on this USB. Press any key to return.\r\n"); key();
    if(logfile) logfile->Close(logfile);
    return EFI_SUCCESS;
}
