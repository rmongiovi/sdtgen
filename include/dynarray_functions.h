/* This file is part of the SDTGEN Project - an LR(1) scanner and parser      */
/* generator and associated tools providing automatic locally least-cost      */
/* error repair.							      */
/* Copyright (C) 2024  Roy J. Mongiovi					      */
/*									      */
/* SDTGEN is free software: you can redistribute it and/or modify it	      */
/* under the terms of the GNU Lesser General Public License as published      */
/* by the Free Software Foundation, either version 3 of the License, or	      */
/* (at your option) any later version.					      */
/*									      */
/* This program is distributed in the hope that it will be useful, but	      */
/* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY */
/* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   */
/* for more details.							      */
/*									      */
/* You should have received a copy of the GNU General Public License along    */
/* with this program.  If not, see <https://www.gnu.org/licenses/>.	      */

#if !defined(_INCLUDED_DYNARRAY_FUNCTIONS_H)
#define	  _INCLUDED_DYNARRAY_FUNCTIONS_H

#include <stdbool.h>
#include "dynarray_definitions.h"


extern void dynalloc(dynarray *, int, int);
extern bool dyncheck(dynarray *, int);
extern void dyncopy(dynarray *, dynarray *);
extern void dynfree(dynarray *);
extern void dynresize(dynarray *, int);
#endif /* _INCLUDED_DYNARRAY_FUNCTIONS_H */
