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

#ifndef __REACTOS__
#include <sys/stat.h>
#endif /* __REACTOS__ */
#include "btrfs_drv.h"
#ifndef __REACTOS__
#include <winioctl.h>
#endif

extern PDEVICE_OBJECT devobj;

static NTSTATUS find_file_dir_index(device_extension* Vcb, root* r, UINT64 inode, UINT64 parinode, PANSI_STRING utf8, UINT64* pindex) {
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    UINT64 index;
    
    searchkey.obj_id = inode;
    searchkey.obj_type = TYPE_INODE_REF;
    searchkey.offset = parinode;
    
    Status = find_item(Vcb, r, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return Status;
    }
    
    if (!keycmp(&tp.item->key, &searchkey)) {
        INODE_REF* ir;
        ULONG len;
        
        index = 0;
        
        ir = (INODE_REF*)tp.item->data;
        len = tp.item->size;
        
        do {
            ULONG itemlen;
            
            if (len < sizeof(INODE_REF) || len < sizeof(INODE_REF) - 1 + ir->n) {
                ERR("(%llx,%x,%llx) was truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
                break;
            }
            
            itemlen = sizeof(INODE_REF) - sizeof(char) + ir->n;
            
            if (ir->n == utf8->Length && RtlCompareMemory(ir->name, utf8->Buffer, ir->n) == ir->n) {
                index = ir->index;
                break;
            }
            
            if (len > itemlen) {
                len -= itemlen;
                ir = (INODE_REF*)&ir->name[ir->n];
            } else
                break;
        } while (len > 0);
        
        if (index == 0)
            return STATUS_NOT_FOUND;
        
        *pindex = index;
        
        return STATUS_SUCCESS;
    } else
        return STATUS_NOT_FOUND;
}

static NTSTATUS find_file_dir_index_extref(device_extension* Vcb, root* r, UINT64 inode, UINT64 parinode, PANSI_STRING utf8, UINT64* pindex) {
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    UINT64 index;
    
    searchkey.obj_id = inode;
    searchkey.obj_type = TYPE_INODE_EXTREF;
    searchkey.offset = calc_crc32c((UINT32)parinode, (UINT8*)utf8->Buffer, utf8->Length);
    
    Status = find_item(Vcb, r, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return Status;
    }
    
    if (!keycmp(&tp.item->key, &searchkey)) {
        INODE_EXTREF* ier;
        ULONG len;
        
        index = 0;
        
        ier = (INODE_EXTREF*)tp.item->data;
        len = tp.item->size;
        
        do {
            ULONG itemlen;
            
            if (len < sizeof(INODE_EXTREF) || len < sizeof(INODE_EXTREF) - 1 + ier->n) {
                ERR("(%llx,%x,%llx) was truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
                break;
            }
            
            itemlen = sizeof(INODE_EXTREF) - sizeof(char) + ier->n;
            
            if (ier->n == utf8->Length && RtlCompareMemory(ier->name, utf8->Buffer, ier->n) == ier->n) {
                index = ier->index;
                break;
            }
            
            if (len > itemlen) {
                len -= itemlen;
                ier = (INODE_EXTREF*)&ier->name[ier->n];
            } else
                break;
        } while (len > 0);
        
        if (index == 0)
            return STATUS_NOT_FOUND;
        
        *pindex = index;
        
        return STATUS_SUCCESS;
    } else
        return STATUS_NOT_FOUND;
}

static NTSTATUS find_subvol_dir_index(device_extension* Vcb, root* r, UINT64 subvolid, UINT64 parinode, PANSI_STRING utf8, UINT64* pindex) {
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    ROOT_REF* rr;
    
    searchkey.obj_id = r->id;
    searchkey.obj_type = TYPE_ROOT_REF;
    searchkey.offset = subvolid;
    
    Status = find_item(Vcb, Vcb->root_root, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return Status;
    }
    
    if (keycmp(&tp.item->key, &searchkey)) {
        ERR("couldn't find (%llx,%x,%llx) in root tree\n", searchkey.obj_id, searchkey.obj_type, searchkey.offset);
        return STATUS_INTERNAL_ERROR;
    }
    
    if (tp.item->size < sizeof(ROOT_REF)) {
        ERR("(%llx,%x,%llx) was %u bytes, expected at least %u\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset,
            tp.item->size, sizeof(ROOT_REF));
        return STATUS_INTERNAL_ERROR;
    }
    
    rr = (ROOT_REF*)tp.item->data;
    
    if (tp.item->size < sizeof(ROOT_REF) - 1 + rr->n) {
        ERR("(%llx,%x,%llx) was %u bytes, expected %u\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset,
            tp.item->size, sizeof(ROOT_REF) - 1 + rr->n);
        return STATUS_INTERNAL_ERROR;
    }
    
    if (rr->dir == parinode && rr->n == utf8->Length && RtlCompareMemory(utf8->Buffer, rr->name, rr->n) == rr->n) {
        *pindex = rr->index;
        return STATUS_SUCCESS;
    } else
        return STATUS_NOT_FOUND;
}

static NTSTATUS load_index_list(fcb* fcb) {
    KEY searchkey;
    traverse_ptr tp, next_tp;
    NTSTATUS Status;
    BOOL b;
    
    searchkey.obj_id = fcb->inode;
    searchkey.obj_type = TYPE_DIR_INDEX;
    searchkey.offset = 2;
    
    Status = find_item(fcb->Vcb, fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return Status;
    }

    if (keycmp(&tp.item->key, &searchkey) == -1) {
        if (find_next_item(fcb->Vcb, &tp, &next_tp, FALSE)) {
            tp = next_tp;
            
            TRACE("moving on to %llx,%x,%llx\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
        }
    }
    
    if (tp.item->key.obj_id != fcb->inode || tp.item->key.obj_type != TYPE_DIR_INDEX) {
        Status = STATUS_SUCCESS;
        goto end;
    }
    
    do {
        DIR_ITEM* di;
        
        TRACE("key: %llx,%x,%llx\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
        di = (DIR_ITEM*)tp.item->data;
        
        if (tp.item->size < sizeof(DIR_ITEM) || tp.item->size < (sizeof(DIR_ITEM) - 1 + di->m + di->n)) {
            WARN("(%llx,%x,%llx) is truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
        } else {
            index_entry* ie;
            ULONG stringlen;
            UNICODE_STRING us;
            LIST_ENTRY* le;
            BOOL inserted;
            
            ie = ExAllocatePoolWithTag(PagedPool, sizeof(index_entry), ALLOC_TAG);
            if (!ie) {
                ERR("out of memory\n");
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto end;
            }
            
            ie->utf8.Length = ie->utf8.MaximumLength = di->n;
            
            if (di->n > 0) {
                ie->utf8.Buffer = ExAllocatePoolWithTag(PagedPool, ie->utf8.MaximumLength, ALLOC_TAG);
                if (!ie->utf8.Buffer) {
                    ERR("out of memory\n");
                    ExFreePool(ie);
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto end;
                }
                
                RtlCopyMemory(ie->utf8.Buffer, di->name, di->n);
            } else
                ie->utf8.Buffer = NULL;
            
            Status = RtlUTF8ToUnicodeN(NULL, 0, &stringlen, di->name, di->n);
            if (!NT_SUCCESS(Status)) {
                ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
                ExFreePool(ie);
                goto nextitem;
            }
            
            if (stringlen == 0) {
                ERR("UTF8 length was 0\n");
                if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
                ExFreePool(ie);
                goto nextitem;
            }
            
            us.Length = us.MaximumLength = stringlen;
            us.Buffer = ExAllocatePoolWithTag(PagedPool, us.MaximumLength, ALLOC_TAG);
            
            if (!us.Buffer) {
                ERR("out of memory\n");
                if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
                ExFreePool(ie);
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            
            Status = RtlUTF8ToUnicodeN(us.Buffer, stringlen, &stringlen, di->name, di->n);
            if (!NT_SUCCESS(Status)) {
                ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                ExFreePool(us.Buffer);
                if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
                ExFreePool(ie);
                goto nextitem;
            }
            
            Status = RtlUpcaseUnicodeString(&ie->filepart_uc, &us, TRUE);
            if (!NT_SUCCESS(Status)) {
                ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
                ExFreePool(us.Buffer);
                if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
                ExFreePool(ie);
                goto nextitem;
            }
            
            ie->key = di->key;
            ie->type = di->type;
            ie->index = tp.item->key.offset;
            
            ie->hash = calc_crc32c(0xfffffffe, (UINT8*)ie->filepart_uc.Buffer, (ULONG)ie->filepart_uc.Length);
            inserted = FALSE;
            
            le = fcb->index_list.Flink;
            while (le != &fcb->index_list) {
                index_entry* ie2 = CONTAINING_RECORD(le, index_entry, list_entry);
                
                if (ie2->hash >= ie->hash) {
                    InsertHeadList(le->Blink, &ie->list_entry);
                    inserted = TRUE;
                    break;
                }
                
                le = le->Flink;
            }
            
            if (!inserted)
                InsertTailList(&fcb->index_list, &ie->list_entry);
        }
        
nextitem:
        b = find_next_item(fcb->Vcb, &tp, &next_tp, FALSE);
         
        if (b) {
            tp = next_tp;
            
            b = tp.item->key.obj_id == fcb->inode && tp.item->key.obj_type == TYPE_DIR_INDEX;
        }
    } while (b);
    
    Status = STATUS_SUCCESS;
    
end:
    if (!NT_SUCCESS(Status)) {
        while (!IsListEmpty(&fcb->index_list)) {
            LIST_ENTRY* le = RemoveHeadList(&fcb->index_list);
            index_entry* ie = CONTAINING_RECORD(le, index_entry, list_entry);

            if (ie->utf8.Buffer) ExFreePool(ie->utf8.Buffer);
            if (ie->filepart_uc.Buffer) ExFreePool(ie->filepart_uc.Buffer);
            ExFreePool(ie);
        }
    } else
        mark_fcb_dirty(fcb);
    
    return Status;
}

static NTSTATUS STDCALL find_file_in_dir_index(file_ref* fr, PUNICODE_STRING filename, root** subvol, UINT64* inode, UINT8* type,
                                               UINT64* pindex, PANSI_STRING utf8) {
    LIST_ENTRY* le;
    NTSTATUS Status;
    UNICODE_STRING us;
    UINT32 hash;
        
    Status = RtlUpcaseUnicodeString(&us, filename, TRUE);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
        return Status;
    }
    
    hash = calc_crc32c(0xfffffffe, (UINT8*)us.Buffer, (ULONG)us.Length);
    
    ExAcquireResourceExclusiveLite(&fr->fcb->nonpaged->index_lock, TRUE);
    
    if (!fr->fcb->index_loaded) {
        Status = load_index_list(fr->fcb);
        if (!NT_SUCCESS(Status)) {
            ERR("load_index_list returned %08x\n", Status);
            goto end;
        }
        
        fr->fcb->index_loaded = TRUE;
    }
    
    ExConvertExclusiveToSharedLite(&fr->fcb->nonpaged->index_lock);
    
    le = fr->fcb->index_list.Flink;
    while (le != &fr->fcb->index_list) {
        index_entry* ie = CONTAINING_RECORD(le, index_entry, list_entry);
        
        if (ie->hash == hash && ie->filepart_uc.Length == us.Length && RtlCompareMemory(ie->filepart_uc.Buffer, us.Buffer, us.Length) == us.Length) {
            LIST_ENTRY* le;
            BOOL ignore_entry = FALSE;
            
            ExAcquireResourceSharedLite(&fr->nonpaged->children_lock, TRUE);

            le = fr->children.Flink;
            while (le != &fr->children) {
                file_ref* fr2 = CONTAINING_RECORD(le, file_ref, list_entry);
                
                if (fr2->index == ie->index) {
                    if (fr2->deleted || fr2->filepart_uc.Length != us.Length ||
                        RtlCompareMemory(fr2->filepart_uc.Buffer, us.Buffer, us.Length) != us.Length) {
                        ignore_entry = TRUE;
                        break;
                    }
                    break;
                } else if (fr2->index > ie->index)
                    break;
                
                le = le->Flink;
            }
            
            ExReleaseResourceLite(&fr->nonpaged->children_lock);
            
            if (ignore_entry)
                goto nextitem;
            
            if (ie->key.obj_type == TYPE_ROOT_ITEM) {
                if (subvol) {
                    *subvol = NULL;
                    
                    le = fr->fcb->Vcb->roots.Flink;
                    while (le != &fr->fcb->Vcb->roots) {
                        root* r2 = CONTAINING_RECORD(le, root, list_entry);
                        
                        if (r2->id == ie->key.obj_id) {
                            *subvol = r2;
                            break;
                        }
                        
                        le = le->Flink;
                    }
                }
                
                if (inode)
                    *inode = SUBVOL_ROOT_INODE;
                
                if (type)
                    *type = BTRFS_TYPE_DIRECTORY;
            } else {
                if (subvol)
                    *subvol = fr->fcb->subvol;
                
                if (inode)
                    *inode = ie->key.obj_id;
                
                if (type)
                    *type = ie->type;
            }
            
            if (utf8) {
                utf8->MaximumLength = utf8->Length = ie->utf8.Length;
                utf8->Buffer = ExAllocatePoolWithTag(PagedPool, utf8->MaximumLength, ALLOC_TAG);
                if (!utf8->Buffer) {
                    ERR("out of memory\n");
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto end;
                }
                
                RtlCopyMemory(utf8->Buffer, ie->utf8.Buffer, ie->utf8.Length);
            }
            
            if (pindex)
                *pindex = ie->index;
            
            Status = STATUS_SUCCESS;
            goto end;
        } else if (ie->hash > hash) {
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            goto end;
        }
        
nextitem:
        le = le->Flink;
    }
    
    Status = STATUS_OBJECT_NAME_NOT_FOUND;
    
end:
    ExReleaseResourceLite(&fr->fcb->nonpaged->index_lock);
    
    ExFreePool(us.Buffer);
    
    return Status;
}

NTSTATUS STDCALL find_file_in_dir_with_crc32(device_extension* Vcb, PUNICODE_STRING filename, UINT32 crc32, file_ref* fr,
                                             root** subvol, UINT64* inode, UINT8* type, UINT64* pindex, PANSI_STRING utf8) {
    DIR_ITEM* di;
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    ULONG stringlen;
    
    TRACE("(%p, %.*S, %08x, (%llx, %llx), %p, %p, %p)\n", Vcb, filename->Length / sizeof(WCHAR), filename->Buffer, crc32,
                                                          fr->fcb->subvol->id, fr->fcb->inode, subvol, inode, type);
    
    searchkey.obj_id = fr->fcb->inode;
    searchkey.obj_type = TYPE_DIR_ITEM;
    searchkey.offset = crc32;
    
    Status = find_item(Vcb, fr->fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return Status;
    }
    
    TRACE("found item %llx,%x,%llx\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
    
    if (!keycmp(&searchkey, &tp.item->key)) {
        UINT32 size = tp.item->size;
        
        // found by hash
        
        if (tp.item->size < sizeof(DIR_ITEM)) {
            WARN("(%llx;%llx,%x,%llx) was %u bytes, expected at least %u\n", fr->fcb->subvol->id, tp.item->key.obj_id, tp.item->key.obj_type,
                                                                             tp.item->key.offset, tp.item->size, sizeof(DIR_ITEM));
        } else {
            di = (DIR_ITEM*)tp.item->data;
            
            while (size > 0) {
                if (size < sizeof(DIR_ITEM) || size < (sizeof(DIR_ITEM) - 1 + di->m + di->n)) {
                    WARN("(%llx,%x,%llx) is truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
                    break;
                }
                
                size -= sizeof(DIR_ITEM) - sizeof(char);
                size -= di->n;
                size -= di->m;
                
                Status = RtlUTF8ToUnicodeN(NULL, 0, &stringlen, di->name, di->n);
                if (!NT_SUCCESS(Status)) {
                    ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                } else {
                    WCHAR* utf16 = ExAllocatePoolWithTag(PagedPool, stringlen, ALLOC_TAG);
                    UNICODE_STRING us;
                    
                    if (!utf16) {
                        ERR("out of memory\n");
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    
                    Status = RtlUTF8ToUnicodeN(utf16, stringlen, &stringlen, di->name, di->n);

                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                    } else {
                        us.Buffer = utf16;
                        us.Length = us.MaximumLength = (USHORT)stringlen;
                        
                        if (FsRtlAreNamesEqual(filename, &us, TRUE, NULL)) {
                            UINT64 index;
                            
                            if (di->key.obj_type == TYPE_ROOT_ITEM) {
                                LIST_ENTRY* le = Vcb->roots.Flink;
                                
                                if (subvol) {
                                    *subvol = NULL;
                                    
                                    while (le != &Vcb->roots) {
                                        root* r2 = CONTAINING_RECORD(le, root, list_entry);
                                        
                                        if (r2->id == di->key.obj_id) {
                                            *subvol = r2;
                                            break;
                                        }
                                        
                                        le = le->Flink;
                                    }
                                }

                                if (inode)
                                    *inode = SUBVOL_ROOT_INODE;
                                
                                if (type)
                                    *type = BTRFS_TYPE_DIRECTORY;
                            } else {
                                if (subvol)
                                    *subvol = fr->fcb->subvol;
                                
                                if (inode)
                                    *inode = di->key.obj_id;
                                
                                if (type)
                                    *type = di->type;
                            }
                            
                            if (utf8) {
                                utf8->MaximumLength = di->n;
                                utf8->Length = utf8->MaximumLength;
                                utf8->Buffer = ExAllocatePoolWithTag(PagedPool, utf8->MaximumLength, ALLOC_TAG);
                                if (!utf8->Buffer) {
                                    ERR("out of memory\n");
                                    ExFreePool(utf16);
                                    return STATUS_INSUFFICIENT_RESOURCES;
                                }
                                
                                RtlCopyMemory(utf8->Buffer, di->name, di->n);
                            }
                            
                            ExFreePool(utf16);
                            
                            index = 0;
                                
                            if (fr->fcb->subvol != Vcb->root_root) {
                                if (di->key.obj_type == TYPE_ROOT_ITEM) {
                                    Status = find_subvol_dir_index(Vcb, fr->fcb->subvol, di->key.obj_id, fr->fcb->inode, utf8, &index);
                                    if (!NT_SUCCESS(Status)) {
                                        ERR("find_subvol_dir_index returned %08x\n", Status);
                                        return Status;
                                    }
                                } else {
                                    Status = find_file_dir_index(Vcb, fr->fcb->subvol, di->key.obj_id, fr->fcb->inode, utf8, &index);
                                    if (!NT_SUCCESS(Status)) {
                                        if (Vcb->superblock.incompat_flags & BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF) {
                                            Status = find_file_dir_index_extref(Vcb, fr->fcb->subvol, di->key.obj_id, fr->fcb->inode, utf8, &index);
                                            
                                            if (!NT_SUCCESS(Status)) {
                                                ERR("find_file_dir_index_extref returned %08x\n", Status);
                                                return Status;
                                            }
                                        } else {
                                            ERR("find_file_dir_index returned %08x\n", Status);
                                            return Status;
                                        }
                                    }
                                }
                            }
                            
                            if (index != 0) {
                                LIST_ENTRY* le = fr->children.Flink;
                                
                                while (le != &fr->children) {
                                    file_ref* fr2 = CONTAINING_RECORD(le, file_ref, list_entry);
                                    
                                    if (fr2->index == index) {
                                        if (fr2->deleted || !FsRtlAreNamesEqual(&fr2->filepart, filename, TRUE, NULL)) {
                                            goto byindex;
                                        }
                                        break;
                                    } else if (fr2->index > index)
                                        break;
                                    
                                    le = le->Flink;
                                }
                            }
                            
//                             TRACE("found %.*S by hash at (%llx,%llx)\n", filename->Length / sizeof(WCHAR), filename->Buffer, (*subvol)->id, *inode);

                            if (pindex)
                                *pindex = index;
                            
                            return STATUS_SUCCESS;
                        }
                    }
                    
                    ExFreePool(utf16);
                }
                
                di = (DIR_ITEM*)&di->name[di->n + di->m];
            }
        }
    }
    
byindex:
    Status = find_file_in_dir_index(fr, filename, subvol, inode, type, pindex, utf8);
    if (!NT_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_NOT_FOUND) {
        ERR("find_file_in_dir_index returned %08x\n", Status);
        return Status;
    }
    
    return Status;
}

fcb* create_fcb() {
    fcb* fcb;
    
    fcb = ExAllocatePoolWithTag(PagedPool, sizeof(struct _fcb), ALLOC_TAG);
    if (!fcb) {
        ERR("out of memory\n");
        return NULL;
    }
    
#ifdef DEBUG_FCB_REFCOUNTS
    WARN("allocating fcb %p\n", fcb);
#endif
    RtlZeroMemory(fcb, sizeof(struct _fcb));
    
    fcb->Header.NodeTypeCode = BTRFS_NODE_TYPE_FCB;
    fcb->Header.NodeByteSize = sizeof(struct _fcb);
    
    fcb->nonpaged = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct _fcb_nonpaged), ALLOC_TAG);
    if (!fcb->nonpaged) {
        ERR("out of memory\n");
        ExFreePool(fcb);
        return NULL;
    }
    RtlZeroMemory(fcb->nonpaged, sizeof(struct _fcb_nonpaged));
    
    ExInitializeResourceLite(&fcb->nonpaged->paging_resource);
    fcb->Header.PagingIoResource = &fcb->nonpaged->paging_resource;
    
    ExInitializeFastMutex(&fcb->nonpaged->HeaderMutex);
    FsRtlSetupAdvancedHeader(&fcb->Header, &fcb->nonpaged->HeaderMutex);
    
    fcb->refcount = 1;
#ifdef DEBUG_FCB_REFCOUNTS
    WARN("fcb %p: refcount now %i\n", fcb, fcb->refcount);
#endif
    
    ExInitializeResourceLite(&fcb->nonpaged->resource);
    fcb->Header.Resource = &fcb->nonpaged->resource;
    
    ExInitializeResourceLite(&fcb->nonpaged->index_lock);
    
    FsRtlInitializeFileLock(&fcb->lock, NULL, NULL);
    
    InitializeListHead(&fcb->extents);
    InitializeListHead(&fcb->index_list);
    InitializeListHead(&fcb->hardlinks);
    
    return fcb;
}

file_ref* create_fileref() {
    file_ref* fr;
    
    fr = ExAllocatePoolWithTag(PagedPool, sizeof(file_ref), ALLOC_TAG);
    if (!fr) {
        ERR("out of memory\n");
        return NULL;
    }
    
    RtlZeroMemory(fr, sizeof(file_ref));
    
    fr->nonpaged = ExAllocatePoolWithTag(NonPagedPool, sizeof(file_ref_nonpaged), ALLOC_TAG);
    if (!fr->nonpaged) {
        ERR("out of memory\n");
        ExFreePool(fr);
        return NULL;
    }
    
    fr->refcount = 1;
    
#ifdef DEBUG_FCB_REFCOUNTS
    WARN("fileref %p: refcount now 1\n", fr);
#endif
    
    InitializeListHead(&fr->children);
    
    ExInitializeResourceLite(&fr->nonpaged->children_lock);
    
    return fr;
}

NTSTATUS STDCALL find_file_in_dir(device_extension* Vcb, PUNICODE_STRING filename, file_ref* fr,
                                  root** subvol, UINT64* inode, UINT8* type, UINT64* index, PANSI_STRING utf8) {
    char* fn;
    UINT32 crc32;
    ULONG utf8len;
    NTSTATUS Status;
    
    Status = RtlUnicodeToUTF8N(NULL, 0, &utf8len, filename->Buffer, filename->Length);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUnicodeToUTF8N 1 returned %08x\n", Status);
        return Status;
    }
    
    fn = ExAllocatePoolWithTag(PagedPool, utf8len, ALLOC_TAG);
    if (!fn) {
        ERR("out of memory\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    Status = RtlUnicodeToUTF8N(fn, utf8len, &utf8len, filename->Buffer, filename->Length);
    if (!NT_SUCCESS(Status)) {
        ExFreePool(fn);
        ERR("RtlUnicodeToUTF8N 2 returned %08x\n", Status);
        return Status;
    }
    
    TRACE("%.*s\n", utf8len, fn);
    
    crc32 = calc_crc32c(0xfffffffe, (UINT8*)fn, (ULONG)utf8len);
    TRACE("crc32c(%.*s) = %08x\n", utf8len, fn, crc32);
    
    return find_file_in_dir_with_crc32(Vcb, filename, crc32, fr, subvol, inode, type, index, utf8);
}

static BOOL find_stream(device_extension* Vcb, fcb* fcb, PUNICODE_STRING stream, PUNICODE_STRING newstreamname, UINT32* hash, PANSI_STRING xattr) {
    NTSTATUS Status;
    ULONG utf8len;
    char* utf8;
    UINT32 crc32;
    KEY searchkey;
    traverse_ptr tp, next_tp;
    BOOL success = FALSE, b;
    
    static char xapref[] = "user.";
    ULONG xapreflen = strlen(xapref);
    
    TRACE("(%p, %p, %.*S)\n", Vcb, fcb, stream->Length / sizeof(WCHAR), stream->Buffer);
    
    Status = RtlUnicodeToUTF8N(NULL, 0, &utf8len, stream->Buffer, stream->Length);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUnicodeToUTF8N 1 returned %08x\n", Status);
        return FALSE;
    }
    
    TRACE("utf8len = %u\n", utf8len);
    
    utf8 = ExAllocatePoolWithTag(PagedPool, xapreflen + utf8len + 1, ALLOC_TAG);
    if (!utf8) {
        ERR("out of memory\n");
        goto end;
    }
    
    RtlCopyMemory(utf8, xapref, xapreflen);
    
    Status = RtlUnicodeToUTF8N(&utf8[xapreflen], utf8len, &utf8len, stream->Buffer, stream->Length);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUnicodeToUTF8N 2 returned %08x\n", Status);
        goto end;
    }
    
    utf8len += xapreflen;
    utf8[utf8len] = 0;
    
    TRACE("utf8 = %s\n", utf8);
    
    crc32 = calc_crc32c(0xfffffffe, (UINT8*)utf8, utf8len);
    TRACE("crc32 = %08x\n", crc32);
    
    searchkey.obj_id = fcb->inode;
    searchkey.obj_type = TYPE_XATTR_ITEM;
    searchkey.offset = crc32;
    
    Status = find_item(Vcb, fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        goto end;
    }
    
    if (!keycmp(&tp.item->key, &searchkey)) {
        if (tp.item->size < sizeof(DIR_ITEM)) {
            ERR("(%llx,%x,%llx) was %u bytes, expected at least %u\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset, tp.item->size, sizeof(DIR_ITEM));
        } else {
            ULONG len = tp.item->size, xasize;
            DIR_ITEM* di = (DIR_ITEM*)tp.item->data;
            
            TRACE("found match on hash\n");
            
            while (len > 0) {
                if (len < sizeof(DIR_ITEM) || len < sizeof(DIR_ITEM) - 1 + di->m + di->n) {
                    ERR("(%llx,%x,%llx) was truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
                    break;
                }
                
                if (RtlCompareMemory(di->name, utf8, utf8len) == utf8len) {
                    TRACE("found exact match for %s\n", utf8);
                    
                    *hash = tp.item->key.offset;
                    
                    xattr->Buffer = ExAllocatePoolWithTag(PagedPool, di->n + 1, ALLOC_TAG);
                    if (!xattr->Buffer) {
                        ERR("out of memory\n");
                        goto end;
                    }
                    
                    xattr->Length = xattr->MaximumLength = di->n;
                    RtlCopyMemory(xattr->Buffer, di->name, di->n);
                    xattr->Buffer[di->n] = 0;
                    
                    success = TRUE;
                    goto end;
                }
                
                xasize = sizeof(DIR_ITEM) - 1 + di->m + di->n;
                
                if (len > xasize) {
                    len -= xasize;
                    di = (DIR_ITEM*)&di->name[di->m + di->n];
                } else
                    break;
            }
        }
    }
    
    searchkey.offset = 0;
    
    Status = find_item(Vcb, fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        goto end;
    }
    
    do {
        if (tp.item->key.obj_id == fcb->inode && tp.item->key.obj_type == TYPE_XATTR_ITEM && tp.item->key.offset != crc32) {
            if (tp.item->size < sizeof(DIR_ITEM)) {
                ERR("(%llx,%x,%llx) was %u bytes, expected at least %u\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset, tp.item->size, sizeof(DIR_ITEM));
            } else {
                ULONG len = tp.item->size, xasize;
                DIR_ITEM* di = (DIR_ITEM*)tp.item->data;
                ULONG utf16len;
                
                TRACE("found xattr with hash %08x\n", (UINT32)tp.item->key.offset);
                
                while (len > 0) {
                    if (len < sizeof(DIR_ITEM) || len < sizeof(DIR_ITEM) - 1 + di->m + di->n) {
                        ERR("(%llx,%x,%llx) was truncated\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset);
                        break;
                    }
                    
                    if (di->n > xapreflen && RtlCompareMemory(di->name, xapref, xapreflen) == xapreflen) {
                        TRACE("found potential xattr %.*s\n", di->n, di->name);
                    }
                    
                    Status = RtlUTF8ToUnicodeN(NULL, 0, &utf16len, &di->name[xapreflen], di->n - xapreflen);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                    } else {
                        WCHAR* utf16 = ExAllocatePoolWithTag(PagedPool, utf16len, ALLOC_TAG);
                        if (!utf16) {
                            ERR("out of memory\n");
                            goto end;
                        }
                        
                        Status = RtlUTF8ToUnicodeN(utf16, utf16len, &utf16len, &di->name[xapreflen], di->n - xapreflen);

                        if (!NT_SUCCESS(Status)) {
                            ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                        } else {
                            UNICODE_STRING us;
                            
                            us.Buffer = utf16;
                            us.Length = us.MaximumLength = (USHORT)utf16len;
                            
                            if (FsRtlAreNamesEqual(stream, &us, TRUE, NULL)) {
                                TRACE("found case-insensitive match for %s\n", utf8);
                                
                                *newstreamname = us;
                                *hash = tp.item->key.offset;
                                
                                xattr->Buffer = ExAllocatePoolWithTag(PagedPool, di->n + 1, ALLOC_TAG);
                                if (!xattr->Buffer) {
                                    ERR("out of memory\n");
                                    ExFreePool(utf16);
                                    goto end;
                                }
                                
                                xattr->Length = xattr->MaximumLength = di->n;
                                RtlCopyMemory(xattr->Buffer, di->name, di->n);
                                xattr->Buffer[di->n] = 0;
                                
                                success = TRUE;
                                goto end;
                            }
                        }
                        
                        ExFreePool(utf16);
                    }
                    
                    xasize = sizeof(DIR_ITEM) - 1 + di->m + di->n;
                    
                    if (len > xasize) {
                        len -= xasize;
                        di = (DIR_ITEM*)&di->name[di->m + di->n];
                    } else
                        break;
                }
            }
        }
        
        b = find_next_item(Vcb, &tp, &next_tp, FALSE);
        if (b) {
            tp = next_tp;
            
            if (next_tp.item->key.obj_id > fcb->inode || next_tp.item->key.obj_type > TYPE_XATTR_ITEM)
                break;
        }
    } while (b);
    
end:
    ExFreePool(utf8);
    
    return success;
}

static NTSTATUS split_path(PUNICODE_STRING path, UNICODE_STRING** parts, ULONG* num_parts, BOOL* stream) {
    ULONG len, i, j, np;
    BOOL has_stream;
    UNICODE_STRING* ps;
    WCHAR* buf;
    
    np = 1;
    
    len = path->Length / sizeof(WCHAR);
    if (len > 0 && (path->Buffer[len - 1] == '/' || path->Buffer[len - 1] == '\\'))
        len--;
    
    has_stream = FALSE;
    for (i = 0; i < len; i++) {
        if (path->Buffer[i] == '/' || path->Buffer[i] == '\\') {
            np++;
            has_stream = FALSE;
        } else if (path->Buffer[i] == ':') {
            has_stream = TRUE;
        }
    }
    
    if (has_stream)
        np++;
    
    ps = ExAllocatePoolWithTag(PagedPool, np * sizeof(UNICODE_STRING), ALLOC_TAG);
    if (!ps) {
        ERR("out of memory\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(ps, np * sizeof(UNICODE_STRING));
    
    buf = path->Buffer;
    
    j = 0;
    for (i = 0; i < len; i++) {
        if (path->Buffer[i] == '/' || path->Buffer[i] == '\\') {
            ps[j].Buffer = buf;
            ps[j].Length = (&path->Buffer[i] - buf) * sizeof(WCHAR);
            ps[j].MaximumLength = ps[j].Length;
            
            buf = &path->Buffer[i+1];
            j++;
        }
    }
    
    ps[j].Buffer = buf;
    ps[j].Length = (&path->Buffer[i] - buf) * sizeof(WCHAR);
    ps[j].MaximumLength = ps[j].Length;
    
    if (has_stream) {
        static WCHAR datasuf[] = {':','$','D','A','T','A',0};
        UNICODE_STRING dsus;
        
        dsus.Buffer = datasuf;
        dsus.Length = dsus.MaximumLength = wcslen(datasuf) * sizeof(WCHAR);
        
        for (i = 0; i < ps[j].Length / sizeof(WCHAR); i++) {
            if (ps[j].Buffer[i] == ':') {
                ps[j+1].Buffer = &ps[j].Buffer[i+1];
                ps[j+1].Length = ps[j].Length - (i * sizeof(WCHAR)) - sizeof(WCHAR);
                
                ps[j].Length = i * sizeof(WCHAR);
                ps[j].MaximumLength = ps[j].Length;
                
                j++;
                
                break;
            }
        }
        
        // FIXME - should comparison be case-insensitive?
        // remove :$DATA suffix
        if (ps[j].Length >= dsus.Length && RtlCompareMemory(&ps[j].Buffer[(ps[j].Length - dsus.Length)/sizeof(WCHAR)], dsus.Buffer, dsus.Length) == dsus.Length)
            ps[j].Length -= dsus.Length;
        
        if (ps[j].Length == 0) {
            np--;
            has_stream = FALSE;
        }
    }
    
    // if path is just stream name, remove first empty item
    if (has_stream && path->Length >= sizeof(WCHAR) && path->Buffer[0] == ':') {
        ps[0] = ps[1];
        np--;
    }

//     for (i = 0; i < np; i++) {
//         ERR("part %u: %u, (%.*S)\n", i, ps[i].Length, ps[i].Length / sizeof(WCHAR), ps[i].Buffer);
//     }
    
    *num_parts = np;
    *parts = ps;
    *stream = has_stream;
    
    return STATUS_SUCCESS;
}

// #ifdef DEBUG_FCB_REFCOUNTS
// static void print_fcbs(device_extension* Vcb) {
//     fcb* fcb = Vcb->fcbs;
//     
//     while (fcb) {
//         ERR("fcb %p (%.*S): refcount %u\n", fcb, fcb->full_filename.Length / sizeof(WCHAR), fcb->full_filename.Buffer, fcb->refcount);
//         
//         fcb = fcb->next;
//     }
// }
// #endif

static file_ref* search_fileref_children(file_ref* dir, PUNICODE_STRING name) {
    LIST_ENTRY* le;
    file_ref *c, *deleted = NULL;
    NTSTATUS Status;
    UNICODE_STRING ucus;
#ifdef DEBUG_FCB_REFCOUNTS
    ULONG rc;
#endif

    Status = RtlUpcaseUnicodeString(&ucus, name, TRUE);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
        return NULL;
    }
    
    le = dir->children.Flink;
    while (le != &dir->children) {
        c = CONTAINING_RECORD(le, file_ref, list_entry);
        
        if (c->refcount > 0 && c->filepart_uc.Length == ucus.Length &&
            RtlCompareMemory(c->filepart_uc.Buffer, ucus.Buffer, ucus.Length) == ucus.Length) {
            if (c->deleted) {
                deleted = c;
            } else {
#ifdef DEBUG_FCB_REFCOUNTS
                rc = InterlockedIncrement(&c->refcount);
                WARN("fileref %p: refcount now %i (%S)\n", c, rc, file_desc_fileref(c));
#else
                InterlockedIncrement(&c->refcount);
#endif
                ExFreePool(ucus.Buffer);
                
                return c;
            }
        }
        
        le = le->Flink;
    }
    
    if (deleted)
        increase_fileref_refcount(deleted);
    
    ExFreePool(ucus.Buffer);
    
    return deleted;
}

static UINT64 get_extent_refcount(device_extension* Vcb, UINT64 address, UINT64 size) {
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    EXTENT_ITEM* ei;
    
    searchkey.obj_id = address;
    searchkey.obj_type = TYPE_EXTENT_ITEM;
    searchkey.offset = size;
    
    Status = find_item(Vcb, Vcb->extent_root, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        return 0;
    }
    
    if (keycmp(&searchkey, &tp.item->key)) {
        ERR("couldn't find (%llx,%x,%llx) in extent tree\n", searchkey.obj_id, searchkey.obj_type, searchkey.offset);
        return 0;
    }
    
    if (tp.item->size == sizeof(EXTENT_ITEM_V0)) {
        EXTENT_ITEM_V0* eiv0 = (EXTENT_ITEM_V0*)tp.item->data;
        
        return eiv0->refcount;
    } else if (tp.item->size < sizeof(EXTENT_ITEM)) {
        ERR("(%llx,%x,%llx) was %x bytes, expected at least %x\n", tp.item->key.obj_id, tp.item->key.obj_type,
                                                                       tp.item->key.offset, tp.item->size, sizeof(EXTENT_DATA));
        return 0;
    }
    
    ei = (EXTENT_ITEM*)tp.item->data;
    
    return ei->refcount;
}

NTSTATUS open_fcb(device_extension* Vcb, root* subvol, UINT64 inode, UINT8 type, PANSI_STRING utf8, fcb* parent, fcb** pfcb) {
    KEY searchkey;
    traverse_ptr tp;
    NTSTATUS Status;
    fcb* fcb;
    BOOL b;
    
    if (!IsListEmpty(&subvol->fcbs)) {
        LIST_ENTRY* le = subvol->fcbs.Flink;
                                
        while (le != &subvol->fcbs) {
            fcb = CONTAINING_RECORD(le, struct _fcb, list_entry);
            
            if (fcb->inode == inode && !fcb->ads) {
#ifdef DEBUG_FCB_REFCOUNTS
                LONG rc = InterlockedIncrement(&fcb->refcount);
                
                WARN("fcb %p: refcount now %i (subvol %llx, inode %llx)\n", fcb, rc, fcb->subvol->id, fcb->inode);
#else
                InterlockedIncrement(&fcb->refcount);
#endif

                *pfcb = fcb;
                return STATUS_SUCCESS;
            }
            
            le = le->Flink;
        }
    }
    
    fcb = create_fcb();
    if (!fcb) {
        ERR("out of memory\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    fcb->Vcb = Vcb;
    
    fcb->subvol = subvol;
    fcb->inode = inode;
    fcb->type = type;
    
    searchkey.obj_id = inode;
    searchkey.obj_type = TYPE_INODE_ITEM;
    searchkey.offset = 0xffffffffffffffff;
    
    Status = find_item(Vcb, subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        free_fcb(fcb);
        return Status;
    }
    
    if (tp.item->key.obj_id != searchkey.obj_id || tp.item->key.obj_type != searchkey.obj_type) {
        WARN("couldn't find INODE_ITEM for inode %llx in subvol %llx\n", inode, subvol->id);
        free_fcb(fcb);
        return STATUS_INVALID_PARAMETER;
    }
    
    if (tp.item->size > 0)
        RtlCopyMemory(&fcb->inode_item, tp.item->data, min(sizeof(INODE_ITEM), tp.item->size));
    
    if (fcb->type == 0) { // guess the type from the inode mode, if the caller doesn't know already
        if (fcb->inode_item.st_mode & __S_IFDIR)
            fcb->type = BTRFS_TYPE_DIRECTORY;
        else if (fcb->inode_item.st_mode & __S_IFCHR)
            fcb->type = BTRFS_TYPE_CHARDEV;
        else if (fcb->inode_item.st_mode & __S_IFBLK)
            fcb->type = BTRFS_TYPE_BLOCKDEV;
        else if (fcb->inode_item.st_mode & __S_IFIFO)
            fcb->type = BTRFS_TYPE_FIFO;
        else if (fcb->inode_item.st_mode & __S_IFLNK)
            fcb->type = BTRFS_TYPE_SYMLINK;
        else if (fcb->inode_item.st_mode & __S_IFSOCK)
            fcb->type = BTRFS_TYPE_SOCKET;
        else
            fcb->type = BTRFS_TYPE_FILE;
    }
    
    fcb->atts = get_file_attributes(Vcb, &fcb->inode_item, fcb->subvol, fcb->inode, fcb->type, utf8 && utf8->Buffer[0] == '.', FALSE);
    
    fcb_get_sd(fcb, parent);
    
    if (fcb->type == BTRFS_TYPE_DIRECTORY) {
        UINT8* xattrdata;
        UINT16 xattrlen;
        
        if (get_xattr(Vcb, subvol, inode, EA_REPARSE, EA_REPARSE_HASH, &xattrdata, &xattrlen)) {
            fcb->reparse_xattr.Buffer = (char*)xattrdata;
            fcb->reparse_xattr.Length = fcb->reparse_xattr.MaximumLength = xattrlen;
        }
    }
    
    InsertTailList(&subvol->fcbs, &fcb->list_entry);
    InsertTailList(&Vcb->all_fcbs, &fcb->list_entry_all);
    
    fcb->Header.IsFastIoPossible = fast_io_possible(fcb);
    
    if (fcb->inode_item.st_size == 0 || (fcb->type != BTRFS_TYPE_FILE && fcb->type != BTRFS_TYPE_SYMLINK)) {
        fcb->Header.AllocationSize.QuadPart = 0;
        fcb->Header.FileSize.QuadPart = 0;
        fcb->Header.ValidDataLength.QuadPart = 0;
    } else {
        EXTENT_DATA* ed = NULL;
        traverse_ptr next_tp;
        
        searchkey.obj_id = fcb->inode;
        searchkey.obj_type = TYPE_EXTENT_DATA;
        searchkey.offset = 0;
        
        Status = find_item(fcb->Vcb, fcb->subvol, &tp, &searchkey, FALSE);
        if (!NT_SUCCESS(Status)) {
            ERR("error - find_item returned %08x\n", Status);
            free_fcb(fcb);
            return Status;
        }
        
        do {
            if (tp.item->key.obj_id == searchkey.obj_id && tp.item->key.obj_type == searchkey.obj_type) {
                extent* ext;
                BOOL unique = FALSE;
                
                ed = (EXTENT_DATA*)tp.item->data;
                
                if (tp.item->size < sizeof(EXTENT_DATA)) {
                    ERR("(%llx,%x,%llx) was %llx bytes, expected at least %llx\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset,
                        tp.item->size, sizeof(EXTENT_DATA));
                    
                    free_fcb(fcb);
                    return STATUS_INTERNAL_ERROR;
                }
                
                if (ed->type == EXTENT_TYPE_REGULAR || ed->type == EXTENT_TYPE_PREALLOC) {
                    EXTENT_DATA2* ed2 = (EXTENT_DATA2*)&ed->data[0];
                    
                    if (tp.item->size < sizeof(EXTENT_DATA) - 1 + sizeof(EXTENT_DATA2)) {
                        ERR("(%llx,%x,%llx) was %llx bytes, expected at least %llx\n", tp.item->key.obj_id, tp.item->key.obj_type, tp.item->key.offset,
                            tp.item->size, sizeof(EXTENT_DATA) - 1 + sizeof(EXTENT_DATA2));
                    
                        free_fcb(fcb);
                        return STATUS_INTERNAL_ERROR;
                    }
                    
                    if (ed2->size != 0)
                        unique = get_extent_refcount(fcb->Vcb, ed2->address, ed2->size) == 1;
                }
                
                ext = ExAllocatePoolWithTag(PagedPool, sizeof(extent), ALLOC_TAG);
                if (!ext) {
                    ERR("out of memory\n");
                    free_fcb(fcb);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
                
                ext->data = ExAllocatePoolWithTag(PagedPool, tp.item->size, ALLOC_TAG);
                if (!ext->data) {
                    ERR("out of memory\n");
                    ExFreePool(ext);
                    free_fcb(fcb);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
                
                ext->offset = tp.item->key.offset;
                RtlCopyMemory(ext->data, tp.item->data, tp.item->size);
                ext->datalen = tp.item->size;
                ext->unique = unique;
                ext->ignore = FALSE;
                
                InsertTailList(&fcb->extents, &ext->list_entry);
            }
            
            b = find_next_item(Vcb, &tp, &next_tp, FALSE);
         
            if (b) {
                tp = next_tp;
                
                if (tp.item->key.obj_id > searchkey.obj_id || (tp.item->key.obj_id == searchkey.obj_id && tp.item->key.obj_type > searchkey.obj_type))
                    break;
            }
        } while (b);
        
        if (ed && ed->type == EXTENT_TYPE_INLINE)
            fcb->Header.AllocationSize.QuadPart = fcb->inode_item.st_size;
        else
            fcb->Header.AllocationSize.QuadPart = sector_align(fcb->inode_item.st_size, fcb->Vcb->superblock.sector_size);
        
        fcb->Header.FileSize.QuadPart = fcb->inode_item.st_size;
        fcb->Header.ValidDataLength.QuadPart = fcb->inode_item.st_size;
    }
    
    // FIXME - only do if st_nlink > 1?
    
    searchkey.obj_id = inode;
    searchkey.obj_type = TYPE_INODE_REF;
    searchkey.offset = 0;
    
    Status = find_item(fcb->Vcb, fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        free_fcb(fcb);
        return Status;
    }
    
    do {
        traverse_ptr next_tp;
        
        if (tp.item->key.obj_id == searchkey.obj_id) {
            if (tp.item->key.obj_type == TYPE_INODE_REF) {
                ULONG len;
                INODE_REF* ir;
                
                len = tp.item->size;
                ir = (INODE_REF*)tp.item->data;
                
                while (len >= sizeof(INODE_REF) - 1) {
                    hardlink* hl;
                    ULONG stringlen;
                    
                    hl = ExAllocatePoolWithTag(PagedPool, sizeof(hardlink), ALLOC_TAG);
                    if (!hl) {
                        ERR("out of memory\n");
                        free_fcb(fcb);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    
                    hl->parent = tp.item->key.offset;
                    hl->index = ir->index;
                    
                    hl->utf8.Length = hl->utf8.MaximumLength = ir->n;
                    
                    if (hl->utf8.Length > 0) {
                        hl->utf8.Buffer = ExAllocatePoolWithTag(PagedPool, hl->utf8.MaximumLength, ALLOC_TAG);
                        RtlCopyMemory(hl->utf8.Buffer, ir->name, ir->n);
                    }
                    
                    Status = RtlUTF8ToUnicodeN(NULL, 0, &stringlen, ir->name, ir->n);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                        ExFreePool(hl);
                        free_fcb(fcb);
                        return Status;
                    }
                    
                    hl->name.Length = hl->name.MaximumLength = stringlen;
                    
                    if (stringlen == 0)
                        hl->name.Buffer = NULL;
                    else {
                        hl->name.Buffer = ExAllocatePoolWithTag(PagedPool, hl->name.MaximumLength, ALLOC_TAG);
                        
                        if (!hl->name.Buffer) {
                            ERR("out of memory\n");
                            ExFreePool(hl);
                            free_fcb(fcb);
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }
                        
                        Status = RtlUTF8ToUnicodeN(hl->name.Buffer, stringlen, &stringlen, ir->name, ir->n);
                        if (!NT_SUCCESS(Status)) {
                            ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                            ExFreePool(hl->name.Buffer);
                            ExFreePool(hl);
                            free_fcb(fcb);
                            return Status;
                        }
                    }
                    
                    InsertTailList(&fcb->hardlinks, &hl->list_entry);
                    
                    len -= sizeof(INODE_REF) - 1 + ir->n;
                    ir = (INODE_REF*)&ir->name[ir->n];
                }
            } else if (tp.item->key.obj_type == TYPE_INODE_EXTREF) {
                ULONG len;
                INODE_EXTREF* ier;
                
                len = tp.item->size;
                ier = (INODE_EXTREF*)tp.item->data;
                
                while (len >= sizeof(INODE_EXTREF) - 1) {
                    hardlink* hl;
                    ULONG stringlen;
                    
                    hl = ExAllocatePoolWithTag(PagedPool, sizeof(hardlink), ALLOC_TAG);
                    if (!hl) {
                        ERR("out of memory\n");
                        free_fcb(fcb);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }
                    
                    hl->parent = ier->dir;
                    hl->index = ier->index;
                    
                    hl->utf8.Length = hl->utf8.MaximumLength = ier->n;
                    
                    if (hl->utf8.Length > 0) {
                        hl->utf8.Buffer = ExAllocatePoolWithTag(PagedPool, hl->utf8.MaximumLength, ALLOC_TAG);
                        RtlCopyMemory(hl->utf8.Buffer, ier->name, ier->n);
                    }
                    
                    Status = RtlUTF8ToUnicodeN(NULL, 0, &stringlen, ier->name, ier->n);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                        ExFreePool(hl);
                        free_fcb(fcb);
                        return Status;
                    }
                    
                    hl->name.Length = hl->name.MaximumLength = stringlen;
                    
                    if (stringlen == 0)
                        hl->name.Buffer = NULL;
                    else {
                        hl->name.Buffer = ExAllocatePoolWithTag(PagedPool, hl->name.MaximumLength, ALLOC_TAG);
                        
                        if (!hl->name.Buffer) {
                            ERR("out of memory\n");
                            ExFreePool(hl);
                            free_fcb(fcb);
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }
                        
                        Status = RtlUTF8ToUnicodeN(hl->name.Buffer, stringlen, &stringlen, ier->name, ier->n);
                        if (!NT_SUCCESS(Status)) {
                            ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                            ExFreePool(hl->name.Buffer);
                            ExFreePool(hl);
                            free_fcb(fcb);
                            return Status;
                        }
                    }
                    
                    InsertTailList(&fcb->hardlinks, &hl->list_entry);
                    
                    len -= sizeof(INODE_EXTREF) - 1 + ier->n;
                    ier = (INODE_EXTREF*)&ier->name[ier->n];
                }
            }
        }
        
        b = find_next_item(Vcb, &tp, &next_tp, FALSE);
        
        if (b) {
            tp = next_tp;
            
            if (tp.item->key.obj_id > searchkey.obj_id || (tp.item->key.obj_id == searchkey.obj_id && tp.item->key.obj_type > TYPE_INODE_EXTREF))
                break;
        }
    } while (b);

    *pfcb = fcb;
    return STATUS_SUCCESS;
}

NTSTATUS open_fcb_stream(device_extension* Vcb, root* subvol, UINT64 inode, ANSI_STRING* xattr,
                         UINT32 streamhash, fcb* parent, fcb** pfcb) {
    fcb* fcb;
    UINT8* xattrdata;
    UINT16 xattrlen;
    
    if (!IsListEmpty(&subvol->fcbs)) {
        LIST_ENTRY* le = subvol->fcbs.Flink;
                                
        while (le != &subvol->fcbs) {
            fcb = CONTAINING_RECORD(le, struct _fcb, list_entry);
            
            if (fcb->inode == inode && fcb->ads && fcb->adsxattr.Length == xattr->Length &&
                RtlCompareMemory(fcb->adsxattr.Buffer, xattr->Buffer, fcb->adsxattr.Length) == fcb->adsxattr.Length) {
#ifdef DEBUG_FCB_REFCOUNTS
                LONG rc = InterlockedIncrement(&fcb->refcount);
                
                WARN("fcb %p: refcount now %i (subvol %llx, inode %llx)\n", fcb, rc, fcb->subvol->id, fcb->inode);
#else
                InterlockedIncrement(&fcb->refcount);
#endif

                *pfcb = fcb;
                return STATUS_SUCCESS;
            }
                            
            le = le->Flink;
        }
    }
    
    fcb = create_fcb();
    if (!fcb) {
        ERR("out of memory\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
      
    if (!get_xattr(Vcb, parent->subvol, parent->inode, xattr->Buffer, streamhash, &xattrdata, &xattrlen)) {
        ERR("get_xattr failed\n");
        free_fcb(fcb);
        return STATUS_INTERNAL_ERROR;
    }

    fcb->Vcb = Vcb;
    
    fcb->subvol = parent->subvol;
    fcb->inode = parent->inode;
    fcb->type = parent->type;
    fcb->ads = TRUE;
    fcb->adshash = streamhash;
    fcb->adsxattr = *xattr;
    
    fcb->adsdata.Buffer = (char*)xattrdata;
    fcb->adsdata.Length = fcb->adsdata.MaximumLength = xattrlen;
    
    fcb->Header.IsFastIoPossible = fast_io_possible(fcb);
    fcb->Header.AllocationSize.QuadPart = xattrlen;
    fcb->Header.FileSize.QuadPart = xattrlen;
    fcb->Header.ValidDataLength.QuadPart = xattrlen;
    
    TRACE("stream found: size = %x, hash = %08x\n", xattrlen, fcb->adshash);
    
    InsertTailList(&fcb->subvol->fcbs, &fcb->list_entry);
    InsertTailList(&Vcb->all_fcbs, &fcb->list_entry_all);
    
    *pfcb = fcb;
    
    return STATUS_SUCCESS;
}

void insert_fileref_child(file_ref* parent, file_ref* child, BOOL do_lock) {
    if (do_lock)
        ExAcquireResourceExclusiveLite(&parent->nonpaged->children_lock, TRUE);
    
    if (IsListEmpty(&parent->children))
        InsertTailList(&parent->children, &child->list_entry);
    else {
        LIST_ENTRY* le = parent->children.Flink;
        file_ref* fr1 = CONTAINING_RECORD(le, file_ref, list_entry);
        
        if (child->index < fr1->index)
            InsertHeadList(&parent->children, &child->list_entry);
        else {
            while (le != &parent->children) {
                file_ref* fr2 = (le->Flink == &parent->children) ? NULL : CONTAINING_RECORD(le->Flink, file_ref, list_entry);
                
                if (child->index >= fr1->index && (!fr2 || fr2->index > child->index)) {
                    InsertHeadList(&fr1->list_entry, &child->list_entry);
                    break;
                }
                
                fr1 = fr2;
                le = le->Flink;
            }
        }
    }
    
    if (do_lock)
        ExReleaseResourceLite(&parent->nonpaged->children_lock);
}

NTSTATUS open_fileref(device_extension* Vcb, file_ref** pfr, PUNICODE_STRING fnus, file_ref* related, BOOL parent, USHORT* unparsed) {
    UNICODE_STRING fnus2;
    file_ref *dir, *sf, *sf2;
    ULONG i, num_parts;
    UNICODE_STRING* parts = NULL;
    BOOL has_stream;
    NTSTATUS Status;
    
    TRACE("(%p, %p, %p, %u, %p)\n", Vcb, pfr, related, parent, unparsed);
    
    if (Vcb->removing || Vcb->locked)
        return STATUS_ACCESS_DENIED;
    
    fnus2 = *fnus;
    
    if (fnus2.Length < sizeof(WCHAR) && !related) {
        ERR("error - fnus was too short\n");
        return STATUS_INTERNAL_ERROR;
    }
    
    if (related && fnus->Length == 0) {
        increase_fileref_refcount(related);
        
        *pfr = related;
        return STATUS_SUCCESS;
    }
    
    if (related) {
        dir = related;
    } else {
        if (fnus2.Buffer[0] != '\\') {
            ERR("error - filename %.*S did not begin with \\\n", fnus2.Length / sizeof(WCHAR), fnus2.Buffer);
            return STATUS_OBJECT_PATH_NOT_FOUND;
        }
        
        if (fnus2.Length == sizeof(WCHAR)) {
            if (Vcb->root_fileref->fcb->open_count == 0) { // don't allow root to be opened on unmounted FS
                ULONG cc;
                IO_STATUS_BLOCK iosb;
                
                Status = dev_ioctl(Vcb->devices[0].devobj, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, &cc, sizeof(ULONG), TRUE, &iosb);
                
                if (!NT_SUCCESS(Status))
                    return Status;
            }
            
            increase_fileref_refcount(Vcb->root_fileref);
            *pfr = Vcb->root_fileref;
            return STATUS_SUCCESS;
        }
        
        dir = Vcb->root_fileref;
        
        fnus2.Buffer++;
        fnus2.Length -= sizeof(WCHAR);
        fnus2.MaximumLength -= sizeof(WCHAR);
    }
    
    if (dir->fcb->type != BTRFS_TYPE_DIRECTORY && (fnus->Length < sizeof(WCHAR) || fnus->Buffer[0] != ':')) {
        WARN("passed related fileref which isn't a directory (%S) (fnus = %.*S)\n",
             file_desc_fileref(related), fnus->Length / sizeof(WCHAR), fnus->Buffer);
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }
    
    if (fnus->Length == 0) {
        num_parts = 0;
    } else {
        Status = split_path(&fnus2, &parts, &num_parts, &has_stream);
        if (!NT_SUCCESS(Status)) {
            ERR("split_path returned %08x\n", Status);
            return Status;
        }
    }
    
    sf = dir;
    increase_fileref_refcount(dir);
    
    if (parent) {
        num_parts--;
        
        if (has_stream && num_parts > 0) {
            num_parts--;
            has_stream = FALSE;
        }
    }
    
    if (num_parts == 0) {
        Status = STATUS_SUCCESS;
        *pfr = dir;
        goto end2;
    }
    
    ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
    
    for (i = 0; i < num_parts; i++) {
        BOOL lastpart = (i == num_parts-1) || (i == num_parts-2 && has_stream);
        
        sf2 = search_fileref_children(sf, &parts[i]);
        
        if (sf2 && sf2->fcb->type != BTRFS_TYPE_DIRECTORY && !lastpart) {
            WARN("passed path including file as subdirectory\n");
            free_fileref(sf2);
            
            Status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto end;
        }
        
        if (!sf2) {
            if (has_stream && i == num_parts - 1) {
                UNICODE_STRING streamname;
                ANSI_STRING xattr;
                UINT32 streamhash;
                
                streamname.Buffer = NULL;
                streamname.Length = streamname.MaximumLength = 0;
                xattr.Buffer = NULL;
                xattr.Length = xattr.MaximumLength = 0;
                
                // FIXME - check if already opened
                
                if (!find_stream(Vcb, sf->fcb, &parts[i], &streamname, &streamhash, &xattr)) {
                    TRACE("could not find stream %.*S\n", parts[i].Length / sizeof(WCHAR), parts[i].Buffer);
                    
                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                    goto end;
                } else {
                    fcb* fcb;

                    if (streamhash == EA_DOSATTRIB_HASH && xattr.Length == strlen(EA_DOSATTRIB) &&
                        RtlCompareMemory(xattr.Buffer, EA_DOSATTRIB, xattr.Length) == xattr.Length) {
                        WARN("not allowing user.DOSATTRIB to be opened as stream\n");
                    
                        Status = STATUS_OBJECT_NAME_NOT_FOUND;
                        goto end;
                    }
                    
                    Status = open_fcb_stream(Vcb, sf->fcb->subvol, sf->fcb->inode, &xattr, streamhash, sf->fcb, &fcb);
                    if (!NT_SUCCESS(Status)) {
                        ERR("open_fcb_stream returned %08x\n", Status);
                        goto end;
                    }
                    
                    sf2 = create_fileref();
                    if (!sf2) {
                        ERR("out of memory\n");
                        free_fcb(fcb);
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto end;
                    }
                    
                    sf2->fcb = fcb;
        
                    if (streamname.Buffer) // case has changed
                        sf2->filepart = streamname;
                    else {
                        sf2->filepart.MaximumLength = sf2->filepart.Length = parts[i].Length;
                        sf2->filepart.Buffer = ExAllocatePoolWithTag(PagedPool, sf2->filepart.MaximumLength, ALLOC_TAG);
                        if (!sf2->filepart.Buffer) {
                            ERR("out of memory\n");
                            free_fileref(sf2);
                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            goto end;
                        }   
                        
                        RtlCopyMemory(sf2->filepart.Buffer, parts[i].Buffer, parts[i].Length);
                    }
                    
                    Status = RtlUpcaseUnicodeString(&sf2->filepart_uc, &sf2->filepart, TRUE);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
                        free_fileref(sf2);
                        goto end;
                    }
                    
                    // FIXME - make sure all functions know that ADS FCBs won't have a valid SD or INODE_ITEM

                    sf2->parent = (struct _file_ref*)sf;
                    insert_fileref_child(sf, sf2, TRUE);
                    
                    increase_fileref_refcount(sf);
                }
            } else {
                root* subvol;
                UINT64 inode, index;
                UINT8 type;
                ANSI_STRING utf8;
                
                Status = find_file_in_dir(Vcb, &parts[i], sf, &subvol, &inode, &type, &index, &utf8);
                if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
                    TRACE("could not find %.*S\n", parts[i].Length / sizeof(WCHAR), parts[i].Buffer);

                    Status = lastpart ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_OBJECT_PATH_NOT_FOUND;
                    goto end;
                } else if (!NT_SUCCESS(Status)) {
                    ERR("find_file_in_dir returned %08x\n", Status);
                    goto end;
                } else {
                    fcb* fcb;
                    ULONG strlen;
                    
                    if (type != BTRFS_TYPE_DIRECTORY && !lastpart) {
                        WARN("passed path including file as subdirectory\n");
                        
                        Status = STATUS_OBJECT_PATH_NOT_FOUND;
                        goto end;
                    }
                    
                    Status = open_fcb(Vcb, subvol, inode, type, &utf8, sf->fcb, &fcb);
                    if (!NT_SUCCESS(Status)) {
                        ERR("open_fcb returned %08x\n", Status);
                        goto end;
                    }
                    
                    sf2 = create_fileref();
                    if (!sf2) {
                        ERR("out of memory\n");
                        free_fcb(fcb);
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto end;
                    }
                    
                    sf2->fcb = fcb;
                    
                    if (type == BTRFS_TYPE_DIRECTORY)
                        fcb->fileref = sf2;
                    
                    sf2->index = index;
                    sf2->utf8 = utf8;
                    
                    Status = RtlUTF8ToUnicodeN(NULL, 0, &strlen, utf8.Buffer, utf8.Length);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                        free_fileref(sf2);
                        goto end;
                    }
                    
                    sf2->filepart.MaximumLength = sf2->filepart.Length = strlen;
                    sf2->filepart.Buffer = ExAllocatePoolWithTag(PagedPool, sf2->filepart.MaximumLength, ALLOC_TAG);
                    if (!sf2->filepart.Buffer) {
                        ERR("out of memory\n");
                        free_fileref(sf2);
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto end;
                    }
                    
                    Status = RtlUTF8ToUnicodeN(sf2->filepart.Buffer, strlen, &strlen, utf8.Buffer, utf8.Length);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                        free_fileref(sf2);
                        goto end;
                    }
                    
                    Status = RtlUpcaseUnicodeString(&sf2->filepart_uc, &sf2->filepart, TRUE);
                    if (!NT_SUCCESS(Status)) {
                        ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
                        free_fileref(sf2);
                        goto end;
                    }
                    
                    sf2->parent = (struct _file_ref*)sf;
                    
                    insert_fileref_child(sf, sf2, TRUE);
                    
                    increase_fileref_refcount(sf);
                }
            }
        }
        
        if (i == num_parts - 1)
            break;
        
        if (sf2->fcb->atts & FILE_ATTRIBUTE_REPARSE_POINT) {
            Status = STATUS_REPARSE;
            
            if (unparsed)
                *unparsed = fnus->Length - ((parts[i+1].Buffer - fnus->Buffer - 1) * sizeof(WCHAR));
            
            break;
        }
        
        free_fileref(sf);
        sf = sf2;
    }
    
    if (Status != STATUS_REPARSE)
        Status = STATUS_SUCCESS;
    *pfr = sf2;
    
end:
    ExReleaseResourceLite(&Vcb->fcb_lock);
    free_fileref(sf);
    
end2:
    if (parts)
        ExFreePool(parts);
    
    TRACE("returning %08x\n", Status);
    
    return Status;
}

NTSTATUS fcb_get_last_dir_index(fcb* fcb, UINT64* index) {
    KEY searchkey;
    traverse_ptr tp, prev_tp;
    NTSTATUS Status;
    
    ExAcquireResourceExclusiveLite(fcb->Header.Resource, TRUE);
    
    if (fcb->last_dir_index != 0) {
        *index = fcb->last_dir_index;
        fcb->last_dir_index++;
        Status = STATUS_SUCCESS;
        goto end;
    }
    
    searchkey.obj_id = fcb->inode;
    searchkey.obj_type = TYPE_DIR_INDEX + 1;
    searchkey.offset = 0;
    
    Status = find_item(fcb->Vcb, fcb->subvol, &tp, &searchkey, FALSE);
    if (!NT_SUCCESS(Status)) {
        ERR("error - find_item returned %08x\n", Status);
        goto end;
    }
    
    if (tp.item->key.obj_id > searchkey.obj_id || (tp.item->key.obj_id == searchkey.obj_id && tp.item->key.obj_type >= searchkey.obj_type)) {
        if (find_prev_item(fcb->Vcb, &tp, &prev_tp, FALSE))
            tp = prev_tp;
    }
    
    if (tp.item->key.obj_id == searchkey.obj_id && tp.item->key.obj_type == TYPE_DIR_INDEX) {
        fcb->last_dir_index = tp.item->key.offset + 1;
    } else
        fcb->last_dir_index = 2;
    
    *index = fcb->last_dir_index;
    fcb->last_dir_index++;
    
    Status = STATUS_SUCCESS;
    
end:
    ExReleaseResourceLite(fcb->Header.Resource);
    
    return Status;
}

static NTSTATUS STDCALL file_create2(PIRP Irp, device_extension* Vcb, PUNICODE_STRING fpus, file_ref* parfileref, ULONG options, file_ref** pfr, LIST_ENTRY* rollback) {
    NTSTATUS Status;
    fcb* fcb;
    ULONG utf8len;
    char* utf8 = NULL;
    UINT64 dirpos, inode;
    UINT8 type;
    LARGE_INTEGER time;
    BTRFS_TIME now;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG defda;
    file_ref* fileref;
    hardlink* hl;
#ifdef DEBUG_FCB_REFCOUNTS
    LONG rc;
#endif
    
    Status = RtlUnicodeToUTF8N(NULL, 0, &utf8len, fpus->Buffer, fpus->Length);
    if (!NT_SUCCESS(Status))
        return Status;
    
    utf8 = ExAllocatePoolWithTag(PagedPool, utf8len + 1, ALLOC_TAG);
    if (!utf8) {
        ERR("out of memory\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    Status = RtlUnicodeToUTF8N(utf8, utf8len, &utf8len, fpus->Buffer, fpus->Length);
    if (!NT_SUCCESS(Status)) {
        ExFreePool(utf8);
        return Status;
    }
    
    utf8[utf8len] = 0;
    
    Status = fcb_get_last_dir_index(parfileref->fcb, &dirpos);
    if (!NT_SUCCESS(Status)) {
        ERR("fcb_get_last_dir_index returned %08x\n", Status);
        ExFreePool(utf8);
        return Status;
    }
    
    KeQuerySystemTime(&time);
    win_time_to_unix(time, &now);
    
    TRACE("create file %.*S\n", fpus->Length / sizeof(WCHAR), fpus->Buffer);
    ExAcquireResourceExclusiveLite(parfileref->fcb->Header.Resource, TRUE);
    TRACE("parfileref->fcb->inode_item.st_size (inode %llx) was %llx\n", parfileref->fcb->inode, parfileref->fcb->inode_item.st_size);
    parfileref->fcb->inode_item.st_size += utf8len * 2;
    TRACE("parfileref->fcb->inode_item.st_size (inode %llx) now %llx\n", parfileref->fcb->inode, parfileref->fcb->inode_item.st_size);
    parfileref->fcb->inode_item.transid = Vcb->superblock.generation;
    parfileref->fcb->inode_item.sequence++;
    parfileref->fcb->inode_item.st_ctime = now;
    parfileref->fcb->inode_item.st_mtime = now;
    ExReleaseResourceLite(parfileref->fcb->Header.Resource);
    
    mark_fcb_dirty(parfileref->fcb);
    
    if (parfileref->fcb->subvol->lastinode == 0)
        get_last_inode(Vcb, parfileref->fcb->subvol);
    
    inode = parfileref->fcb->subvol->lastinode + 1;
    
    type = options & FILE_DIRECTORY_FILE ? BTRFS_TYPE_DIRECTORY : BTRFS_TYPE_FILE;
    
    // FIXME - link FILE_ATTRIBUTE_READONLY to st_mode
    
    TRACE("requested attributes = %x\n", IrpSp->Parameters.Create.FileAttributes);
    
    IrpSp->Parameters.Create.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    
    defda = 0;
    
    if (utf8[0] == '.')
        defda |= FILE_ATTRIBUTE_HIDDEN;
    
    if (options & FILE_DIRECTORY_FILE) {
        defda |= FILE_ATTRIBUTE_DIRECTORY;
        IrpSp->Parameters.Create.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }
    
    TRACE("defda = %x\n", defda);
    
    if (IrpSp->Parameters.Create.FileAttributes == FILE_ATTRIBUTE_NORMAL)
        IrpSp->Parameters.Create.FileAttributes = defda;
    
    parfileref->fcb->subvol->lastinode++;
    
    fcb = create_fcb();
    if (!fcb) {
        ERR("out of memory\n");
        ExFreePool(utf8);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
        
    fcb->Vcb = Vcb;
    
    fcb->inode_item.generation = Vcb->superblock.generation;
    fcb->inode_item.transid = Vcb->superblock.generation;
    fcb->inode_item.st_size = 0;
    fcb->inode_item.st_blocks = 0;
    fcb->inode_item.block_group = 0;
    fcb->inode_item.st_nlink = 1;
//     fcb->inode_item.st_uid = UID_NOBODY; // FIXME?
    fcb->inode_item.st_gid = GID_NOBODY; // FIXME?
    fcb->inode_item.st_mode = parfileref->fcb ? (parfileref->fcb->inode_item.st_mode & ~S_IFDIR) : 0755; // use parent's permissions by default
    fcb->inode_item.st_rdev = 0;
    fcb->inode_item.flags = 0;
    fcb->inode_item.sequence = 1;
    fcb->inode_item.st_atime = now;
    fcb->inode_item.st_ctime = now;
    fcb->inode_item.st_mtime = now;
    fcb->inode_item.otime = now;
    
    if (type == BTRFS_TYPE_DIRECTORY)
        fcb->inode_item.st_mode |= S_IFDIR;
    else {
        fcb->inode_item.st_mode |= S_IFREG;
        fcb->inode_item.st_mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH); // remove executable bit if not directory
    }
    
    // inherit nodatacow flag from parent directory
    if (parfileref->fcb->inode_item.flags & BTRFS_INODE_NODATACOW) {
        fcb->inode_item.flags |= BTRFS_INODE_NODATACOW;
        
        if (type != BTRFS_TYPE_DIRECTORY)
            fcb->inode_item.flags |= BTRFS_INODE_NODATASUM;
    }
    
    fcb->Header.IsFastIoPossible = fast_io_possible(fcb);
    fcb->Header.AllocationSize.QuadPart = 0;
    fcb->Header.FileSize.QuadPart = 0;
    fcb->Header.ValidDataLength.QuadPart = 0;
    
    fcb->atts = IrpSp->Parameters.Create.FileAttributes;
    fcb->atts_changed = fcb->atts != defda;
    
#ifdef DEBUG_FCB_REFCOUNTS
    rc = InterlockedIncrement(&parfileref->fcb->refcount);
    WARN("fcb %p: refcount now %i (%S)\n", parfileref->fcb, rc, file_desc_fileref(parfileref));
#else
    InterlockedIncrement(&parfileref->fcb->refcount);
#endif
    fcb->subvol = parfileref->fcb->subvol;
    fcb->inode = inode;
    fcb->type = type;
    
    Status = fcb_get_new_sd(fcb, parfileref, IrpSp->Parameters.Create.SecurityContext->AccessState);
    
    if (!NT_SUCCESS(Status)) {
        ERR("fcb_get_new_sd returned %08x\n", Status);
        free_fcb(fcb);
        return Status;
    }
    
    fcb->sd_dirty = TRUE;
    
    hl = ExAllocatePoolWithTag(PagedPool, sizeof(hardlink), ALLOC_TAG);
    if (!hl) {
        ERR("out of memory\n");
        free_fcb(fcb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    hl->parent = parfileref->fcb->inode;
    hl->index = dirpos;
    
    hl->utf8.Length = hl->utf8.MaximumLength = utf8len;
    hl->utf8.Buffer = ExAllocatePoolWithTag(PagedPool, utf8len, ALLOC_TAG);
    
    if (!hl->utf8.Buffer) {
        ERR("out of memory\n");
        ExFreePool(hl);
        free_fcb(fcb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(hl->utf8.Buffer, utf8, utf8len);
    
    hl->name.Length = hl->name.MaximumLength = fpus->Length;
    hl->name.Buffer = ExAllocatePoolWithTag(PagedPool, fpus->Length, ALLOC_TAG);
    
    if (!hl->name.Buffer) {
        ERR("out of memory\n");
        ExFreePool(hl->utf8.Buffer);
        ExFreePool(hl);
        free_fcb(fcb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlCopyMemory(hl->name.Buffer, fpus->Buffer, fpus->Length);
    
    InsertTailList(&fcb->hardlinks, &hl->list_entry);
    
    fileref = create_fileref();
    if (!fileref) {
        ERR("out of memory\n");
        free_fcb(fcb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    fileref->fcb = fcb;
    fileref->index = dirpos;

    fileref->utf8.MaximumLength = fileref->utf8.Length = utf8len;
    fileref->utf8.Buffer = utf8;
    
    fileref->filepart = *fpus;
    
    Status = RtlUpcaseUnicodeString(&fileref->filepart_uc, &fileref->filepart, TRUE);
    if (!NT_SUCCESS(Status)) {
        ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
        free_fileref(fileref);
        return Status;
    }
        
    if (Irp->Overlay.AllocationSize.QuadPart > 0) {
        Status = extend_file(fcb, fileref, Irp->Overlay.AllocationSize.QuadPart, TRUE, NULL, rollback);
        
        if (!NT_SUCCESS(Status)) {
            ERR("extend_file returned %08x\n", Status);
            free_fileref(fileref);
            return Status;
        }
    }
    
    fcb->created = TRUE;
    mark_fcb_dirty(fcb);
    
    fileref->created = TRUE;
    mark_fileref_dirty(fileref);
    
    fcb->subvol->root_item.ctransid = Vcb->superblock.generation;
    fcb->subvol->root_item.ctime = now;
    
    fileref->parent = parfileref;
    
    ExAcquireResourceExclusiveLite(&parfileref->nonpaged->children_lock, TRUE);
    
    if (IsListEmpty(&parfileref->children))
        InsertTailList(&parfileref->children, &fileref->list_entry);
    else {
        LIST_ENTRY* le = parfileref->children.Flink;
        file_ref* fr1 = CONTAINING_RECORD(le, file_ref, list_entry);
        
        while (le != &parfileref->children) {
            file_ref* fr2 = (le->Flink == &parfileref->children) ? NULL : CONTAINING_RECORD(le->Flink, file_ref, list_entry);
            
            if (fileref->index > fr1->index && (!fr2 || fr2->index > fileref->index)) {
                InsertHeadList(&fr1->list_entry, &fileref->list_entry);
                break;
            }
            
            fr1 = fr2;
            le = le->Flink;
        }
    }
    
    ExReleaseResourceLite(&parfileref->nonpaged->children_lock);
    
    increase_fileref_refcount(parfileref);
 
    InsertTailList(&fcb->subvol->fcbs, &fcb->list_entry);
    InsertTailList(&Vcb->all_fcbs, &fcb->list_entry_all);
    
    *pfr = fileref;
    
    if (type == BTRFS_TYPE_DIRECTORY)
        fileref->fcb->fileref = fileref;
    
    TRACE("created new file %S in subvol %llx, inode %llx\n", file_desc_fileref(fileref), fcb->subvol->id, fcb->inode);
    
    return STATUS_SUCCESS;
}

static NTSTATUS STDCALL file_create(PIRP Irp, device_extension* Vcb, PFILE_OBJECT FileObject, PUNICODE_STRING fnus, ULONG disposition, ULONG options, LIST_ENTRY* rollback) {
    NTSTATUS Status;
//     fcb *fcb, *parfcb = NULL;
    file_ref *fileref, *parfileref = NULL, *related;
    ULONG i, j;
//     ULONG utf8len;
    ccb* ccb;
    static WCHAR datasuf[] = {':','$','D','A','T','A',0};
    UNICODE_STRING dsus, fpus, stream;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PACCESS_STATE access_state = IrpSp->Parameters.Create.SecurityContext->AccessState;
#ifdef DEBUG_FCB_REFCOUNTS
    LONG oc;
#endif
            
    TRACE("(%p, %p, %p, %.*S, %x, %x)\n", Irp, Vcb, FileObject, fnus->Length / sizeof(WCHAR), fnus->Buffer, disposition, options);
    
    if (Vcb->readonly)
        return STATUS_MEDIA_WRITE_PROTECTED;
    
    dsus.Buffer = datasuf;
    dsus.Length = dsus.MaximumLength = wcslen(datasuf) * sizeof(WCHAR);
    fpus.Buffer = NULL;
    
    if (FileObject->RelatedFileObject && FileObject->RelatedFileObject->FsContext2) {
        struct _ccb* relatedccb = FileObject->RelatedFileObject->FsContext2;
        
        related = relatedccb->fileref;
    } else
        related = NULL;
    
    Status = open_fileref(Vcb, &parfileref, &FileObject->FileName, related, TRUE, NULL);
    if (!NT_SUCCESS(Status))
        goto end;
    
    if (parfileref->fcb->type != BTRFS_TYPE_DIRECTORY && (fnus->Length < sizeof(WCHAR) || fnus->Buffer[0] != ':')) {
        Status = STATUS_OBJECT_PATH_NOT_FOUND;
        goto end;
    }
    
    if (parfileref->fcb->subvol->root_item.flags & BTRFS_SUBVOL_READONLY) {
        Status = STATUS_ACCESS_DENIED;
        goto end;
    }
    
    i = (fnus->Length / sizeof(WCHAR))-1;
    while ((fnus->Buffer[i] == '\\' || fnus->Buffer[i] == '/') && i > 0) { i--; }
    
    j = i;
    
    while (i > 0 && fnus->Buffer[i-1] != '\\' && fnus->Buffer[i-1] != '/') { i--; }
    
    fpus.MaximumLength = (j - i + 2) * sizeof(WCHAR);
    fpus.Buffer = ExAllocatePoolWithTag(PagedPool, fpus.MaximumLength, ALLOC_TAG);
    if (!fpus.Buffer) {
        ERR("out of memory\n");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto end;
    }
    
    fpus.Length = (j - i + 1) * sizeof(WCHAR);
    
    RtlCopyMemory(fpus.Buffer, &fnus->Buffer[i], (j - i + 1) * sizeof(WCHAR));
    fpus.Buffer[j - i + 1] = 0;
    
    if (fpus.Length > dsus.Length) { // check for :$DATA suffix
        UNICODE_STRING lb;
                
        lb.Buffer = &fpus.Buffer[(fpus.Length - dsus.Length)/sizeof(WCHAR)];
        lb.Length = lb.MaximumLength = dsus.Length;
        
        TRACE("lb = %.*S\n", lb.Length/sizeof(WCHAR), lb.Buffer);
        
        if (FsRtlAreNamesEqual(&dsus, &lb, TRUE, NULL)) {
            TRACE("ignoring :$DATA suffix\n");
            
            fpus.Length -= lb.Length;
            
            if (fpus.Length > sizeof(WCHAR) && fpus.Buffer[(fpus.Length-1)/sizeof(WCHAR)] == ':')
                fpus.Length -= sizeof(WCHAR);
            
            TRACE("fpus = %.*S\n", fpus.Length / sizeof(WCHAR), fpus.Buffer);
        }
    }
    
    stream.Length = 0;
    
    for (i = 0; i < fpus.Length/sizeof(WCHAR); i++) {
        if (fpus.Buffer[i] == ':') {
            stream.Length = fpus.Length - (i*sizeof(WCHAR)) - sizeof(WCHAR);
            stream.Buffer = &fpus.Buffer[i+1];
            fpus.Buffer[i] = 0;
            fpus.Length = i * sizeof(WCHAR);
            break;
        }
    }
    
    if (stream.Length > 0) {
        file_ref* newpar;
        fcb* fcb;
        static char xapref[] = "user.";
        ULONG xapreflen = strlen(xapref);
        LARGE_INTEGER time;
        BTRFS_TIME now;
        ULONG utf8len;
#ifdef DEBUG_FCB_REFCOUNTS
        LONG rc;
#endif
        
        TRACE("fpus = %.*S\n", fpus.Length / sizeof(WCHAR), fpus.Buffer);
        TRACE("stream = %.*S\n", stream.Length / sizeof(WCHAR), stream.Buffer);
        
        Status = open_fileref(Vcb, &newpar, &fpus, parfileref, FALSE, NULL);
        
        if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
            UNICODE_STRING fpus2;
            
            if (!is_file_name_valid(&fpus))
                return STATUS_OBJECT_NAME_INVALID;
            
            fpus2.Length = fpus2.MaximumLength = fpus.Length;
            fpus2.Buffer = ExAllocatePoolWithTag(PagedPool, fpus2.MaximumLength, ALLOC_TAG);
            
            if (!fpus2.Buffer) {
                ERR("out of memory\n");
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto end;
            }
            
            RtlCopyMemory(fpus2.Buffer, fpus.Buffer, fpus2.Length);
            
            Status = file_create2(Irp, Vcb, &fpus2, parfileref, options, &newpar, rollback);
        
            if (!NT_SUCCESS(Status)) {
                ERR("file_create2 returned %08x\n", Status);
                ExFreePool(fpus2.Buffer);
                goto end;
            }
            
            send_notification_fileref(newpar, options & FILE_DIRECTORY_FILE ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME, FILE_ACTION_ADDED);
            send_notification_fcb(newpar->parent, FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);
        } else if (!NT_SUCCESS(Status)) {
            ERR("open_fileref returned %08x\n", Status);
            goto end;
        }
        
        free_fileref(parfileref);
        parfileref = newpar;
        
        if (parfileref->fcb->type != BTRFS_TYPE_FILE && parfileref->fcb->type != BTRFS_TYPE_SYMLINK) {
            WARN("parent not file or symlink\n");
            Status = STATUS_INVALID_PARAMETER;
            goto end;
        }
        
        if (options & FILE_DIRECTORY_FILE) {
            WARN("tried to create directory as stream\n");
            Status = STATUS_INVALID_PARAMETER;
            goto end;
        }
            
        fcb = create_fcb();
        if (!fcb) {
            ERR("out of memory\n");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }
        
        fcb->Vcb = Vcb;
        
        fcb->Header.IsFastIoPossible = fast_io_possible(fcb);
        fcb->Header.AllocationSize.QuadPart = 0;
        fcb->Header.FileSize.QuadPart = 0;
        fcb->Header.ValidDataLength.QuadPart = 0;
        
#ifdef DEBUG_FCB_REFCOUNTS
        rc = InterlockedIncrement(&parfileref->fcb->refcount);
        WARN("fcb %p: refcount now %i (%S)\n", parfileref->fcb, rc, file_desc_fileref(parfileref));
#else
        InterlockedIncrement(&parfileref->fcb->refcount);
#endif
        fcb->subvol = parfileref->fcb->subvol;
        fcb->inode = parfileref->fcb->inode;
        fcb->type = parfileref->fcb->type;
        
        fcb->ads = TRUE;
        
        Status = RtlUnicodeToUTF8N(NULL, 0, &utf8len, stream.Buffer, stream.Length);
        if (!NT_SUCCESS(Status)) {
            ERR("RtlUnicodeToUTF8N 1 returned %08x\n", Status);
            free_fcb(fcb);
            goto end;
        }
        
        fcb->adsxattr.Length = utf8len + xapreflen;
        fcb->adsxattr.MaximumLength = fcb->adsxattr.Length + 1;
        fcb->adsxattr.Buffer = ExAllocatePoolWithTag(PagedPool, fcb->adsxattr.MaximumLength, ALLOC_TAG);
        if (!fcb->adsxattr.Buffer) {
            ERR("out of memory\n");
            free_fcb(fcb);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }
        
        RtlCopyMemory(fcb->adsxattr.Buffer, xapref, xapreflen);
        
        Status = RtlUnicodeToUTF8N(&fcb->adsxattr.Buffer[xapreflen], utf8len, &utf8len, stream.Buffer, stream.Length);
        if (!NT_SUCCESS(Status)) {
            ERR("RtlUnicodeToUTF8N 2 returned %08x\n", Status);
            free_fcb(fcb);
            goto end;
        }
        
        fcb->adsxattr.Buffer[fcb->adsxattr.Length] = 0;
        
        TRACE("adsxattr = %s\n", fcb->adsxattr.Buffer);
        
        fcb->adshash = calc_crc32c(0xfffffffe, (UINT8*)fcb->adsxattr.Buffer, fcb->adsxattr.Length);
        TRACE("adshash = %08x\n", fcb->adshash);

        fileref = create_fileref();
        if (!fileref) {
            ERR("out of memory\n");
            free_fcb(fcb);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }
        
        fileref->fcb = fcb;

        fileref->filepart.MaximumLength = fileref->filepart.Length = stream.Length;
        fileref->filepart.Buffer = ExAllocatePoolWithTag(PagedPool, fileref->filepart.MaximumLength, ALLOC_TAG);
        if (!fileref->filepart.Buffer) {
            ERR("out of memory\n");
            free_fileref(fileref);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }
        
        RtlCopyMemory(fileref->filepart.Buffer, stream.Buffer, stream.Length);
        
        Status = RtlUpcaseUnicodeString(&fileref->filepart_uc, &fileref->filepart, TRUE);
        if (!NT_SUCCESS(Status)) {
            ERR("RtlUpcaseUnicodeString returned %08x\n", Status);
            free_fileref(fileref);
            goto end;
        }
       
        mark_fcb_dirty(fcb);
        
        InsertTailList(&fcb->subvol->fcbs, &fcb->list_entry);
        InsertTailList(&Vcb->all_fcbs, &fcb->list_entry_all);
        
        KeQuerySystemTime(&time);
        win_time_to_unix(time, &now);
        
        parfileref->fcb->inode_item.transid = Vcb->superblock.generation;
        parfileref->fcb->inode_item.sequence++;
        parfileref->fcb->inode_item.st_ctime = now;
        
        mark_fcb_dirty(parfileref->fcb);
        
        parfileref->fcb->subvol->root_item.ctransid = Vcb->superblock.generation;
        parfileref->fcb->subvol->root_item.ctime = now;
        
        fileref->parent = (struct _file_ref*)parfileref;
        
        ExAcquireResourceExclusiveLite(&parfileref->nonpaged->children_lock, TRUE);
        InsertTailList(&parfileref->children, &fileref->list_entry);
        ExReleaseResourceLite(&parfileref->nonpaged->children_lock);
        
        increase_fileref_refcount(parfileref);
        
        ExFreePool(fpus.Buffer);
        fpus.Buffer = NULL;
    } else {
        if (!is_file_name_valid(&fpus)) {
            Status = STATUS_OBJECT_NAME_INVALID;
            goto end;
        }
        
        Status = file_create2(Irp, Vcb, &fpus, parfileref, options, &fileref, rollback);
        
        if (!NT_SUCCESS(Status)) {
            ERR("file_create2 returned %08x\n", Status);
            goto end;
        }
        
        send_notification_fileref(fileref, options & FILE_DIRECTORY_FILE ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME, FILE_ACTION_ADDED);
        send_notification_fcb(fileref->parent, FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);
    }
    
    FileObject->FsContext = fileref->fcb;
    
    ccb = ExAllocatePoolWithTag(NonPagedPool, sizeof(*ccb), ALLOC_TAG);
    if (!ccb) {
        ERR("out of memory\n");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        free_fileref(fileref);
        goto end;
    }
    
    RtlZeroMemory(ccb, sizeof(*ccb));
    
    ccb->fileref = fileref;
    
    ccb->NodeType = BTRFS_NODE_TYPE_CCB;
    ccb->NodeSize = sizeof(ccb);
    ccb->disposition = disposition;
    ccb->options = options;
    ccb->query_dir_offset = 0;
    RtlInitUnicodeString(&ccb->query_string, NULL);
    ccb->has_wildcard = FALSE;
    ccb->specific_file = FALSE;
    ccb->access = access_state->OriginalDesiredAccess;
    
#ifdef DEBUG_FCB_REFCOUNTS
    oc = InterlockedIncrement(&fileref->fcb->open_count);
    ERR("fcb %p: open_count now %i\n", fileref->fcb, oc);
#else
    InterlockedIncrement(&fileref->fcb->open_count);
#endif
    
    FileObject->FsContext2 = ccb;

    FileObject->SectionObjectPointer = &fileref->fcb->nonpaged->segment_object;
    
//     TRACE("returning FCB %p with parent %p\n", fcb, parfcb);
    
//     ULONG fnlen;
// 
//     fcb->name_offset = fcb->par->full_filename.Length / sizeof(WCHAR);
//             
//     if (fcb->par != Vcb->root_fcb)
//         fcb->name_offset++;
//     
//     fnlen = (fcb->name_offset * sizeof(WCHAR)) + fcb->filepart.Length;
//     
//     fcb->full_filename.Buffer = ExAllocatePoolWithTag(PagedPool, fnlen, ALLOC_TAG);
//     if (!fcb->full_filename.Buffer) {
//         ERR("out of memory\n");
//         Status = STATUS_INSUFFICIENT_RESOURCES;
//         goto end;
//     }   
//     
//     fcb->full_filename.Length = fcb->full_filename.MaximumLength = fnlen;
//     RtlCopyMemory(fcb->full_filename.Buffer, fcb->par->full_filename.Buffer, fcb->par->full_filename.Length);
//     
//     if (fcb->par != Vcb->root_fcb)
//         fcb->full_filename.Buffer[fcb->par->full_filename.Length / sizeof(WCHAR)] = '\\';
//     
//     RtlCopyMemory(&fcb->full_filename.Buffer[fcb->name_offset], fcb->filepart.Buffer, fcb->filepart.Length);
        
    goto end2;
    
end:    
    if (fpus.Buffer)
        ExFreePool(fpus.Buffer);
    
end2:
    if (parfileref)
        free_fileref(parfileref);
    
    return Status;
}

static __inline void debug_create_options(ULONG RequestedOptions) {
    if (RequestedOptions != 0) {
        ULONG options = RequestedOptions;
        
        TRACE("requested options:\n");
        
        if (options & FILE_DIRECTORY_FILE) {
            TRACE("    FILE_DIRECTORY_FILE\n");
            options &= ~FILE_DIRECTORY_FILE;
        }

        if (options & FILE_WRITE_THROUGH) {
            TRACE("    FILE_WRITE_THROUGH\n");
            options &= ~FILE_WRITE_THROUGH;
        }

        if (options & FILE_SEQUENTIAL_ONLY) {
            TRACE("    FILE_SEQUENTIAL_ONLY\n");
            options &= ~FILE_SEQUENTIAL_ONLY;
        }

        if (options & FILE_NO_INTERMEDIATE_BUFFERING) {
            TRACE("    FILE_NO_INTERMEDIATE_BUFFERING\n");
            options &= ~FILE_NO_INTERMEDIATE_BUFFERING;
        }

        if (options & FILE_SYNCHRONOUS_IO_ALERT) {
            TRACE("    FILE_SYNCHRONOUS_IO_ALERT\n");
            options &= ~FILE_SYNCHRONOUS_IO_ALERT;
        }

        if (options & FILE_SYNCHRONOUS_IO_NONALERT) {
            TRACE("    FILE_SYNCHRONOUS_IO_NONALERT\n");
            options &= ~FILE_SYNCHRONOUS_IO_NONALERT;
        }

        if (options & FILE_NON_DIRECTORY_FILE) {
            TRACE("    FILE_NON_DIRECTORY_FILE\n");
            options &= ~FILE_NON_DIRECTORY_FILE;
        }

        if (options & FILE_CREATE_TREE_CONNECTION) {
            TRACE("    FILE_CREATE_TREE_CONNECTION\n");
            options &= ~FILE_CREATE_TREE_CONNECTION;
        }

        if (options & FILE_COMPLETE_IF_OPLOCKED) {
            TRACE("    FILE_COMPLETE_IF_OPLOCKED\n");
            options &= ~FILE_COMPLETE_IF_OPLOCKED;
        }

        if (options & FILE_NO_EA_KNOWLEDGE) {
            TRACE("    FILE_NO_EA_KNOWLEDGE\n");
            options &= ~FILE_NO_EA_KNOWLEDGE;
        }
        
        if (options & FILE_OPEN_REMOTE_INSTANCE) {
            TRACE("    FILE_OPEN_REMOTE_INSTANCE\n");
            options &= ~FILE_OPEN_REMOTE_INSTANCE;
        }

        if (options & FILE_RANDOM_ACCESS) {
            TRACE("    FILE_RANDOM_ACCESS\n");
            options &= ~FILE_RANDOM_ACCESS;
        }

        if (options & FILE_DELETE_ON_CLOSE) {
            TRACE("    FILE_DELETE_ON_CLOSE\n");
            options &= ~FILE_DELETE_ON_CLOSE;
        }

        if (options & FILE_OPEN_BY_FILE_ID) {
            TRACE("    FILE_OPEN_BY_FILE_ID\n");
            options &= ~FILE_OPEN_BY_FILE_ID;
        }

        if (options & FILE_OPEN_FOR_BACKUP_INTENT) {
            TRACE("    FILE_OPEN_FOR_BACKUP_INTENT\n");
            options &= ~FILE_OPEN_FOR_BACKUP_INTENT;
        }
        
        if (options & FILE_NO_COMPRESSION) {
            TRACE("    FILE_NO_COMPRESSION\n");
            options &= ~FILE_NO_COMPRESSION;
        }

#if NTDDI_VERSION >= NTDDI_WIN7
        if (options & FILE_OPEN_REQUIRING_OPLOCK) {
            TRACE("    FILE_OPEN_REQUIRING_OPLOCK\n");
            options &= ~FILE_OPEN_REQUIRING_OPLOCK;
        }
        
        if (options & FILE_DISALLOW_EXCLUSIVE) {
            TRACE("    FILE_DISALLOW_EXCLUSIVE\n");
            options &= ~FILE_DISALLOW_EXCLUSIVE;
        }
#endif

        if (options & FILE_RESERVE_OPFILTER) {
            TRACE("    FILE_RESERVE_OPFILTER\n");
            options &= ~FILE_RESERVE_OPFILTER;
        }

        if (options & FILE_OPEN_REPARSE_POINT) {
            TRACE("    FILE_OPEN_REPARSE_POINT\n");
            options &= ~FILE_OPEN_REPARSE_POINT;
        }
        
        if (options & FILE_OPEN_NO_RECALL) {
            TRACE("    FILE_OPEN_NO_RECALL\n");
            options &= ~FILE_OPEN_NO_RECALL;
        }
        
        if (options & FILE_OPEN_FOR_FREE_SPACE_QUERY) {
            TRACE("    FILE_OPEN_FOR_FREE_SPACE_QUERY\n");
            options &= ~FILE_OPEN_FOR_FREE_SPACE_QUERY;
        }
        
        if (options)
            TRACE("    unknown options: %x\n", options);
    } else {
        TRACE("requested options: (none)\n");
    }
}

static NTSTATUS get_reparse_block(fcb* fcb, UINT8** data) {
    NTSTATUS Status;
    
    if (fcb->type == BTRFS_TYPE_FILE || fcb->type == BTRFS_TYPE_SYMLINK) {
        ULONG size, bytes_read, i;
        
        if (fcb->inode_item.st_size < sizeof(ULONG)) {
            WARN("file was too short to be a reparse point\n");
            return STATUS_INVALID_PARAMETER;
        }
        
        // 0x10007 = 0xffff (maximum length of data buffer) + 8 bytes header
        size = min(0x10007, fcb->inode_item.st_size);
        
        *data = ExAllocatePoolWithTag(PagedPool, size, ALLOC_TAG);
        if (!*data) {
            ERR("out of memory\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        Status = read_file(fcb, *data, 0, size, &bytes_read, NULL);
        if (!NT_SUCCESS(Status)) {
            ERR("read_file_fcb returned %08x\n", Status);
            ExFreePool(*data);
            return Status;
        }
        
        if (fcb->type == BTRFS_TYPE_SYMLINK) {
            ULONG stringlen, subnamelen, printnamelen, reqlen;
            REPARSE_DATA_BUFFER* rdb;
            
            Status = RtlUTF8ToUnicodeN(NULL, 0, &stringlen, (char*)*data, bytes_read);
            if (!NT_SUCCESS(Status)) {
                ERR("RtlUTF8ToUnicodeN 1 returned %08x\n", Status);
                ExFreePool(*data);
                return Status;
            }
            
            subnamelen = stringlen;
            printnamelen = stringlen;
            
            reqlen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + subnamelen + printnamelen;
            
            rdb = ExAllocatePoolWithTag(PagedPool, reqlen, ALLOC_TAG);
            
            if (!rdb) {
                ERR("out of memory\n");
                ExFreePool(*data);
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            
            rdb->ReparseTag = IO_REPARSE_TAG_SYMLINK;
            rdb->ReparseDataLength = reqlen - offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer);
            rdb->Reserved = 0;
            
            rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
            rdb->SymbolicLinkReparseBuffer.SubstituteNameLength = subnamelen;
            rdb->SymbolicLinkReparseBuffer.PrintNameOffset = subnamelen;
            rdb->SymbolicLinkReparseBuffer.PrintNameLength = printnamelen;
            rdb->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;
            
            Status = RtlUTF8ToUnicodeN(&rdb->SymbolicLinkReparseBuffer.PathBuffer[rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)],
                                    stringlen, &stringlen, (char*)*data, size);

            if (!NT_SUCCESS(Status)) {
                ERR("RtlUTF8ToUnicodeN 2 returned %08x\n", Status);
                ExFreePool(rdb);
                ExFreePool(*data);
                return Status;
            }
            
            for (i = 0; i < stringlen / sizeof(WCHAR); i++) {
                if (rdb->SymbolicLinkReparseBuffer.PathBuffer[(rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)) + i] == '/')
                    rdb->SymbolicLinkReparseBuffer.PathBuffer[(rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)) + i] = '\\';
            }
            
            RtlCopyMemory(&rdb->SymbolicLinkReparseBuffer.PathBuffer[rdb->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(WCHAR)],
                        &rdb->SymbolicLinkReparseBuffer.PathBuffer[rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)],
                        rdb->SymbolicLinkReparseBuffer.SubstituteNameLength);
            
            ExFreePool(*data);
            
            *data = (UINT8*)rdb;
        } else {
            Status = FsRtlValidateReparsePointBuffer(bytes_read, (REPARSE_DATA_BUFFER*)*data);
            if (!NT_SUCCESS(Status)) {
                ERR("FsRtlValidateReparsePointBuffer returned %08x\n", Status);
                ExFreePool(*data);
                return Status;
            }
        }
    } else if (fcb->type == BTRFS_TYPE_DIRECTORY) {
        if (!fcb->reparse_xattr.Buffer || fcb->reparse_xattr.Length == 0)
            return STATUS_INTERNAL_ERROR;
            
        if (fcb->reparse_xattr.Length < sizeof(ULONG)) {
            WARN("xattr was too short to be a reparse point\n");
            return STATUS_INTERNAL_ERROR;
        }
        
        Status = FsRtlValidateReparsePointBuffer(fcb->reparse_xattr.Length, (REPARSE_DATA_BUFFER*)fcb->reparse_xattr.Buffer);
        if (!NT_SUCCESS(Status)) {
            ERR("FsRtlValidateReparsePointBuffer returned %08x\n", Status);
            return Status;
        }
        
        *data = ExAllocatePoolWithTag(PagedPool, fcb->reparse_xattr.Length, ALLOC_TAG);
        if (!*data) {
            ERR("out of memory\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        RtlCopyMemory(*data, fcb->reparse_xattr.Buffer, fcb->reparse_xattr.Length);
    }
    
    return STATUS_SUCCESS;
}

static NTSTATUS STDCALL open_file(PDEVICE_OBJECT DeviceObject, PIRP Irp, LIST_ENTRY* rollback) {
    PFILE_OBJECT FileObject;
    ULONG RequestedDisposition;
    ULONG options;
    NTSTATUS Status;
    ccb* ccb;
    device_extension* Vcb = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    USHORT unparsed;
    file_ref *related, *fileref;
#ifdef DEBUG_FCB_REFCOUNTS
    LONG oc;
#endif
    
    Irp->IoStatus.Information = 0;
    
    RequestedDisposition = ((Stack->Parameters.Create.Options >> 24) & 0xff);
    options = Stack->Parameters.Create.Options & FILE_VALID_OPTION_FLAGS;
    
    if (options & FILE_DIRECTORY_FILE && RequestedDisposition == FILE_SUPERSEDE) {
        WARN("error - supersede requested with FILE_DIRECTORY_FILE\n");
        Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    FileObject = Stack->FileObject;
    
    if (FileObject->RelatedFileObject && FileObject->RelatedFileObject->FsContext2) {
        struct _ccb* relatedccb = FileObject->RelatedFileObject->FsContext2;
        
        related = relatedccb->fileref;
    } else
        related = NULL;
    
    debug_create_options(options);
    
    switch (RequestedDisposition) {
        case FILE_SUPERSEDE:
            TRACE("requested disposition: FILE_SUPERSEDE\n");
            break;
            
        case FILE_CREATE:
            TRACE("requested disposition: FILE_CREATE\n");
            break;
            
        case FILE_OPEN:
            TRACE("requested disposition: FILE_OPEN\n");
            break;
            
        case FILE_OPEN_IF:
            TRACE("requested disposition: FILE_OPEN_IF\n");
            break;
            
        case FILE_OVERWRITE:
            TRACE("requested disposition: FILE_OVERWRITE\n");
            break;
            
        case FILE_OVERWRITE_IF:
            TRACE("requested disposition: FILE_OVERWRITE_IF\n");
            break;
            
        default:
            ERR("unknown disposition: %x\n", RequestedDisposition);
            Status = STATUS_NOT_IMPLEMENTED;
            goto exit;
    }
    
    TRACE("(%.*S)\n", FileObject->FileName.Length / sizeof(WCHAR), FileObject->FileName.Buffer);
    TRACE("FileObject = %p\n", FileObject);
    
    if (Vcb->readonly && (RequestedDisposition == FILE_SUPERSEDE || RequestedDisposition == FILE_CREATE || RequestedDisposition == FILE_OVERWRITE)) {
        Status = STATUS_MEDIA_WRITE_PROTECTED;
        goto exit;
    }
    
    // FIXME - if Vcb->readonly or subvol readonly, don't allow the write ACCESS_MASK flags
    
    if (options & FILE_OPEN_BY_FILE_ID) {
        if (FileObject->FileName.Length == sizeof(UINT64) && related && RequestedDisposition == FILE_OPEN) {
            UINT64 inode;
            
            RtlCopyMemory(&inode, FileObject->FileName.Buffer, sizeof(UINT64));
            
            if (related->fcb == Vcb->root_fileref->fcb && inode == 0)
                inode = Vcb->root_fileref->fcb->inode;
            
            if (inode == 0) { // we use 0 to mean the parent of a subvolume
                fileref = related->parent;
                increase_fileref_refcount(fileref);
                Status = STATUS_SUCCESS;
            } else
                Status = open_fileref_by_inode(Vcb, related->fcb->subvol, inode, &fileref);
        } else {
            WARN("FILE_OPEN_BY_FILE_ID only supported for inodes\n");
            Status = STATUS_NOT_IMPLEMENTED;
            goto exit;
        }
    } else {
        if (related && FileObject->FileName.Length != 0 && FileObject->FileName.Buffer[0] == '\\') {
            Status = STATUS_OBJECT_NAME_INVALID;
            goto exit;
        }
        
        Status = open_fileref(Vcb, &fileref, &FileObject->FileName, related, Stack->Flags & SL_OPEN_TARGET_DIRECTORY, &unparsed);
    }
    
    if (Status == STATUS_REPARSE) {
        REPARSE_DATA_BUFFER* data;
        
        ExAcquireResourceSharedLite(fileref->fcb->Header.Resource, TRUE);
        Status = get_reparse_block(fileref->fcb, (UINT8**)&data);
        ExReleaseResourceLite(fileref->fcb->Header.Resource);
        
        if (!NT_SUCCESS(Status)) {
            ERR("get_reparse_block returned %08x\n", Status);
            
            ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
            free_fileref(fileref);
            ExReleaseResourceLite(&Vcb->fcb_lock);
            goto exit;
        }
        
        Status = STATUS_REPARSE;
        RtlCopyMemory(&Irp->IoStatus.Information, data, sizeof(ULONG));
        
        data->Reserved = unparsed;
        
        Irp->Tail.Overlay.AuxiliaryBuffer = (void*)data;
        
        ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
        free_fileref(fileref);
        ExReleaseResourceLite(&Vcb->fcb_lock);
        goto exit;
    }
    
    if (NT_SUCCESS(Status) && fileref->deleted) {
        ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
        free_fileref(fileref);
        ExReleaseResourceLite(&Vcb->fcb_lock);
        
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
    }
    
    if (NT_SUCCESS(Status)) {
        if (RequestedDisposition == FILE_CREATE) {
            TRACE("file %S already exists, returning STATUS_OBJECT_NAME_COLLISION\n", file_desc_fileref(fileref));
            Status = STATUS_OBJECT_NAME_COLLISION;
            free_fileref(fileref);
            goto exit;
        }
    } else if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
        if (RequestedDisposition == FILE_OPEN || RequestedDisposition == FILE_OVERWRITE) {
            TRACE("file doesn't exist, returning STATUS_OBJECT_NAME_NOT_FOUND\n");
            goto exit;
        }
    } else {
        TRACE("open_fileref returned %08x\n", Status);
        goto exit;
    }
    
    if (NT_SUCCESS(Status)) { // file already exists
        file_ref* sf;
        
        if (RequestedDisposition == FILE_SUPERSEDE || RequestedDisposition == FILE_OVERWRITE || RequestedDisposition == FILE_OVERWRITE_IF) {
            if (fileref->fcb->type == BTRFS_TYPE_DIRECTORY || fileref->fcb->subvol->root_item.flags & BTRFS_SUBVOL_READONLY) {
                Status = STATUS_ACCESS_DENIED;
                free_fileref(fileref);
                goto exit;
            }
            
            if (Vcb->readonly) {
                Status = STATUS_MEDIA_WRITE_PROTECTED;
                free_fileref(fileref);
                goto exit;
            }
        }
        
        TRACE("deleted = %s\n", fileref->deleted ? "TRUE" : "FALSE");
        
        sf = fileref;
        while (sf) {
            if (sf->delete_on_close) {
                WARN("could not open as deletion pending\n");
                Status = STATUS_DELETE_PENDING;
                
                ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
                free_fileref(fileref);
                ExReleaseResourceLite(&Vcb->fcb_lock);
                goto exit;
            }
            sf = sf->parent;
        }
        
        if (fileref->fcb->atts & FILE_ATTRIBUTE_READONLY) {
            ACCESS_MASK allowed = DELETE | READ_CONTROL | WRITE_OWNER | WRITE_DAC |
                                    SYNCHRONIZE | ACCESS_SYSTEM_SECURITY | FILE_READ_DATA |
                                    FILE_READ_EA | FILE_WRITE_EA | FILE_READ_ATTRIBUTES |
                                    FILE_WRITE_ATTRIBUTES | FILE_EXECUTE | FILE_LIST_DIRECTORY |
                                    FILE_TRAVERSE;

            if (fileref->fcb->type == BTRFS_TYPE_DIRECTORY)
                allowed |= FILE_ADD_SUBDIRECTORY | FILE_ADD_FILE | FILE_DELETE_CHILD;
            
            if (Stack->Parameters.Create.SecurityContext->DesiredAccess & ~allowed) {
                Status = STATUS_ACCESS_DENIED;
                free_fileref(fileref);
                goto exit;
            }
        }
        
        if (options & FILE_NON_DIRECTORY_FILE && fileref->fcb->type == BTRFS_TYPE_DIRECTORY) {
            ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
            free_fileref(fileref);
            ExReleaseResourceLite(&Vcb->fcb_lock);
            
            Status = STATUS_FILE_IS_A_DIRECTORY;
            goto exit;
        } else if (options & FILE_DIRECTORY_FILE && fileref->fcb->type != BTRFS_TYPE_DIRECTORY) {
            TRACE("returning STATUS_NOT_A_DIRECTORY (type = %u, %S)\n", fileref->fcb->type, file_desc_fileref(fileref));
            
            ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
            free_fileref(fileref);
            ExReleaseResourceLite(&Vcb->fcb_lock);
            
            Status = STATUS_NOT_A_DIRECTORY;
            goto exit;
        }
        
        if (options & FILE_DELETE_ON_CLOSE && (fileref == Vcb->root_fileref || Vcb->readonly ||
            fileref->fcb->subvol->root_item.flags & BTRFS_SUBVOL_READONLY || fileref->fcb->atts & FILE_ATTRIBUTE_READONLY)) {
            Status = STATUS_CANNOT_DELETE;
            free_fileref(fileref);
            goto exit;
        }
        
        
        if ((fileref->fcb->type == BTRFS_TYPE_SYMLINK || fileref->fcb->atts & FILE_ATTRIBUTE_REPARSE_POINT) && !(options & FILE_OPEN_REPARSE_POINT))  {
            UINT8* data;
            
            /* How reparse points work from the point of view of the filesystem appears to
             * undocumented. When returning STATUS_REPARSE, MSDN encourages us to return
             * IO_REPARSE in Irp->IoStatus.Information, but that means we have to do our own
             * translation. If we instead return the reparse tag in Information, and store
             * a pointer to the reparse data buffer in Irp->Tail.Overlay.AuxiliaryBuffer,
             * IopSymlinkProcessReparse will do the translation for us. */
            
            Status = get_reparse_block(fileref->fcb, &data);
            if (!NT_SUCCESS(Status)) {
                ERR("get_reparse_block returned %08x\n", Status);
                
                ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
                free_fileref(fileref);
                ExReleaseResourceLite(&Vcb->fcb_lock);
                goto exit;
            }
            
            Status = STATUS_REPARSE;
            RtlCopyMemory(&Irp->IoStatus.Information, data, sizeof(ULONG));
            
            Irp->Tail.Overlay.AuxiliaryBuffer = (void*)data;
            
            ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
            free_fileref(fileref);
            ExReleaseResourceLite(&Vcb->fcb_lock);
            goto exit;
        }
        
        if (fileref->fcb->open_count > 0) {
            Status = IoCheckShareAccess(Stack->Parameters.Create.SecurityContext->DesiredAccess,
                                        Stack->Parameters.Create.ShareAccess, FileObject, &fileref->fcb->share_access, TRUE);
            
            if (!NT_SUCCESS(Status)) {
                WARN("IoCheckShareAccess failed, returning %08x\n", Status);
                
                ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
                free_fileref(fileref);
                ExReleaseResourceLite(&Vcb->fcb_lock);
                goto exit;
            }
        } else {
            IoSetShareAccess(Stack->Parameters.Create.SecurityContext->DesiredAccess,
                             Stack->Parameters.Create.ShareAccess, FileObject, &fileref->fcb->share_access);
        }

        if (Stack->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA || options & FILE_DELETE_ON_CLOSE) {
            if (!MmFlushImageSection(&fileref->fcb->nonpaged->segment_object, MmFlushForWrite)) {
                Status = (options & FILE_DELETE_ON_CLOSE) ? STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION;
                
                ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
                free_fileref(fileref);
                ExReleaseResourceLite(&Vcb->fcb_lock);
                goto exit;
            }
        }
        
        if (RequestedDisposition == FILE_OVERWRITE || RequestedDisposition == FILE_OVERWRITE_IF || RequestedDisposition == FILE_SUPERSEDE) {
            ULONG defda, oldatts, filter;
            LARGE_INTEGER time;
            BTRFS_TIME now;
            
            if ((RequestedDisposition == FILE_OVERWRITE || RequestedDisposition == FILE_OVERWRITE_IF) && fileref->fcb->atts & FILE_ATTRIBUTE_READONLY) {
                WARN("cannot overwrite readonly file\n");
                Status = STATUS_ACCESS_DENIED;
                free_fileref(fileref);
                goto exit;
            }
    
            // FIXME - where did we get this from?
//             if (fcb->refcount > 1) {
//                 WARN("cannot overwrite open file (fcb = %p, refcount = %u)\n", fcb, fcb->refcount);
//                 Status = STATUS_ACCESS_DENIED;
//                 free_fcb(fcb);
//                 goto exit;
//             }
            
            // FIXME - make sure not ADS!
            Status = truncate_file(fileref->fcb, fileref->fcb->inode_item.st_size, rollback);
            if (!NT_SUCCESS(Status)) {
                ERR("truncate_file returned %08x\n", Status);
                free_fileref(fileref);
                goto exit;
            }
            
            if (Irp->Overlay.AllocationSize.QuadPart > 0) {
                Status = extend_file(fileref->fcb, fileref, Irp->Overlay.AllocationSize.QuadPart, TRUE, NULL, rollback);
                
                if (!NT_SUCCESS(Status)) {
                    ERR("extend_file returned %08x\n", Status);
                    free_fileref(fileref);
                    goto exit;
                }
            }
            
            filter = FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
            
            mark_fcb_dirty(fileref->fcb);
            
            oldatts = fileref->fcb->atts;
            
            defda = get_file_attributes(Vcb, &fileref->fcb->inode_item, fileref->fcb->subvol, fileref->fcb->inode, fileref->fcb->type,
                                        fileref->filepart.Length > 0 && fileref->filepart.Buffer[0] == '.', TRUE);
            
            if (RequestedDisposition == FILE_SUPERSEDE)
                fileref->fcb->atts = Stack->Parameters.Create.FileAttributes | FILE_ATTRIBUTE_ARCHIVE;
            else
                fileref->fcb->atts |= Stack->Parameters.Create.FileAttributes | FILE_ATTRIBUTE_ARCHIVE;
            
            if (fileref->fcb->atts != oldatts) {
                fileref->fcb->atts_changed = TRUE;
                fileref->fcb->atts_deleted = Stack->Parameters.Create.FileAttributes == defda;
                filter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
            }
            
            KeQuerySystemTime(&time);
            win_time_to_unix(time, &now);
            
            fileref->fcb->inode_item.transid = Vcb->superblock.generation;
            fileref->fcb->inode_item.sequence++;
            fileref->fcb->inode_item.st_ctime = now;
            fileref->fcb->inode_item.st_mtime = now;

            // FIXME - truncate streams
            // FIXME - do we need to alter parent directory's times?
            
            send_notification_fcb(fileref, filter, FILE_ACTION_MODIFIED);
        }
    
        FileObject->FsContext = fileref->fcb;
        
        ccb = ExAllocatePoolWithTag(NonPagedPool, sizeof(*ccb), ALLOC_TAG);
        if (!ccb) {
            ERR("out of memory\n");
            
            ExAcquireResourceExclusiveLite(&Vcb->fcb_lock, TRUE);
            free_fileref(fileref);
            ExReleaseResourceLite(&Vcb->fcb_lock);

            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        
        RtlZeroMemory(ccb, sizeof(*ccb));
        
        ccb->NodeType = BTRFS_NODE_TYPE_CCB;
        ccb->NodeSize = sizeof(ccb);
        ccb->disposition = RequestedDisposition;
        ccb->options = options;
        ccb->query_dir_offset = 0;
        RtlInitUnicodeString(&ccb->query_string, NULL);
        ccb->has_wildcard = FALSE;
        ccb->specific_file = FALSE;
        ccb->access = Stack->Parameters.Create.SecurityContext->DesiredAccess;
        
        ccb->fileref = fileref;
        
        FileObject->FsContext2 = ccb;
        
        FileObject->SectionObjectPointer = &fileref->fcb->nonpaged->segment_object;
        
        if (NT_SUCCESS(Status)) {
            switch (RequestedDisposition) {
                case FILE_SUPERSEDE:
                    Irp->IoStatus.Information = FILE_SUPERSEDED;
                    break;
                    
                case FILE_OPEN:
                case FILE_OPEN_IF:
                    Irp->IoStatus.Information = FILE_OPENED;
                    break;
                    
                case FILE_OVERWRITE:
                case FILE_OVERWRITE_IF:
                    Irp->IoStatus.Information = FILE_OVERWRITTEN;
                    break;
            }
        }
        
#ifdef DEBUG_FCB_REFCOUNTS
        oc = InterlockedIncrement(&fileref->fcb->open_count);
        ERR("fcb %p: open_count now %i\n", fileref->fcb, oc);
#else
        InterlockedIncrement(&fileref->fcb->open_count);
#endif
    } else {
        Status = file_create(Irp, DeviceObject->DeviceExtension, FileObject, &FileObject->FileName, RequestedDisposition, options, rollback);
        Irp->IoStatus.Information = NT_SUCCESS(Status) ? FILE_CREATED : 0;
    }
    
    if (NT_SUCCESS(Status) && !(options & FILE_NO_INTERMEDIATE_BUFFERING))
        FileObject->Flags |= FO_CACHE_SUPPORTED;
    
exit:
    if (NT_SUCCESS(Status)) {
        if (!FileObject->Vpb)
            FileObject->Vpb = DeviceObject->Vpb;
    } else {
        if (Status != STATUS_OBJECT_NAME_NOT_FOUND && Status != STATUS_OBJECT_PATH_NOT_FOUND)
            TRACE("returning %08x\n", Status);
    }
    
    return Status;
}

NTSTATUS STDCALL drv_create(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    device_extension* Vcb = DeviceObject->DeviceExtension;
    BOOL top_level, locked = FALSE;
    LIST_ENTRY rollback;
    
    TRACE("create (flags = %x)\n", Irp->Flags);
    
    InitializeListHead(&rollback);
    
    FsRtlEnterFileSystem();
    
    top_level = is_top_level(Irp);
    
    /* return success if just called for FS device object */
    if (DeviceObject == devobj || (Vcb && Vcb->type == VCB_TYPE_PARTITION0))  {
        TRACE("create called for FS device object\n");
        
        Irp->IoStatus.Information = FILE_OPENED;
        Status = STATUS_SUCCESS;

        goto exit;
    }
    
    Vcb = DeviceObject->DeviceExtension;
    ExAcquireResourceSharedLite(&Vcb->load_lock, TRUE);
    locked = TRUE;
    
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    
    if (IrpSp->Flags != 0) {
        UINT32 flags = IrpSp->Flags;
        
        TRACE("flags:\n");
        
        if (flags & SL_CASE_SENSITIVE) {
            TRACE("SL_CASE_SENSITIVE\n");
            flags &= ~SL_CASE_SENSITIVE;
        }
        
        if (flags & SL_FORCE_ACCESS_CHECK) {
            TRACE("SL_FORCE_ACCESS_CHECK\n");
            flags &= ~SL_FORCE_ACCESS_CHECK;
        }
        
        if (flags & SL_OPEN_PAGING_FILE) {
            TRACE("SL_OPEN_PAGING_FILE\n");
            flags &= ~SL_OPEN_PAGING_FILE;
        }
        
        if (flags & SL_OPEN_TARGET_DIRECTORY) {
            TRACE("SL_OPEN_TARGET_DIRECTORY\n");
            flags &= ~SL_OPEN_TARGET_DIRECTORY;
        }
        
        if (flags & SL_STOP_ON_SYMLINK) {
            TRACE("SL_STOP_ON_SYMLINK\n");
            flags &= ~SL_STOP_ON_SYMLINK;
        }
        
        if (flags)
            WARN("unknown flags: %x\n", flags);
    } else {
        TRACE("flags: (none)\n");
    }
    
//     Vpb = DeviceObject->DeviceExtension;
    
//     TRACE("create called for something other than FS device object\n");
    
    // opening volume
    // FIXME - also check if RelatedFileObject is Vcb
    if (IrpSp->FileObject->FileName.Length == 0 && !IrpSp->FileObject->RelatedFileObject) {
        ULONG RequestedDisposition = ((IrpSp->Parameters.Create.Options >> 24) & 0xff);
        ULONG RequestedOptions = IrpSp->Parameters.Create.Options & FILE_VALID_OPTION_FLAGS;
#ifdef DEBUG_FCB_REFCOUNTS
        LONG rc, oc;
#endif
        
        TRACE("open operation for volume\n");

        if (RequestedDisposition != FILE_OPEN &&
            RequestedDisposition != FILE_OPEN_IF)
        {
            Status = STATUS_ACCESS_DENIED;
            goto exit;
        }

        if (RequestedOptions & FILE_DIRECTORY_FILE)
        {
            Status = STATUS_NOT_A_DIRECTORY;
            goto exit;
        }

#ifdef DEBUG_FCB_REFCOUNTS
        rc = InterlockedIncrement(&Vcb->volume_fcb->refcount);
        oc = InterlockedIncrement(&Vcb->volume_fcb->open_count);
        WARN("fcb %p: refcount now %i (volume)\n", Vcb->volume_fcb, rc);
        WARN("fcb %p: open_count now %i (volume)\n", Vcb->volume_fcb, oc);
#else
        InterlockedIncrement(&Vcb->volume_fcb->refcount);
        InterlockedIncrement(&Vcb->volume_fcb->open_count);
#endif
        IrpSp->FileObject->FsContext = Vcb->volume_fcb;
        
        IrpSp->FileObject->SectionObjectPointer = &Vcb->volume_fcb->nonpaged->segment_object;

        if (!IrpSp->FileObject->Vpb)
            IrpSp->FileObject->Vpb = DeviceObject->Vpb;

        Irp->IoStatus.Information = FILE_OPENED;
        Status = STATUS_SUCCESS;
    } else {
        BOOL skip_lock;
        
        TRACE("file name: %.*S\n", IrpSp->FileObject->FileName.Length / sizeof(WCHAR), IrpSp->FileObject->FileName.Buffer);
        
        if (IrpSp->FileObject->RelatedFileObject)
            TRACE("related file = %S\n", file_desc(IrpSp->FileObject->RelatedFileObject));
        
        // Don't lock again if we're being called from within CcCopyRead etc.
        skip_lock = ExIsResourceAcquiredExclusiveLite(&Vcb->tree_lock);

        if (!skip_lock)
            ExAcquireResourceSharedLite(&Vcb->tree_lock, TRUE);
        
//         ExAcquireResourceExclusiveLite(&Vpb->DirResource, TRUE);
    //     Status = NtfsCreateFile(DeviceObject,
    //                             Irp);
        Status = open_file(DeviceObject, Irp, &rollback);
//         ExReleaseResourceLite(&Vpb->DirResource);
        
        if (!NT_SUCCESS(Status))
            do_rollback(Vcb, &rollback);
        else
            clear_rollback(&rollback);
        
        if (!skip_lock)
            ExReleaseResourceLite(&Vcb->tree_lock);
        
//         Status = STATUS_ACCESS_DENIED;
    }
    
exit:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest( Irp, NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT );
//     IoCompleteRequest( Irp, IO_DISK_INCREMENT );
    
    TRACE("create returning %08x\n", Status);
    
    if (locked)
        ExReleaseResourceLite(&Vcb->load_lock);
    
    if (top_level) 
        IoSetTopLevelIrp(NULL);
    
    FsRtlExitFileSystem();

    return Status;
}
