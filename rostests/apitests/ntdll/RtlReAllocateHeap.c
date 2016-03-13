/*
 * PROJECT:         ReactOS api tests
 * LICENSE:         LGPLv2.1+ - See COPYING.LIB in the top level directory
 * PURPOSE:         Test for RtlReAllocateHeap
 * PROGRAMMERS:     Thomas Faber <thomas.faber@reactos.org>
 */

#include <apitest.h>
#include <ndk/rtlfuncs.h>

static
BOOLEAN
CheckBuffer(
    PVOID Buffer,
    SIZE_T Size,
    UCHAR Value)
{
    PUCHAR Array = Buffer;
    SIZE_T i;

    for (i = 0; i < Size; i++)
        if (Array[i] != Value)
        {
            trace("Expected %x, found %x at offset %lu\n", Value, Array[i], (ULONG)i);
            return FALSE;
        }
    return TRUE;
}

static
BOOLEAN
ReAllocBuffer(
    PUCHAR *Buffer,
    SIZE_T Size,
    SIZE_T *OldSizePtr,
    PCSTR Action)
{
    PUCHAR NewBuffer;
    SIZE_T OldSize = *OldSizePtr;

    RtlFillMemory(*Buffer, OldSize, 0x7a);
    NewBuffer = RtlReAllocateHeap(RtlGetProcessHeap(),
                                  HEAP_ZERO_MEMORY,
                                  *Buffer,
                                  Size);
    if (!NewBuffer)
    {
        skip("RtlReAllocateHeap failed for size %lu (%s)\n", Size, Action);
        return FALSE;
    }
    *Buffer = NewBuffer;
    ok_hex(RtlSizeHeap(RtlGetProcessHeap(), 0, NewBuffer), Size);
    if (OldSize < Size)
    {
        ok(CheckBuffer(NewBuffer, OldSize, 0x7a), "CheckBuffer failed at size 0x%lx -> 0x%lx\n", OldSize, Size);
        ok(CheckBuffer(NewBuffer + OldSize, Size - OldSize, 0), "HEAP_ZERO_MEMORY not respected for 0x%lx -> 0x%lx\n", OldSize, Size);
    }
    else
    {
        ok(CheckBuffer(NewBuffer, Size, 0x7a), "CheckBuffer failed at size 0x%lx -> 0x%lx\n", OldSize, Size);
    }
    *OldSizePtr = Size;
    return TRUE;
}

START_TEST(RtlReAllocateHeap)
{
    PUCHAR Buffer = NULL;
    SIZE_T OldSize = 0;
    SIZE_T Size;
    BOOLEAN Continue = TRUE;

    OldSize = 0x100;
    Buffer = RtlReAllocateHeap(RtlGetProcessHeap(),
                               HEAP_ZERO_MEMORY,
                               NULL,
                               OldSize);
    ok(Buffer == NULL, "RtlReAllocateHeap succeeded for NULL\n");
    if (Buffer)
        RtlFreeHeap(RtlGetProcessHeap(), 0, Buffer);

    Buffer = RtlAllocateHeap(RtlGetProcessHeap(),
                             HEAP_ZERO_MEMORY,
                             OldSize);
    if (!Buffer)
    {
        skip("RtlAllocateHeap failed for size %lu\n", OldSize);
        return;
    }
    ok(CheckBuffer(Buffer, OldSize, 0), "HEAP_ZERO_MEMORY not respected for 0x%lx\n", OldSize);

    for (Size = 0x78000; Size < 0x90000 && Continue; Size += 0x100)
    {
        Continue = ReAllocBuffer(&Buffer, Size, &OldSize, "growing");
    }

    /* and back again */
    for (Size -= 0x100; Size >= 0x78000 && Continue; Size -= 0x100)
    {
        Continue = ReAllocBuffer(&Buffer, Size, &OldSize, "shrinking");
    }
    RtlFreeHeap(RtlGetProcessHeap(), 0, Buffer);
}
