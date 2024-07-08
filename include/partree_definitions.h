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

#if !defined(_INCLUDED_PARTREE_DEFINITIONS_H)
#define	  _INCLUDED_PARTREE_DEFINITIONS_H

typedef union parsetree treenode;


#include "symbols_definitions.h"


/* Defined counts for parse tree nodes */

#define LEAF	0
#define UNARY	1
#define BINARY	2
#define TRINARY 3

/* Defined kinds for leaf nodes */

#define EPSILON		0
#define LOOKAHEAD	1
#define REFERENCE	2
#define CHARACTER	3
#define CLASS		4
#define ZEROBYTE	5
#define ENDOFFILE	6
#define SEMANTIC	7

/* Defined kinds of internal nodes */

/* '.'	Concatenate two regular expressions or productions	 */
/* '|'	Alternate two regular expressions or production RH Sides */
/*								 */
/* '*'	0 or more occurrences of a regular expression		 */
/* '+'	1 or more occurrences of a regular expression		 */
/* '-'	Difference of two character regular expressions		 */
/* '~'	Complement of a character regular expression		 */
/* ':'	Range of character regular expressions			 */
/*								 */
/* '_'	Alternate two productions				 */
/* '>'	Concatenate LHS and RHS of production			 */


union parsetree /* One node in the parse tree */
{
   struct
   {
      int		count;		/* Node branch count: UNARY, etc. */
      int		type;		/* User defined type of tree node */

      union parsetree  *next;		/* Next node in linked list */

      union parsetree  *entry[4];	/* Pointers to child nodes */
	 /* [0] points to the first, [1] to the second, [2] to	*/
	 /* the third, and [3] to the last entry in the subtree */
	 /* For   UNARY, [1] and [2] are NULL, and [3] == [0]	*/
	 /* For  BINARY, [2] is NULL and [3] == [1]		*/
	 /* For TRINARY, [3] == [2]				*/
   } node;

   struct
   {
      int		count;		/* Node branch count: LEAF */
      int		type;		/* User defined type of leaf node */

      union parsetree  *next;		/* Next node in linked list */

      union
      {
	 symbolentry   *symbol;		/* If type == REFERENCE */
	 unsigned char *value;		/* If type == CHARACTER or CLASS */
	 int		number;		/* If type == SEMANTIC */
      } value;
   } leaf;
};
#endif /* _INCLUDED_PARTREE_DEFINITIONS_H */
