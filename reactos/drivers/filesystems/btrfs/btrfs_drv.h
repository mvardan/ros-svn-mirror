/* Copyright (c) Mark Harmstone 2016
 * 
 * This file is part of WinBtrfs.
 * 
 * WinBtrfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public Licence as published by
 * the Free Software Foundation, either version 3 of the Licence, or
 * (at your option) any later version.
 * 
 * WinBtrfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public Licence for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public Licence
 * along with WinBtrfs.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef BTRFS_DRV_H_DEFINED
#define BTRFS_DRV_H_DEFINED

#ifndef __REACTOS__
#undef _WIN32_WINNT
#undef NTDDI_VERSION

#define _WIN32_WINNT 0x0600
#define NTDDI_VERSION 0x06010000 // Win 7
#define _CRT_SECURE_NO_WARNINGS
#endif /* __REACTOS__ */

#include <ntifs.h>
#include <ntddk.h>
#ifdef __REACTOS__
#include <ntdddisk.h>
#endif /* __REACTOS__ */
#include <mountmgr.h>
#ifdef __REACTOS__
#include <rtlfuncs.h>
#include <iotypes.h>
#include <pseh/pseh2.h>
#endif /* __REACTOS__ */
//#include <windows.h>
#include <windef.h>
#include <wdm.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include "btrfs.h"

#ifdef _DEBUG
// #define DEBUG_FCB_REFCOUNTS
// #define DEBUG_LONG_MESSAGES
#define DEBUG_PARANOID
#endif

#define BTRFS_NODE_TYPE_CCB 0x2295
#define BTRFS_NODE_TYPE_FCB 0x2296

#define ALLOC_TAG 0x7442484D //'MHBt'

#define STDCALL __stdcall

#define UID_NOBODY 65534
#define GID_NOBODY 65534

#define EA_NTACL "security.NTACL"
#define EA_NTACL_HASH 0x45922146

#define EA_DOSATTRIB "user.DOSATTRIB"
#define EA_DOSATTRIB_HASH 0x914f9939

#define EA_REPARSE "system.reparse"
#define EA_REPARSE_HASH 0x786f6167

#define READ_AHEAD_GRANULARITY 0x10000 // 64 KB

#define MAX_EXTENT_SIZE 0x8000000 // 128 MB

#ifdef _MSC_VER
#define try __try
#define except __except
#define finally __finally
#else
#define try if (1)
#define except(x) if (0 && (x))
#define finally if (1)
#endif

// #pragma pack(push, 1)

struct device_extension;

typedef struct {
    BTRFS_UUID fsuuid;
    BTRFS_UUID devuuid;
    UINT64 devnum;
    UNICODE_STRING devpath;
    UINT64 length;
    UINT64 gen1, gen2;
    BOOL processed;
    LIST_ENTRY list_entry;
} volume;

typedef struct _fcb_nonpaged {
    FAST_MUTEX HeaderMutex;
    SECTION_OBJECT_POINTERS segment_object;
    ERESOURCE resource;
    ERESOURCE paging_resource;
    ERESOURCE index_lock;
} fcb_nonpaged;

struct _root;

typedef struct {
    UINT64 offset;
    EXTENT_DATA* data;
    ULONG datalen;
    BOOL unique;
    BOOL ignore;
    
    LIST_ENTRY list_entry;
} extent;

typedef struct {
    UINT32 hash;
    KEY key;
    UINT8 type;
    UINT64 index;
    ANSI_STRING utf8;
    UNICODE_STRING filepart_uc;

    LIST_ENTRY list_entry;
} index_entry;

typedef struct {
    UINT64 parent;
    UINT64 index;
    UNICODE_STRING name;
    ANSI_STRING utf8;
    LIST_ENTRY list_entry;
} hardlink;

struct _file_ref;

typedef struct _fcb {
    FSRTL_ADVANCED_FCB_HEADER Header;
    struct _fcb_nonpaged* nonpaged;
    LONG refcount;
    LONG open_count;
    struct _device_extension* Vcb;
    struct _root* subvol;
    UINT64 inode;
    UINT8 type;
    INODE_ITEM inode_item;
    SECURITY_DESCRIPTOR* sd;
    FILE_LOCK lock;
    BOOL deleted;
    PKTHREAD lazy_writer_thread;
    ULONG atts;
    SHARE_ACCESS share_access;
    WCHAR* debug_desc;
    LIST_ENTRY extents;
    UINT64 last_dir_index;
    ANSI_STRING reparse_xattr;
    LIST_ENTRY hardlinks;
    struct _file_ref* fileref;
    
    BOOL index_loaded;
    LIST_ENTRY index_list;
    
    BOOL dirty;
    BOOL sd_dirty;
    BOOL atts_changed, atts_deleted;
    BOOL extents_changed;
    BOOL reparse_xattr_changed;
    BOOL created;
    
    BOOL ads;
    UINT32 adshash;
    ANSI_STRING adsxattr;
    ANSI_STRING adsdata;
    
    LIST_ENTRY list_entry;
    LIST_ENTRY list_entry_all;
} fcb;

typedef struct {
    fcb* fcb;
    LIST_ENTRY list_entry;
} dirty_fcb;

