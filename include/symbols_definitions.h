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

#if !defined(_INCLUDED_SYMBOLS_DEFINITIONS_H)
#define	  _INCLUDED_SYMBOLS_DEFINITIONS_H

typedef struct token	tokenvalue;
typedef struct symbol	symbolentry;
typedef struct dynarray	symbolset;


#include "dynarray_definitions.h"
#include "partree_definitions.h"


#define INITIAL_SYMBOLSET_SIZE	4

/* Functions for lookup_symbol */

#define LOOKUP		0
#define INSERT		1
#define DELETE		2

/* Defined symbol types */

#define DEFINITION	0
#define TERMINAL	1
#define NONTERMINAL	2

/* Flag bits defined for tokens */

#define INSTALL 	0x0001	/* Must remain 1 as it is written to the output file */
#define LEFT		0x0002
#define RIGHT		0x0004
#define NONE		0x0008
#define ASSOCIATIVITY	(LEFT | RIGHT | NONE)
#define CASE		0x0010
#define ALIAS		0x0020
#define EMPTY		0x0040

#define SYMBOLSET(v, i)	(DYNARRAY(symbolentry *, (v), (i)))
#define SYMELEMENT(v)	(DYNELEMENT((v)))
#define SYMCOUNT(v)	(DYNCOUNT((v)))
#define SYMSIZE(v)	(DYNSIZE((v)))


struct token	/* Values for a token being created */
{
   int token;			/* Token number for parser */
   int flags;			/* Special token processing flags */
   int precedence;		/* Precedence to disambiguate grammar */
   int insert;			/* Error correction insertion cost */
   int delete;			/* Error correction deletion cost */
};

struct symbol	/* One entry in the symbol table */
{
   int		  order;	/* Unique ID for symbol ordering in sets */
   unsigned char *symbol;	/* Identifier string for entry */
   int		  type;		/* Type associated with entry */
   symbolentry	 *alias;	/* List of aliases for this symbol */

   symbolentry	 *next;		/* Next entry in hash table bucket */

   union
   {
      treenode	*tree;		/* If type == DEFINITION */
      tokenvalue value;		/* If type == TERMINAL or NONTERMINAL */
   } value;
};
#endif /* _INCLUDED_SYMBOLS_DEFINITIONS_H */
