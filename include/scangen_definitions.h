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

#if !defined(_INCLUDED_SCANGEN_DEFINITIONS_H)
#define	  _INCLUDED_SCANGEN_DEFINITIONS_H

typedef struct position   position;
typedef struct transition transition;
typedef struct dfastate   dfastate;


#include "dynarray_definitions.h"
#include "intset_definitions.h"


#define INITIAL_DFA_SIZE	32
#define INITIAL_NFASET_SIZE	8
#define INITIAL_TOKENS_SIZE	4
#define INITIAL_GROUP_SIZE	4

#define MAPCOUNT	(256 + 1)		/* All possible bytes plus EOF */
#define MAPSIZE		(MAPCOUNT / 8 + 1)	/* Number of bytes in bitmap */

/* Bitmap set, clear, and test functions */

#define BITSET(m,b)	 ((unsigned char *) (m))[(b) >> 3] |=   1 << ((b) & 7)
#define BITCLR(m,b)	 ((unsigned char *) (m))[(b) >> 3] &= ~(1 << ((b) & 7))
#define BITTST(m,b)	(((unsigned char *) (m))[(b) >> 3] &    1 << ((b) & 7))

#define	NFAPOSITION(i)	(DYNARRAY(position, tables->nfapositions, (i)))
#define	NFAELEMENT	(DYNELEMENT(tables->nfapositions))
#define	NFACOUNT	(DYNCOUNT(tables->nfapositions))
#define	NFASIZE		(DYNSIZE(tables->nfapositions))
#define	DFASTATE(i)	(DYNARRAY(dfastate, tables->dfastates, (i)))
#define	DFAELEMENT	(DYNELEMENT(tables->dfastates))
#define	DFACOUNT	(DYNCOUNT(tables->dfastates))
#define	DFASIZE		(DYNSIZE(tables->dfastates))


struct position			/* One important state in the NFA */
{
   unsigned char bitmap[MAPSIZE]; /* Transition characters for this position */
   intset	 follow;	/* Positions that may follow this one */
   int		 token;		/* Equals N if position recognizes token N */
   int		 final;		/* Equals N if position is the end of token N */
   int		 install;	/* Matching token string should be recorded */
};

struct transition		/* One transition for a state in the DFA */
{
   int index;			/* Character which this transition represents */
   int state;			/* Next state value for this transition */
};

struct dfastate			/* One state in the DFA */
{
   int	       index;		/* Current state partition number */
   intset      states;		/* Positions of member NFA states */
   intset      tokens;		/* Tokens ending in this state */
   int	       final;		/* Equals N if state is a final state for token N */
   int	       install;		/* Matching token string should be recorded */
   transition *action;		/* Transitions out of this state */
   int	       count;		/* Transition count out of state */
};
#endif /* _INCLUDED_SCANGEN_DEFINITIONS_H */