typedef struct {
    ERESOURCE children_lock;
} file_ref_nonpaged;

typedef struct _file_ref {
    fcb* fcb;
    UNICODE_STRING filepart;
    UNICODE_STRING filepart_uc;
    ANSI_STRING utf8;
    ANSI_STRING oldutf8;
    UINT64 index;
    BOOL delete_on_close;
    BOOL deleted;
    BOOL created;
    file_ref_nonpaged* nonpaged;
    LIST_ENTRY children;
    LONG refcount;
    struct _file_ref* parent;
    WCHAR* debug_desc;
    
    BOOL dirty;
    
    LIST_ENTRY list_entry;
} file_ref;

typedef struct {
    file_ref* fileref;
    LIST_ENTRY list_entry;
} dirty_fileref;

typedef struct _ccb {
    USHORT NodeType;
    CSHORT NodeSize;
    ULONG disposition;
    ULONG options;
    UINT64 query_dir_offset;
//     char* query_string;
    UNICODE_STRING query_string;
    BOOL has_wildcard;
    BOOL specific_file;
    ACCESS_MASK access;
    file_ref* fileref;
    UNICODE_STRING filename;
} ccb;

// typedef struct _log_to_phys {
//     UINT64 address;
//     UINT64 size;
//     UINT64 physaddr;
//     UINT32 sector_size;
//     struct _log_to_phys* next;
// } log_to_phys;

struct _device_extension;

// enum tree_holder_status {
//     tree_holder_unloaded,
//     tree_holder_loading,
//     tree_holder_loaded,
//     tree_holder_unloading
// };

// typedef struct {
//     enum tree_holder_status status;
//     KSPIN_LOCK spin_lock;
//     ERESOURCE lock;
// } tree_holder_nonpaged;

typedef struct {
    UINT64 address;
    UINT64 generation;
    struct _tree* tree;
//     tree_holder_nonpaged* nonpaged;
} tree_holder;

typedef struct _tree_data {
    KEY key;
    LIST_ENTRY list_entry;
    BOOL ignore;
    BOOL inserted;
    
    union {
        tree_holder treeholder;
        
        struct {
            UINT32 size;
            UINT8* data;
        };
    };
} tree_data;

// typedef struct _tree_nonpaged {
//     ERESOURCE load_tree_lock;
// } tree_nonpaged;

typedef struct _tree {
//     UINT64 address;
//     UINT8 level;
    tree_header header;
    BOOL has_address;
    UINT32 size;
    struct _device_extension* Vcb;
    struct _tree* parent;
    tree_data* paritem;
    struct _root* root;
//     tree_nonpaged* nonpaged;
    LIST_ENTRY itemlist;
    LIST_ENTRY list_entry;
    UINT64 new_address;
    BOOL has_new_address;
    UINT64 flags;
    BOOL write;
} tree;

typedef struct {
//     KSPIN_LOCK load_tree_lock;
    ERESOURCE load_tree_lock;
} root_nonpaged;

typedef struct _root {
    UINT64 id;
    tree_holder treeholder;
    root_nonpaged* nonpaged;
    UINT64 lastinode;
    ROOT_ITEM root_item;
    UNICODE_STRING path;
    LIST_ENTRY fcbs;
    LIST_ENTRY list_entry;
} root;

typedef struct {
    tree* tree;
    tree_data* item;
} traverse_ptr;

typedef struct _root_cache {
    root* root;
    struct _root_cache* next;
} root_cache;

typedef struct {
    UINT64 address;
    UINT64 size;
    LIST_ENTRY list_entry;
    LIST_ENTRY list_entry_size;
} space;

typedef struct {
    PDEVICE_OBJECT devobj;
    DEV_ITEM devitem;
    BOOL removable;
    ULONG change_count;
    UINT64 length;
    LIST_ENTRY space;
} device;

typedef struct {
    ERESOURCE lock;
    ERESOURCE changed_extents_lock;
} chunk_nonpaged;

typedef struct {
    CHUNK_ITEM* chunk_item;
    UINT32 size;
    UINT64 offset;
    UINT64 used;
    UINT32 oldused;
    device** devices;
    fcb* cache;
    LIST_ENTRY space;
    LIST_ENTRY space_size;
    LIST_ENTRY deleting;
    LIST_ENTRY changed_extents;
    chunk_nonpaged* nonpaged;
    BOOL created;
    
    LIST_ENTRY list_entry;
    LIST_ENTRY list_entry_changed;
} chunk;

typedef struct {
    UINT64 address;
    UINT64 size;
    UINT64 old_size;
    UINT64 count;
    UINT64 old_count;
    BOOL no_csum;
    LIST_ENTRY refs;
    LIST_ENTRY old_refs;
    LIST_ENTRY list_entry;
} changed_extent;

typedef struct {
    EXTENT_DATA_REF edr;
    LIST_ENTRY list_entry;
} changed_extent_ref;

typedef struct {
    UINT64 address;
    UINT64 size;
    EXTENT_DATA_REF edr;
    LIST_ENTRY list_entry;
} shared_data_entry;

