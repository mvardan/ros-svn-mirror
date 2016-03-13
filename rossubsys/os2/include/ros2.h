/* $     $
*/
/*
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS OS/2 sub system
 * FILE:             dll/doscalls.c
 * PURPOSE:          supportheader for Kernelservices.
 *						Use this file if your dll or application
 *						is going to use both, the os2 base services
 *						as well as the native API of NTDLL.DLL
 * PROGRAMMER:       Robert K. nonvolatil@yahoo.de
 * REVISION HISTORY:
 *    1-08-2002  Created
 */

/*   How to use:
	History got us another time. If you want to write a module
	that uses either native-api or os2-services...no problem.
	But if you want to use both of them you run into type conflicts.
	BOOL is not very surprising. But there are many other types 
	whose names are equal. This results from the fact that NT
	should originally become OS/2 3.0. 
	So your solution lies in including this file instead of os2.h
	and ntddk.h. What this file here does is, it puts the os2-types+
	functions into the namespace OS2 while the nt-types+functions are
	put into the namespace NT. The still conflicting #defines are handled
	like this:  If ntddk.h defintes a symbol FOO and os2.h does the same,
	this file here undefines the ntddk.h-one and renames it to NT_FOO.
	This is only done for conflicting symbols. Of course, this list is a 
	source for errors. But is there a better solution than renaming 
	all of the symbols?
*/

#ifndef __ROS2_H__
#define __ROS2_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


namespace NT
{
#include <ddk/ntddk.h>
#include <napi/types.h>
}


/* rename all the symbols */
#define	NT_FILE_CREATE		FILE_CREATE
#undef	FILE_CREATE
#define	NT_CREATE_SUSPENDED		CREATE_SUSPENDED
#undef	CREATE_SUSPENDED

namespace OS2
{
#include "os2.h"
}

using namespace OS2;

APIRET STDCALL  Dos32Open(PSZ    pszFileName,  PHFILE pHf,
                            PULONG pulAction,  ULONG  cbFile,
                            ULONG  ulAttribute,  ULONG  fsOpenFlags,
                            ULONG  fsOpenMode,  PVOID reserved );  //ULONGPEAOP2 peaop2)
APIRET STDCALL  Dos32Close(HFILE hFile);
APIRET STDCALL  Dos32Read(HFILE hFile, PVOID pBuffer,
                            ULONG cbRead, PULONG pcbActual);
APIRET STDCALL  Dos32Write(HFILE hFile, PVOID pBuffer,
                             ULONG cbWrite, PULONG pcbActual);
ULONG STDCALL DosSleep (ULONG ulInterval);
ULONG STDCALL DosBeep (ULONG ulFrequency, ULONG ulDuration);
VOID STDCALL DosExit (ULONG ulAction, ULONG ulResult);

//ULONG STDCALL DosDevIOCtl (HFILE hDevice, ULONG ulCategory, ULONG ulFunction,
//    PVOID pParams, ULONG ulParamsLengthMax, PULONG pulParamsLength,
//    PVOID pData, ULONG ulDataLengthMax, PULONG pulDataLength);

typedef ULONG foo_TID;
typedef foo_TID* foo_PTID;
typedef void (*foo_PFNTHREAD)(ULONG ulThreadArg);

ULONG STDCALL DosCreateThread (foo_PTID ptidThreadID, foo_PFNTHREAD pfnThreadAddr,
    ULONG ulThreadArg, ULONG ulFlags, ULONG ulStackSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ROS2_H__ */
