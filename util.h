/*
 * util.h: global utility stuff for tpop3d
 *
 * Copyright (c) 2000 Chris Lightfoot. All rights reserved.
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  2000/10/28 14:57:04  chris
 * Initial revision
 *
 *
 */

#ifndef __UTIL_H_ /* include guard */
#define __UTIL_H_

#ifndef TPOP3D_VERSION
#   define TPOP3D_VERSION   "(unknown)"
#endif

/*
#define strdup(a)   mystrdup(__FILE__, __LINE__, (a))
#define malloc(a)   mymalloc(__FILE__, __LINE__ ,(a))
#define free(a)     myfree(__FILE__, __LINE__, (a))
*/

char *mystrdup(char *, int, char *);
void *mymalloc(char *, int, size_t);
void myfree(char *, int, void *);

#endif /* __UTIL_H_ */