typedef struct {
    UINT64 address;
    UINT64 parent;
    LIST_ENTRY entries;
    LIST_ENTRY list_entry;
} shared_data;

typedef struct {
    KEY key;
    void* data;
    USHORT size;
    LIST_ENTRY list_entry;
} sys_chunk;

typedef struct {
    PIRP Irp;
    LIST_ENTRY list_entry;
} thread_job;

typedef struct {
    PDEVICE_OBJECT DeviceObject;
    HANDLE handle;
    KEVENT event, finished;
    BOOL quit;
    LIST_ENTRY jobs;
    KSPIN_LOCK spin_lock;
} drv_thread;

typedef struct {
    ULONG num_threads;
    LONG next_thread;
    drv_thread* threads;
} drv_threads;

typedef struct {
    BOOL ignore;
} mount_options;

#define VCB_TYPE_VOLUME     1
#define VCB_TYPE_PARTITION0 2

typedef struct _device_extension {
    UINT32 type;
    mount_options options;
    PVPB Vpb;
    device* devices;
    UINT64 devices_loaded;
//     DISK_GEOMETRY geometry;
    superblock superblock;
//     WCHAR label[MAX_LABEL_SIZE];
    BOOL readonly;
    BOOL removing;
    BOOL locked;
    PFILE_OBJECT locked_fileobj;
    fcb* volume_fcb;
    file_ref* root_fileref;
    ERESOURCE DirResource;
    KSPIN_LOCK FcbListLock;
    ERESOURCE fcb_lock;
    ERESOURCE load_lock;
    ERESOURCE tree_lock;
    PNOTIFY_SYNC NotifySync;
    LIST_ENTRY DirNotifyList;
    LONG open_trees;
    BOOL need_write;
//     ERESOURCE LogToPhysLock;
//     UINT64 chunk_root_phys_addr;
    UINT64 root_tree_phys_addr;
    UINT64 data_flags;
//     log_to_phys* log_to_phys;
    LIST_ENTRY roots;
    LIST_ENTRY drop_roots;
    root* chunk_root;
    root* root_root;
    root* extent_root;
    root* checksum_root;
    root* dev_root;
    root* uuid_root;
    BOOL log_to_phys_loaded;
    UINT32 max_inline;
    LIST_ENTRY sys_chunks;
    LIST_ENTRY chunks;
    LIST_ENTRY chunks_changed;
    LIST_ENTRY trees;
    LIST_ENTRY all_fcbs;
    LIST_ENTRY dirty_fcbs;
    KSPIN_LOCK dirty_fcbs_lock;
    LIST_ENTRY dirty_filerefs;
    KSPIN_LOCK dirty_filerefs_lock;
    ERESOURCE checksum_lock;
    ERESOURCE chunk_lock;
    LIST_ENTRY sector_checksums;
    LIST_ENTRY shared_extents;
    KSPIN_LOCK shared_extents_lock;
    HANDLE flush_thread_handle;
    KTIMER flush_thread_timer;
    KEVENT flush_thread_finished;
    drv_threads threads;
    PFILE_OBJECT root_file;
    LIST_ENTRY list_entry;
} device_extension;

typedef struct {
    UINT32 type;
    PDEVICE_OBJECT devobj;
    BTRFS_UUID uuid;
    UNICODE_STRING name;
} part0_device_extension;

typedef struct {
    LIST_ENTRY listentry;
    PSID sid;
    UINT32 uid;
} uid_map;

typedef struct {
    LIST_ENTRY list_entry;
    UINT64 key;
} ordered_list;

typedef struct {
    ordered_list ol;
    ULONG length;
    UINT32* checksums;
    BOOL deleted;
} changed_sector;

enum write_data_status {
    WriteDataStatus_Pending,
    WriteDataStatus_Success,
    WriteDataStatus_Error,
    WriteDataStatus_Cancelling,
    WriteDataStatus_Cancelled,
    WriteDataStatus_Ignore
};

struct write_data_context;

typedef struct {
    struct write_data_context* context;
    UINT8* buf;
    BOOL need_free;
    device* device;
    PIRP Irp;
    IO_STATUS_BLOCK iosb;
    enum write_data_status status;
    LIST_ENTRY list_entry;
} write_data_stripe;

typedef struct {
    KEVENT Event;
    LIST_ENTRY stripes;
    LONG stripes_left;
    BOOL tree;
} write_data_context;

// #pragma pack(pop)

static __inline void* map_user_buffer(PIRP Irp) {
    if (!Irp->MdlAddress) {
        return Irp->UserBuffer;
    } else {
        return MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    }
}

static __inline UINT64 unix_time_to_win(BTRFS_TIME* t) {
    return (t->seconds * 10000000) + (t->nanoseconds / 100) + 116444736000000000;
}

static __inline void win_time_to_unix(LARGE_INTEGER t, BTRFS_TIME* out) {
    ULONGLONG l = t.QuadPart - 116444736000000000;
    
    out->seconds = l / 10000000;
    out->nanoseconds = (l % 10000000) * 100;
}

