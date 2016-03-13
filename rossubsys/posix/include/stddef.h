/* $Id: stddef.h,v 1.5 2003/01/05 18:27:19 robd Exp $
 */
/*
 * stddef.h
 *
 * standard type definitions. Conforming to the Single UNIX(r) Specification
 * Version 2, System Interface & Headers Issue 5
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
#ifndef __STDDEF_H_INCLUDED__
#define __STDDEF_H_INCLUDED__

/* INCLUDES */
#include <sys/types.h>

/* OBJECTS */

/* TYPES */
typedef signed long int ptrdiff_t;

#ifndef _WCHAR_T_
#define _WCHAR_T_
#define _WCHAR_T
#define _WCHAR_T_DEFINED
#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__      short unsigned int
#endif
#ifndef __cplusplus
typedef __WCHAR_TYPE__      wchar_t;
#endif  /* C++ */
#endif  /* _WCHAR_T_ */

//typedef unsigned short int wchar_t;

/* CONSTANTS */
#ifndef NULL
#define NULL ((void *)(0)) /* Null pointer constant. */
#endif

/* PROTOTYPES */

/* MACROS */
#define offsetof(t,m) ((size_t) &((t *)0)->m)

#endif /* __STDDEF_H_INCLUDED__ */

/* EOF */

