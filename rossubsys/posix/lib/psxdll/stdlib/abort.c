/* $Id: abort.c,v 1.4 2002/10/29 04:45:41 rex Exp $
 */
/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS POSIX+ Subsystem
 * FILE:        subsys/psx/lib/psxdll/stdlib/abort.c
 * PURPOSE:     Generate an abnormal process abort
 * PROGRAMMER:  KJK::Hyperion <noog@libero.it>
 * UPDATE HISTORY:
 *              15/02/2002: Created
 */

#include <stdlib.h>
#include <signal.h>

void abort(void)
{
 raise(SIGABRT);
}

/* EOF */