static __inline void insert_into_ordered_list(LIST_ENTRY* list, ordered_list* ins) {
    LIST_ENTRY* le = list->Flink;
    ordered_list* ol;
    
    while (le != list) {
        ol = (ordered_list*)le;
        
        if (ol->key > ins->key) {
            le->Blink->Flink = &ins->list_entry;
            ins->list_entry.Blink = le->Blink;
            le->Blink = &ins->list_entry;
            ins->list_entry.Flink = le;
            return;
        }
        
        le = le->Flink;
    }
    
    InsertTailList(list, &ins->list_entry);
}

static __inline void get_raid0_offset(UINT64 off, UINT64 stripe_length, UINT16 num_stripes, UINT64* stripeoff, UINT16* stripe) {
    UINT64 initoff, startoff;
    
    startoff = off % (num_stripes * stripe_length);
    initoff = (off / (num_stripes * stripe_length)) * stripe_length;
    
    *stripe = startoff / stripe_length;
    *stripeoff = initoff + startoff - (*stripe * stripe_length);
}

// in btrfs.c
device* find_device_from_uuid(device_extension* Vcb, BTRFS_UUID* uuid);
UINT64 sector_align( UINT64 NumberToBeAligned, UINT64 Alignment );
int keycmp(const KEY* key1, const KEY* key2);
ULONG STDCALL get_file_attributes(device_extension* Vcb, INODE_ITEM* ii, root* r, UINT64 inode, UINT8 type, BOOL dotfile, BOOL ignore_xa);
BOOL STDCALL get_xattr(device_extension* Vcb, root* subvol, UINT64 inode, char* name, UINT32 crc32, UINT8** data, UINT16* datalen);
void _free_fcb(fcb* fcb, const char* func, const char* file, unsigned int line);
void _free_fileref(file_ref* fr, const char* func, const char* file, unsigned int line);
BOOL STDCALL get_last_inode(device_extension* Vcb, root* r);
NTSTATUS add_dir_item(device_extension* Vcb, root* subvol, UINT64 inode, UINT32 crc32, DIR_ITEM* di, ULONG disize, LIST_ENTRY* rollback);
NTSTATUS delete_dir_item(device_extension* Vcb, root* subvol, UINT64 parinode, UINT32 crc32, PANSI_STRING utf8, LIST_ENTRY* rollback);
NTSTATUS delete_inode_ref(device_extension* Vcb, root* subvol, UINT64 inode, UINT64 parinode, PANSI_STRING utf8, LIST_ENTRY* rollback);
fcb* create_fcb();
file_ref* create_fileref();
void protect_superblocks(device_extension* Vcb, chunk* c);
BOOL is_top_level(PIRP Irp);
NTSTATUS create_root(device_extension* Vcb, UINT64 id, root** rootptr, BOOL no_tree, UINT64 offset, LIST_ENTRY* rollback);
void STDCALL uninit(device_extension* Vcb, BOOL flush);
NTSTATUS STDCALL dev_ioctl(PDEVICE_OBJECT DeviceObject, ULONG ControlCode, PVOID InputBuffer,
                           ULONG InputBufferSize, PVOID OutputBuffer, ULONG OutputBufferSize, BOOLEAN Override, IO_STATUS_BLOCK* iosb);
BOOL is_file_name_valid(PUNICODE_STRING us);
void send_notification_fileref(file_ref* fileref, ULONG filter_match, ULONG action);
void send_notification_fcb(file_ref* fileref, ULONG filter_match, ULONG action);
WCHAR* file_desc(PFILE_OBJECT FileObject);
WCHAR* file_desc_fileref(file_ref* fileref);
BOOL add_thread_job(device_extension* Vcb, PIRP Irp);
NTSTATUS part0_passthrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void mark_fcb_dirty(fcb* fcb);
void mark_fileref_dirty(file_ref* fileref);
NTSTATUS delete_fileref(file_ref* fileref, PFILE_OBJECT FileObject, LIST_ENTRY* rollback);

#ifdef _MSC_VER
#define funcname __FUNCTION__
#else
#define funcname __func__
#endif

// FIXME - we probably shouldn't be moving funcname etc. around if we're not printing debug messages
#define free_fcb(fcb) _free_fcb(fcb, funcname, __FILE__, __LINE__)
#define free_fileref(fileref) _free_fileref(fileref, funcname, __FILE__, __LINE__)

#ifdef _DEBUG

extern BOOL log_started;
extern UINT32 debug_log_level;

#ifdef DEBUG_LONG_MESSAGES

#define MSG(fn, file, line, s, level, ...) (!log_started || level <= debug_log_level) ? _debug_message(fn, file, line, s, ##__VA_ARGS__) : 0

#define TRACE(s, ...) MSG(funcname, __FILE__, __LINE__, s, 3, ##__VA_ARGS__)
#define WARN(s, ...) MSG(funcname, __FILE__, __LINE__, s, 2, ##__VA_ARGS__)
#define FIXME(s, ...) MSG(funcname, __FILE__, __LINE__, s, 1, ##__VA_ARGS__)
#define ERR(s, ...) MSG(funcname, __FILE__, __LINE__, s, 1, ##__VA_ARGS__)

void STDCALL _debug_message(const char* func, const char* file, unsigned int line, char* s, ...);

#else

