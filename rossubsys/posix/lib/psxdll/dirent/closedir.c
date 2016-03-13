/* $Id: closedir.c,v 1.4 2002/10/29 04:45:28 rex Exp $
 */
/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS POSIX+ Subsystem
 * FILE:        subsys/psx/lib/psxdll/dirent/closedir.c
 * PURPOSE:     Close a directory stream
 * PROGRAMMER:  KJK::Hyperion <noog@libero.it>
 * UPDATE HISTORY:
 *              01/02/2002: Created
 *              13/02/2002: KJK::Hyperion: modified to use file descriptors
 */

#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <psx/dirent.h>
#include <psx/safeobj.h>

int closedir(DIR *dirp)
{
 /* check the "magic" signature */
 if(!__safeobj_validate(dirp, __IDIR_MAGIC))
 {
  errno = EBADF;
  return (-1);
 }

 /* this will close the handle, deallocate the internal object and
    invalidate the descriptor */
 return (close(((struct __internal_DIR *)dirp)->fildes));
}

/* EOF */

