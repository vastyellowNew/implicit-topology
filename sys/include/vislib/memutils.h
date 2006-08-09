/*
 * memutils.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 * Copyright (C) 2005 by Christoph Mueller (christoph.mueller@vis.uni-stuttgart.de). Alle Rechte vorbehalten.
 */

#ifndef VISLIB_MEMUTILS_H_INCLUDED
#define VISLIB_MEMUTILS_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include <memory.h>


#ifndef NULL
#ifdef __cplusplus
#define NULL (0)
#else
#define NULL ((void *) 0)
#endif /* __cplusplus */
#endif /* NULL */


/**
 * Delete memory designated by 'ptr' and set 'ptr' NULL.
 */
#define SAFE_DELETE(ptr) delete (ptr); (ptr) = NULL;


/**
 * Delete array designated by 'ptr' and set 'ptr' NULL.
 */
#define ARY_SAFE_DELETE(ptr) delete[] (ptr); (ptr) = NULL;


/**
 * Free memory designated by 'ptr', if 'ptr' is not NULL, and set 'ptr' NULL.
 */
#define SAFE_FREE(ptr) if ((ptr) != NULL) { ::free(ptr); (ptr) = NULL; }


#endif /* VISLIB_MEMUTILS_H_INCLUDED */
