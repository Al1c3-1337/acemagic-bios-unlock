/* SPDX-License-Identifier: MIT
 * Run the actual application against in-memory EFI service doubles.
 * Never exposes real firmware, filesystem, settings, or reboot services.
 */
#include "../src/main.c"
static u8 arena[0x400000],package_data[0x40000];
static UINTN arena_pos,package_size,key_pos;
static const char *keys;
static int fault,updates,opens,writes,routes,last_form;
static EFI_LOADED_IMAGE_PROTOCOL loaded[2];
static u8 device_paths[2][24];
static EFI_HANDLE image_handles[2]={(VOID*)1,(VOID*)2};
static EFI_HII_DATABASE_PROTOCOL database;
static EFI_FORM_BROWSER2_PROTOCOL form_browser;
static EFI_HII_CONFIG_ACCESS_PROTOCOL config_access;
static EFI_SYSTEM_TABLE mock_st;
static EFI_BOOT_SERVICES mock_bs;
static EFI_RUNTIME_SERVICES mock_rt;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL input;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL output;
static EFI_STATUS EFIAPI print_text(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self,CHAR16 *s) {
    (void)self;(void)s;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI read_key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *self,EFI_INPUT_KEY *k) {
    (void)self;k->ScanCode=0;k->UnicodeChar=keys[key_pos]?keys[key_pos++]:'q';return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI allocate(EFI_MEMORY_TYPE type,UINTN size,VOID **out) {
    (void)type;size=(size+15)&~(UINTN)15;
    if(size>sizeof(arena)-arena_pos)return EFI_OUT_OF_RESOURCES;
    *out=arena+arena_pos;arena_pos+=size;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pool(VOID *p) {(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI read_var(CHAR16 *name,EFI_GUID *guid,UINT32 *attrs,UINTN *size,VOID *data) {
    (void)guid;UINTN needed=(name[0]=='S')?0xc9:0x67f;
    if(*size<needed){*size=needed;return EFI_BUFFER_TOO_SMALL;}
    memset(data,0,needed);*size=needed;if(attrs)*attrs=7;
    if(needed==0xc9){((u8*)data)[0xaf]=0xff;((u8*)data)[0xb0]=0xc0;((u8*)data)[0xb1]=0x12;}
    else {((u8*)data)[0x5e]=0xff;((u8*)data)[0x5f]=0x80;((u8*)data)[0x60]=0x0c;}
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI write_var(CHAR16 *name,EFI_GUID *guid,UINT32 attrs,UINTN size,VOID *data) {
    (void)name;(void)guid;(void)attrs;(void)size;(void)data;writes++;return EFI_ACCESS_DENIED;
}
static EFI_STATUS EFIAPI route(const EFI_HII_CONFIG_ACCESS_PROTOCOL *self,const EFI_STRING conf,EFI_STRING *progress) {
    (void)self;(void)conf;(void)progress;routes++;return EFI_ACCESS_DENIED;
}
static EFI_STATUS EFIAPI handle_protocol(EFI_HANDLE handle,EFI_GUID *guid,VOID **out) {
    if(equal(guid,&loaded_guid,16)&&(handle==(VOID*)1||handle==(VOID*)2)) {
        *out=&loaded[handle==(VOID*)1?0:1];return EFI_SUCCESS;
    }
    if(handle==(VOID*)3&&equal(guid,&config_guid,16)){*out=&config_access;return EFI_SUCCESS;}
    return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI locate_handles(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *guid,VOID *key_arg,UINTN *n,EFI_HANDLE **handles) {
    (void)type;(void)key_arg;
    if(!equal(guid,&loaded_guid,16))return EFI_NOT_FOUND;
    *n=2;*handles=image_handles;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI locate_protocol(EFI_GUID *guid,VOID *registration,VOID **out) {
    (void)registration;
    if(equal(guid,&hii_guid,16)){*out=&database;return EFI_SUCCESS;}
    if(equal(guid,&browser_guid,16)){*out=&form_browser;return EFI_SUCCESS;}
    return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI list_packages(const EFI_HII_DATABASE_PROTOCOL *self,UINT8 type,const EFI_GUID *guid,UINTN *size,EFI_HII_HANDLE *handles) {
    (void)self;(void)type;(void)guid;
    if(*size<sizeof(*handles)){*size=sizeof(*handles);return EFI_BUFFER_TOO_SMALL;}
    *size=sizeof(*handles);*handles=(VOID*)4;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI export_packages(const EFI_HII_DATABASE_PROTOCOL *self,EFI_HII_HANDLE handle,UINTN *size,EFI_HII_PACKAGE_LIST_HEADER *data) {
    (void)self;(void)handle;
    if(*size<package_size){*size=package_size;return EFI_BUFFER_TOO_SMALL;}
    *size=package_size;memcpy(data,package_data,package_size);
    if(fault==2&&updates==1)((u8*)data)[0]^=1;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI update_packages(const EFI_HII_DATABASE_PROTOCOL *self,EFI_HII_HANDLE handle,const EFI_HII_PACKAGE_LIST_HEADER *data) {
    (void)self;(void)handle;updates++;
    if(data->PackageLength>sizeof(package_data))return EFI_BAD_BUFFER_SIZE;
    package_size=data->PackageLength;memcpy(package_data,data,package_size);
    if(fault==1&&updates==1)return EFI_ABORTED;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI owner(const EFI_HII_DATABASE_PROTOCOL *self,EFI_HII_HANDLE handle,EFI_HANDLE *out) {
    (void)self;(void)handle;*out=(fault==3&&updates==1)?(VOID*)9:(VOID*)3;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI send_form(const EFI_FORM_BROWSER2_PROTOCOL *self,EFI_HII_HANDLE *handles,UINTN count,EFI_GUID *guid,EFI_FORM_ID form,const EFI_SCREEN_DESCRIPTOR *dimensions,EFI_BROWSER_ACTION_REQUEST *action) {
    (void)self;(void)handles;(void)count;(void)guid;(void)dimensions;
    opens++;last_form=form;*action=0;return EFI_SUCCESS;
}
API int mock_run(u8 *setup,u64 setup_size,u8 *ami,u64 ami_size,const u8 *hii,u64 hii_size,const char *sequence,int failure,int *result) {
    if(hii_size>sizeof(package_data))return -1;
    arena_pos=key_pos=0;keys=sequence;fault=failure;updates=opens=writes=routes=last_form=0;
    logfile=0;package_size=hii_size;memcpy(package_data,hii,hii_size);
    memset(&mock_st,0,sizeof(mock_st));memset(&mock_bs,0,sizeof(mock_bs));memset(&mock_rt,0,sizeof(mock_rt));
    memset(loaded,0,sizeof(loaded));memset(device_paths,0,sizeof(device_paths));
    for(unsigned i=0;i<2;i++) {
        device_paths[i][0]=MEDIA_DEVICE_PATH;device_paths[i][1]=MEDIA_PIWG_FW_FILE_DP;device_paths[i][2]=20;
        memcpy(device_paths[i]+4,i?&amitse_file:&setup_file,16);
        device_paths[i][20]=END_DEVICE_PATH_TYPE;device_paths[i][21]=END_ENTIRE_DEVICE_PATH_SUBTYPE;device_paths[i][22]=4;
        loaded[i].FilePath=(VOID*)device_paths[i];loaded[i].ImageBase=i?ami:setup;loaded[i].ImageSize=i?ami_size:setup_size;
    }
    input.ReadKeyStroke=read_key;output.OutputString=print_text;
    mock_st.ConIn=&input;mock_st.ConOut=&output;mock_st.BootServices=&mock_bs;mock_st.RuntimeServices=&mock_rt;
    mock_bs.AllocatePool=allocate;mock_bs.FreePool=free_pool;mock_bs.HandleProtocol=handle_protocol;
    mock_bs.LocateHandleBuffer=locate_handles;mock_bs.LocateProtocol=locate_protocol;
    mock_rt.GetVariable=read_var;mock_rt.SetVariable=write_var;
    database.ListPackageLists=list_packages;database.ExportPackageLists=export_packages;
    database.UpdatePackageList=update_packages;database.GetPackageListHandle=owner;
    config_access.RouteConfig=route;form_browser.SendForm=send_form;
    efi_main((VOID*)5,&mock_st);
    result[0]=updates;result[1]=opens;result[2]=last_form;result[3]=writes;result[4]=routes;
    result[5]=(package_size==hii_size&&equal(package_data,hii,hii_size));
    return 0;
}