#define MSG(fn, s, level, ...) (!log_started || level <= debug_log_level) ? _debug_message(fn, s, ##__VA_ARGS__) : 0

#define TRACE(s, ...) MSG(funcname, s, 3, ##__VA_ARGS__)
#define WARN(s, ...) MSG(funcname, s, 2, ##__VA_ARGS__)
#define FIXME(s, ...) MSG(funcname, s, 1, ##__VA_ARGS__)
#define ERR(s, ...) MSG(funcname, s, 1, ##__VA_ARGS__)

void STDCALL _debug_message(const char* func, char* s, ...);

#endif

#else

#define TRACE(s, ...)
#define WARN(s, ...)
#ifndef __REACTOS__
#define FIXME(s, ...) DbgPrint("Btrfs FIXME : " funcname " : " s, ##__VA_ARGS__)
#define ERR(s, ...) DbgPrint("Btrfs ERR : " funcname " : " s, ##__VA_ARGS__)
#else
#define FIXME(s, ...) DbgPrint("Btrfs FIXME : %s : " s, funcname, ##__VA_ARGS__)
#define ERR(s, ...) DbgPrint("Btrfs ERR : %s : " s, funcname, ##__VA_ARGS__)
#endif

#endif

static __inline void increase_chunk_usage(chunk* c, UINT64 delta) {
    c->used += delta;
    
    TRACE("increasing size of chunk %llx by %llx\n", c->offset, delta);
}

// in fastio.c
void STDCALL init_fast_io_dispatch(FAST_IO_DISPATCH** fiod);

// in crc32c.c
UINT32 STDCALL calc_crc32c(UINT32 seed, UINT8* msg, ULONG msglen);

enum rollback_type {
    ROLLBACK_INSERT_ITEM,
    ROLLBACK_DELETE_ITEM,
    ROLLBACK_INSERT_EXTENT,
    ROLLBACK_DELETE_EXTENT
};

// in treefuncs.c
NTSTATUS STDCALL _find_item(device_extension* Vcb, root* r, traverse_ptr* tp, const KEY* searchkey, BOOL ignore, const char* func, const char* file, unsigned int line);
BOOL STDCALL _find_next_item(device_extension* Vcb, const traverse_ptr* tp, traverse_ptr* next_tp, BOOL ignore, const char* func, const char* file, unsigned int line);
BOOL STDCALL _find_prev_item(device_extension* Vcb, const traverse_ptr* tp, traverse_ptr* prev_tp, BOOL ignore, const char* func, const char* file, unsigned int line);
void STDCALL free_trees(device_extension* Vcb);
BOOL STDCALL insert_tree_item(device_extension* Vcb, root* r, UINT64 obj_id, UINT8 obj_type, UINT64 offset, void* data, UINT32 size, traverse_ptr* ptp, LIST_ENTRY* rollback);
void STDCALL delete_tree_item(device_extension* Vcb, traverse_ptr* tp, LIST_ENTRY* rollback);
tree* STDCALL _free_tree(tree* t, const char* func, const char* file, unsigned int line);
NTSTATUS STDCALL _load_tree(device_extension* Vcb, UINT64 addr, root* r, tree** pt, tree* parent, const char* func, const char* file, unsigned int line);
NTSTATUS STDCALL _do_load_tree(device_extension* Vcb, tree_holder* th, root* r, tree* t, tree_data* td, BOOL* loaded, const char* func, const char* file, unsigned int line);
void clear_rollback(LIST_ENTRY* rollback);
void do_rollback(device_extension* Vcb, LIST_ENTRY* rollback);
void free_trees_root(device_extension* Vcb, root* r);
void add_rollback(LIST_ENTRY* rollback, enum rollback_type type, void* ptr);

#define find_item(Vcb, r, tp, searchkey, ignore) _find_item(Vcb, r, tp, searchkey, ignore, funcname, __FILE__, __LINE__)
#define find_next_item(Vcb, tp, next_tp, ignore) _find_next_item(Vcb, tp, next_tp, ignore, funcname, __FILE__, __LINE__)
#define find_prev_item(Vcb, tp, prev_tp, ignore) _find_prev_item(Vcb, tp, prev_tp, ignore, funcname, __FILE__, __LINE__)
#define free_tree(t) _free_tree(t, funcname, __FILE__, __LINE__)
#define load_tree(t, addr, r, pt, parent) _load_tree(t, addr, r, pt, parent, funcname, __FILE__, __LINE__)
#define do_load_tree(Vcb, th, r, t, td, loaded) _do_load_tree(Vcb, th, r, t, td, loaded, funcname, __FILE__, __LINE__)  

// in search.c
void STDCALL look_for_vols(PDRIVER_OBJECT DriverObject, LIST_ENTRY* volumes);

// in cache.c
NTSTATUS STDCALL init_cache();
void STDCALL free_cache();
extern CACHE_MANAGER_CALLBACKS* cache_callbacks;

// in write.c
NTSTATUS STDCALL do_write(device_extension* Vcb, LIST_ENTRY* rollback);
NTSTATUS write_file(device_extension* Vcb, PIRP Irp, BOOL wait, BOOL deferred_write);
NTSTATUS write_file2(device_extension* Vcb, PIRP Irp, LARGE_INTEGER offset, void* buf, ULONG* length, BOOL paging_io, BOOL no_cache,
                     BOOL wait, BOOL deferred_write, LIST_ENTRY* rollback);
