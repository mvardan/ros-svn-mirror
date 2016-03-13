#define __ROS_LONG64__

#define STANDALONE
#include <apitest.h>

extern void func_dosdev(void);
extern void func_FindFiles(void);
extern void func_GetCurrentDirectory(void);
extern void func_GetDriveType(void);
extern void func_GetModuleFileName(void);
extern void func_interlck(void);
extern void func_lstrcpynW(void);
extern void func_Mailslot(void);
extern void func_MultiByteToWideChar(void);
extern void func_PrivMoveFileIdentityW(void);
extern void func_SetCurrentDirectory(void);
extern void func_SetUnhandledExceptionFilter(void);
extern void func_TerminateProcess(void);
extern void func_TunnelCache(void);

const struct test winetest_testlist[] =
{
    { "dosdev",                      func_dosdev },
    { "FindFiles",                   func_FindFiles },
    { "GetCurrentDirectory",         func_GetCurrentDirectory },
    { "GetDriveType",                func_GetDriveType },
    { "GetModuleFileName",           func_GetModuleFileName },
    { "interlck",                    func_interlck },
    { "lstrcpynW",                   func_lstrcpynW },
    { "MailslotRead",                func_Mailslot },
    { "MultiByteToWideChar",         func_MultiByteToWideChar },
    { "PrivMoveFileIdentityW",       func_PrivMoveFileIdentityW },
    { "SetCurrentDirectory",         func_SetCurrentDirectory },
    { "SetUnhandledExceptionFilter", func_SetUnhandledExceptionFilter },
    { "TerminateProcess",            func_TerminateProcess },
    { "TunnelCache",                 func_TunnelCache },
    { 0, 0 }
};

