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

#if !defined(_INCLUDED_LALRGEN_DEFINITIONS_H)
#define	  _INCLUDED_LALRGEN_DEFINITIONS_H

typedef struct production    production;
typedef struct configuration configuration;
typedef struct target	     target;
typedef struct item	     itementry;
typedef struct gotoentry     gotoentry;
typedef struct firstset	     firstset;
typedef struct lane	     laneentry;
typedef struct trace	     traceentry;
typedef struct collision     collision;
typedef struct statemap	     statemap;


#include <stdbool.h>

#include "dynarray_definitions.h"
#include "intset_definitions.h"
#include "symbols_definitions.h"


/* Maximum width of parsing table representation */

#define PARSE_TABLE_WIDTH		128

#define INITIAL_PRODUCTION_SIZE		8
#define INITIAL_RHS_LENGTH		4
#define INITIAL_COLLECTION_SIZE		16
#define INITIAL_ITEMSET_SIZE		2
#define INITIAL_GOTO_SIZE		4
#define INITIAL_PARENT_SIZE		2
#define INITIAL_ANCESTOR_SIZE		2
#define INITIAL_FOLLOW_SIZE		4
#define INITIAL_UPDATE_SIZE		4
#define INITIAL_CONFLICT_SIZE		2
#define INITIAL_LANE_SIZE		4
#define INITIAL_MAP_SIZE		4
#define INITIAL_REFERENCE_SIZE		4

/* Parser state errors */

#define NO_ERROR			0x0000
#define SHIFT_REDUCE_ERROR		0x0001
#define REDUCE_REDUCE_ERROR		0x0002

#define PRODUCTION(i)		(DYNARRAY(production, tables->productions, (i)))
#define PRODELEMENT		(DYNELEMENT(tables->productions))
#define PRODCOUNT		(DYNCOUNT(tables->productions))
#define PRODSIZE		(DYNSIZE(tables->productions))
#define RHSIDE(i, j)		(DYNARRAY(symbolentry *, PRODUCTION(i).rhside, (j)))
#define RHSELEMENT(i)		(DYNELEMENT(PRODUCTION(i).rhside))
#define RHSCOUNT(i)		(DYNCOUNT(PRODUCTION(i).rhside))
#define RHSSIZE(i)		(DYNSIZE(PRODUCTION(i).rhside))
#define COLLECTION(i)		(DYNARRAY(configuration, tables->collection, (i)))
#define COLLELEMENT		(DYNELEMENT(tables->collection))
#define COLLCOUNT		(DYNCOUNT(tables->collection))
#define COLLSIZE		(DYNSIZE(tables->collection))
#define ITEMSET(i, j)		(DYNARRAY(itementry, COLLECTION(i).itemset, (j)))
#define ITEMELEMENT(i)		(DYNELEMENT(COLLECTION(i).itemset))
#define ITEMCOUNT(i)		(DYNCOUNT(COLLECTION(i).itemset))
#define ITEMSIZE(i)		(DYNSIZE(COLLECTION(i).itemset))
#define GOTO(i, j)		(DYNARRAY(gotoentry, COLLECTION(i).gotos, (j)))
#define GOTOELEMENT(i)		(DYNELEMENT(COLLECTION(i).gotos))
#define GOTOCOUNT(i)		(DYNCOUNT(COLLECTION(i).gotos))
#define GOTOSIZE(i)		(DYNSIZE(COLLECTION(i).gotos))
#define ANCESTOR(i, j, k)	(DYNARRAY(target, ITEMSET(i, j).ancestors, (k)))
#define ANCELEMENT(i, j)	(DYNELEMENT(ITEMSET(i, j).ancestors))
#define ANCCOUNT(i, j)		(DYNCOUNT(ITEMSET(i, j).ancestors))
#define ANCSIZE(i, j)		(DYNSIZE(ITEMSET(i, j).ancestors))
#define UPDATE(i, j, k)		(DYNARRAY(target, ITEMSET(i, j).update, (k)))
#define UPDELEMENT(i, j)	(DYNELEMENT(ITEMSET(i, j).update))
#define UPDCOUNT(i, j)		(DYNCOUNT(ITEMSET(i, j).update))
#define UPDSIZE(i, j)		(DYNSIZE(ITEMSET(i, j).update))


struct production
{
   symbolentry *lhside;		/* Left hand side symbol */
   dynarray	rhside;		/* List of right hand side symbols */
   int		length;		/* Index of last non-epsilon token */
   int		semantic;	/* Semantic routine number */
   int		steps;		/* Minimum number of step to derive terminals */
   int		insert;		/* Minimum insertion cost for RHS */
};

struct configuration
{
   dynarray itemset;		/* LR Items in this configuration */
   int	    kernel;		/* Number of items in the configuration kernel */
   dynarray gotos;		/* List of gotos from state */
};

struct target
{
   int state;			/* Item state number */
   int item;			/* Index of item in state */
};

struct item
{
   int	     prod;		/* Index in PRODUCTION which this item represents */
   int	     dot;		/* Index of dot in the production right hand side */
   target    descendant;	/* Kernel item in goto state created by this item */
   symbolset follow;		/* Spontaneous follow set propagated to this item */
   symbolset lookahead;		/* Spontaneous and propagated follow set */
				/* The remaining elements are for kernel items only */
   dynarray  ancestors;		/* Items in other states which created this item */
   dynarray  update;		/* Set of items to which this kerrnel item propagates */
};

struct gotoentry
{
   int token;			/* Token on which to take goto */
   int state;			/* Target state of goto */
};

struct firstset
{
   symbolset symbols;		/* Symbols in set */
   bool	     nullable;		/* True if set is nullable */
};

struct lane
{
   int	  state;		/* State containing the listed items */
   intset items;		/* Items contributing to lane lookahead */
};

struct trace
{
   bool	     complete;		/* True if we've reached the end of the lane */
   dynarray  lane;		/* List of lanentry items making up the lane */
   symbolset follow;		/* Follow set accumulated by the lane */
};

struct collision
{
   traceentry *lanes;		/* One entry per item in conflict */
   int	       count;		/* Number of items in reduce-reduce conflict */
   bool	       success;		/* This conflict has been resolved */
};

struct statemap
{
   int old;			/* Old state number */
   int new;			/* New state number */
};
#endif /* _INCLUDED_LALRGEN_DEFINITIONS_H */
