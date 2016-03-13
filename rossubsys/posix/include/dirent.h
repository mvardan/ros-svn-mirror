/* $Id: dirent.h,v 1.6 2002/10/29 04:45:08 rex Exp $
 */
/*
 * dirent.h
 *
 * format of directory entries. Conforming to the Single UNIX(r)
 * Specification Version 2, System Interface & Headers Issue 5
 *
 * This file is part of the ReactOS Operating System.
 *
 * Contributors:
 *  Created by KJK::Hyperion <noog@libero.it>
 *
 *  THIS SOFTWARE IS NOT COPYRIGHTED
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#ifndef __DIRENT_H_INCLUDED__
#define __DIRENT_H_INCLUDED__

/* INCLUDES */
#include <sys/types.h>
#include <stddef.h>
#include <limits.h>

/* TYPES */
typedef void DIR;

#define NAME_MAX (255)

struct dirent
{
 ino_t d_ino;               /* file serial number */
 char * d_name /* [NAME_MAX + 1] */; /* name of entry */
};

/* for Unicode filenames */
struct _Wdirent
{
 ino_t    d_ino;               /* file serial number */
 wchar_t * d_name/* [NAME_MAX + 1] */; /* name of entry */
};

/* CONSTANTS */

/* PROTOTYPES */
int            closedir(DIR *);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *, struct dirent *, struct dirent **);
void           rewinddir(DIR *);
void           seekdir(DIR *, long int);
long int       telldir(DIR *);

/* for Unicode filenames */
DIR             *_Wopendir(const wchar_t *);
struct _Wdirent *_Wreaddir(DIR *);
int              _Wreaddir_r(DIR *, struct _Wdirent *, struct _Wdirent **);


/* MACROS */

#endif /* __DIRENT_H_INCLUDED__ */

/* EOF */