NTSTATUS truncate_file(fcb* fcb, UINT64 end, LIST_ENTRY* rollback);
NTSTATUS extend_file(fcb* fcb, file_ref* fileref, UINT64 end, BOOL prealloc, PIRP Irp, LIST_ENTRY* rollback);
NTSTATUS excise_extents(device_extension* Vcb, fcb* fcb, UINT64 start_data, UINT64 end_data, LIST_ENTRY* rollback);
void commit_checksum_changes(device_extension* Vcb, LIST_ENTRY* changed_sector_list);
NTSTATUS insert_sparse_extent(fcb* fcb, UINT64 start, UINT64 length, LIST_ENTRY* rollback);
chunk* get_chunk_from_address(device_extension* Vcb, UINT64 address);
chunk* alloc_chunk(device_extension* Vcb, UINT64 flags, LIST_ENTRY* rollback);
NTSTATUS STDCALL write_data(device_extension* Vcb, UINT64 address, void* data, BOOL need_free, UINT32 length, write_data_context* wtc, PIRP Irp);
NTSTATUS STDCALL write_data_complete(device_extension* Vcb, UINT64 address, void* data, UINT32 length, PIRP Irp);
void free_write_data_stripes(write_data_context* wtc);
NTSTATUS get_tree_new_address(device_extension* Vcb, tree* t, LIST_ENTRY* rollback);
NTSTATUS STDCALL drv_write(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void flush_fcb(fcb* fcb, BOOL cache, LIST_ENTRY* rollback);
BOOL insert_extent_chunk(device_extension* Vcb, fcb* fcb, chunk* c, UINT64 start_data, UINT64 length, BOOL prealloc, void* data, LIST_ENTRY* changed_sector_list,
                         PIRP Irp, LIST_ENTRY* rollback);
NTSTATUS do_nocow_write(device_extension* Vcb, fcb* fcb, UINT64 start_data, UINT64 end_data, void* data, LIST_ENTRY* changed_sector_list, PIRP Irp, LIST_ENTRY* rollback);
NTSTATUS insert_extent(device_extension* Vcb, fcb* fcb, UINT64 start_data, UINT64 length, void* data, LIST_ENTRY* changed_sector_list, PIRP Irp, LIST_ENTRY* rollback);
void remove_fcb_extent(extent* ext, LIST_ENTRY* rollback);
NTSTATUS update_changed_extent_ref(device_extension* Vcb, chunk* c, UINT64 address, UINT64 size, UINT64 root, UINT64 objid, UINT64 offset,
                                   signed long long count, BOOL no_csum, UINT64 new_size);

// in dirctrl.c
NTSTATUS STDCALL drv_directory_control(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

// in security.c
NTSTATUS STDCALL drv_query_security(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS STDCALL drv_set_security(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void fcb_get_sd(fcb* fcb, struct _fcb* parent);
// UINT32 STDCALL get_uid();
void add_user_mapping(WCHAR* sidstring, ULONG sidstringlength, UINT32 uid);
UINT32 sid_to_uid(PSID sid);
NTSTATUS fcb_get_new_sd(fcb* fcb, file_ref* parfileref, ACCESS_STATE* as);

// in fileinfo.c
NTSTATUS STDCALL drv_set_information(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS STDCALL drv_query_information(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS add_inode_ref(device_extension* Vcb, root* subvol, UINT64 inode, UINT64 parinode, UINT64 index, PANSI_STRING utf8, LIST_ENTRY* rollback);
BOOL has_open_children(file_ref* fileref);
NTSTATUS STDCALL stream_set_end_of_file_information(device_extension* Vcb, UINT64 end, fcb* fcb, file_ref* fileref, PFILE_OBJECT FileObject, BOOL advance_only, LIST_ENTRY* rollback);
NTSTATUS fileref_get_filename(file_ref* fileref, PUNICODE_STRING fn, USHORT* name_offset);
NTSTATUS open_fileref_by_inode(device_extension* Vcb, root* subvol, UINT64 inode, file_ref** pfr);

// in reparse.c
NTSTATUS get_reparse_point(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject, void* buffer, DWORD buflen, DWORD* retlen);
NTSTATUS set_reparse_point(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS delete_reparse_point(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// in create.c
NTSTATUS STDCALL drv_create(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS STDCALL find_file_in_dir_with_crc32(device_extension* Vcb, PUNICODE_STRING filename, UINT32 crc32, file_ref* fr, root** subvol,
                                             UINT64* inode, UINT8* type, UINT64* index, PANSI_STRING utf8);
NTSTATUS STDCALL find_file_in_dir(device_extension* Vcb, PUNICODE_STRING filename, file_ref* fr,
                                  root** subvol, UINT64* inode, UINT8* type, UINT64* index, PANSI_STRING utf8);
NTSTATUS open_fileref(device_extension* Vcb, file_ref** pfr, PUNICODE_STRING fnus, file_ref* related, BOOL parent, USHORT* unparsed);
NTSTATUS open_fcb(device_extension* Vcb, root* subvol, UINT64 inode, UINT8 type, PANSI_STRING utf8, fcb* parent, fcb** pfcb);
NTSTATUS open_fcb_stream(device_extension* Vcb, root* subvol, UINT64 inode, ANSI_STRING* xattr, UINT32 streamhash, fcb* parent, fcb** pfcb);
void insert_fileref_child(file_ref* parent, file_ref* child, BOOL do_lock);
NTSTATUS fcb_get_last_dir_index(fcb* fcb, UINT64* index);

// in fsctl.c
NTSTATUS fsctl_request(PDEVICE_OBJECT DeviceObject, PIRP Irp, UINT32 type, BOOL user);
void do_unlock_volume(device_extension* Vcb);

// in flushthread.c
void STDCALL flush_thread(void* context);

// in read.c
NTSTATUS STDCALL drv_read(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS STDCALL read_data(device_extension* Vcb, UINT64 addr, UINT32 length, UINT32* csum, BOOL is_tree, UINT8* buf, chunk** pc, PIRP Irp);
NTSTATUS STDCALL read_file(fcb* fcb, UINT8* data, UINT64 start, UINT64 length, ULONG* pbr, PIRP Irp);
NTSTATUS do_read(PIRP Irp, BOOL wait, ULONG* bytes_read);

// in pnp.c
NTSTATUS STDCALL drv_pnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// in free-space.c
NTSTATUS load_free_space_cache(device_extension* Vcb, chunk* c);
NTSTATUS clear_free_space_cache(device_extension* Vcb);
NTSTATUS allocate_cache(device_extension* Vcb, BOOL* changed, LIST_ENTRY* rollback);
NTSTATUS update_chunk_caches(device_extension* Vcb, LIST_ENTRY* rollback);
NTSTATUS add_space_entry(LIST_ENTRY* list, LIST_ENTRY* list_size, UINT64 offset, UINT64 size);
void _space_list_add(device_extension* Vcb, chunk* c, BOOL deleting, UINT64 address, UINT64 length, LIST_ENTRY* rollback, const char* func);
void _space_list_add2(LIST_ENTRY* list, LIST_ENTRY* list_size, UINT64 address, UINT64 length, LIST_ENTRY* rollback, const char* func);
void _space_list_subtract(device_extension* Vcb, chunk* c, BOOL deleting, UINT64 address, UINT64 length, LIST_ENTRY* rollback, const char* func);
void _space_list_subtract2(LIST_ENTRY* list, LIST_ENTRY* list_size, UINT64 address, UINT64 length, LIST_ENTRY* rollback, const char* func);

#define space_list_add(Vcb, c, deleting, address, length, rollback) _space_list_add(Vcb, c, deleting, address, length, rollback, funcname)
#define space_list_add2(list, list_size, address, length, rollback) _space_list_add2(list, list_size, address, length, rollback, funcname)
#define space_list_subtract(Vcb, c, deleting, address, length, rollback) _space_list_subtract(Vcb, c, deleting, address, length, rollback, funcname)
#define space_list_subtract2(list, list_size, address, length, rollback) _space_list_subtract2(list, list_size, address, length, rollback, funcname)

// in extent-tree.c
NTSTATUS increase_extent_refcount_data(device_extension* Vcb, UINT64 address, UINT64 size, UINT64 root, UINT64 inode, UINT64 offset, UINT32 refcount, LIST_ENTRY* rollback);
NTSTATUS decrease_extent_refcount_data(device_extension* Vcb, UINT64 address, UINT64 size, UINT64 root, UINT64 inode, UINT64 offset, UINT32 refcount, LIST_ENTRY* rollback);
NTSTATUS decrease_extent_refcount_shared_data(device_extension* Vcb, UINT64 address, UINT64 size, UINT64 treeaddr, UINT64 parent, LIST_ENTRY* rollback);
NTSTATUS decrease_extent_refcount_old(device_extension* Vcb, UINT64 address, UINT64 size, UINT64 treeaddr, LIST_ENTRY* rollback);
void decrease_chunk_usage(chunk* c, UINT64 delta);
NTSTATUS convert_old_data_extent(device_extension* Vcb, UINT64 address, UINT64 size, LIST_ENTRY* rollback);
UINT64 find_extent_data_refcount(device_extension* Vcb, UINT64 address, UINT64 size, UINT64 root, UINT64 objid, UINT64 offset);

// in worker-thread.c
void STDCALL worker_thread(void* context);
void do_read_job(PIRP Irp);
void do_write_job(device_extension* Vcb, PIRP Irp);

// in registry.c
void STDCALL read_registry(PUNICODE_STRING regpath);
NTSTATUS registry_mark_volume_mounted(BTRFS_UUID* uuid);
NTSTATUS registry_mark_volume_unmounted(BTRFS_UUID* uuid);
NTSTATUS registry_load_volume_options(BTRFS_UUID* uuid, mount_options* options);

#define fast_io_possible(fcb) (!FsRtlAreThereCurrentFileLocks(&fcb->lock) && !fcb->Vcb->readonly ? FastIoIsPossible : FastIoIsQuestionable)

static __inline void print_open_trees(device_extension* Vcb) {
    LIST_ENTRY* le = Vcb->trees.Flink;
    while (le != &Vcb->trees) {
        tree* t = CONTAINING_RECORD(le, tree, list_entry);
        tree_data* td = CONTAINING_RECORD(t->itemlist.Flink, tree_data, list_entry);
        ERR("tree %p: root %llx, level %u, first key (%llx,%x,%llx)\n",
                      t, t->root->id, t->header.level, td->key.obj_id, td->key.obj_type, td->key.offset);

        le = le->Flink;
    }
}

static __inline void InsertAfter(LIST_ENTRY* head, LIST_ENTRY* item, LIST_ENTRY* before) {
    item->Flink = before->Flink;
    before->Flink = item;
    item->Blink = before;

    if (item->Flink != head)
        item->Flink->Blink = item;
    else
        head->Blink = item;
}

#ifdef DEBUG_FCB_REFCOUNTS
#ifdef DEBUG_LONG_MESSAGES
#define increase_fileref_refcount(fileref) {\
    LONG rc = InterlockedIncrement(&fileref->refcount);\
    MSG(funcname, __FILE__, __LINE__, "fileref %p: refcount now %i\n", 1, fileref, rc);\
}
#else
#define increase_fileref_refcount(fileref) {\
    LONG rc = InterlockedIncrement(&fileref->refcount);\
    MSG(funcname, "fileref %p: refcount now %i\n", 1, fileref, rc);\
}
#endif
#else
#define increase_fileref_refcount(fileref) InterlockedIncrement(&fileref->refcount)
#endif

#ifdef _MSC_VER
// #define int3 __asm { int 3 }
#define int3 __debugbreak()
#else
#define int3 asm("int3;")
#endif

// if (Vcb->open_trees > 0) { ERR("open tree count = %i\n", Vcb->open_trees); print_open_trees(Vcb); int3; }
// else TRACE("open tree count = %i\n", Vcb->open_trees);

// FIXME - find a way to catch unfreed trees again

// from sys/stat.h
#define __S_IFMT        0170000 /* These bits determine file type.  */
#define __S_IFDIR       0040000 /* Directory.  */
#define __S_IFCHR       0020000 /* Character device.  */
#define __S_IFBLK       0060000 /* Block device.  */
#define __S_IFREG       0100000 /* Regular file.  */
#define __S_IFIFO       0010000 /* FIFO.  */
#define __S_IFLNK       0120000 /* Symbolic link.  */
#define __S_IFSOCK      0140000 /* Socket.  */
#define __S_ISTYPE(mode, mask)  (((mode) & __S_IFMT) == (mask))

#ifndef S_ISDIR
#define S_ISDIR(mode)    __S_ISTYPE((mode), __S_IFDIR)
#endif

#ifndef S_IRUSR
#define S_IRUSR 0000400
#endif

#ifndef S_IWUSR
#define S_IWUSR 0000200
#endif

#ifndef S_IXUSR
#define S_IXUSR 0000100
#endif

#ifdef __REACTOS__
#define S_IFDIR __S_IFDIR
#define S_IFREG __S_IFREG
#endif /* __REACTOS__ */

#ifndef S_IRGRP
#define S_IRGRP (S_IRUSR >> 3)
#endif

#ifndef S_IWGRP
#define S_IWGRP (S_IWUSR >> 3)
#endif

#ifndef S_IXGRP
#define S_IXGRP (S_IXUSR >> 3)
#endif

#ifndef S_IROTH
#define S_IROTH (S_IRGRP >> 3)
#endif

#ifndef S_IWOTH
#define S_IWOTH (S_IWGRP >> 3)
#endif

#ifndef S_IXOTH
#define S_IXOTH (S_IXGRP >> 3)
#endif

#if defined(__REACTOS__) && (NTDDI_VERSION < NTDDI_WIN7)
NTSTATUS WINAPI RtlUnicodeToUTF8N(CHAR *utf8_dest, ULONG utf8_bytes_max,
                                  ULONG *utf8_bytes_written,
                                  const WCHAR *uni_src, ULONG uni_bytes);
NTSTATUS WINAPI RtlUTF8ToUnicodeN(WCHAR *uni_dest, ULONG uni_bytes_max,
                                  ULONG *uni_bytes_written,
                                  const CHAR *utf8_src, ULONG utf8_bytes);
#endif /* defined(__REACTOS__) && (NTDDI_VERSION < NTDDI_WIN7) */
#if defined(__REACTOS__) && (NTDDI_VERSION < NTDDI_VISTA)
NTSTATUS NTAPI FsRtlRemoveDotsFromPath(PWSTR OriginalString,
                                       USHORT PathLength, USHORT *NewLength);
NTSTATUS NTAPI FsRtlValidateReparsePointBuffer(ULONG BufferLength,
                                               PREPARSE_DATA_BUFFER ReparseBuffer);
ULONG NTAPI KeQueryActiveProcessorCount(PKAFFINITY ActiveProcessors);
#endif /* defined(__REACTOS__) && (NTDDI_VERSION < NTDDI_VISTA) */

#endif
