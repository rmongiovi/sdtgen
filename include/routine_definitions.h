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

#if !defined(_INCLUDED_ROUTINE_DEFINITIONS_H)
#define	  _INCLUDED_ROUTINE_DEFINITIONS_H

#include "dynarray_definitions.h"


#define INITIAL_SEMANTIC_SIZE	8

/* Types of character expressions */

#define EMPTY_CHARACTER		0
#define SINGLE_CHARACTER	1
#define CHARACTER_CLASS		2
#define CHARACTER_STRING	3

/* Access definitions for dynamic array */

#define	SEMSTACK(i)	(DYNARRAY(treenode *, tables->semstack, (i)))
#define SEMELEMENT	(DYNELEMENT(tables->semstack))
#define SEMCOUNT	(DYNCOUNT(tables->semstack))
#define SEMSIZE		(DYNSIZE(tables->semstack))
#endif /* _INCLUDED_ROUTINE_DEFINITIONS_H */
