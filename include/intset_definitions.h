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

#if !defined(_INCLUDED_INTSET_DEFINITIONS_H)
#define	  _INCLUDED_INTSET_DEFINITIONS_H

typedef struct dynarray intset;


#include "dynarray_definitions.h"


#define INITIAL_INTSET_SIZE	4

#define INTSET(v, i)	(DYNARRAY(int, (v), (i)))
#define INTELEMENT(v)	(DYNELEMENT((v)))
#define INTCOUNT(v)	(DYNCOUNT((v)))
#define INTSIZE(v)	(DYNSIZE((v)))
#endif /* _INCLUDED_INTSET_DEFINITIONS_H */
