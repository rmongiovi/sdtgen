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

#if !defined(_INCLUDED_PARSER_DEFINITIONS_H)
#define	  _INCLUDED_PARSER_DEFINITIONS_H

typedef struct buffer	   bufferentry;
typedef struct location	   location;
typedef struct errorentry  errorentry;
typedef struct tokenentry  tokenentry;
typedef struct name	   nameentry;
typedef struct parseentry  parseentry;
typedef struct reduceentry reduceentry;
typedef struct insertentry insertentry;
typedef struct errorrepair errorrepair;


#include <stdbool.h>

#include "dynarray_definitions.h"


#undef PARSER_STATS	/* Define this to generate buffer size statistics */

#define ENDFILE			256	/* Used to represent end of file */

#define MAXBUFFER		8192	/* Amount of data read from file in one read */

/* Shift state numbers are encoded as SHIFT_OFFSET + state number.	      */
/* Shiftreduce production numbers are encoded directly as the production      */
/* number which must be less than or equal to SHIFT_OFFSET. The Accept entry  */
/* is encoded as the ACCEPT_OFFSET.  Reduce production numbers are encoded as */
/* the negative production number which must be greater than ACCEPT_OFFSET.   */
/* Error entries are 0.							      */

#define SHIFT_OFFSET		10000
#define ACCEPT_OFFSET		-10000

/* Parsing actions decoded from the table entries */

#define ERROR			0
#define SHIFT			1
#define SHIFTREDUCE		2
#define REDUCE			3
#define ACCEPT			4

#define MAXCOST		99999	/* Maximum error correction cost */

/* Initial dynamic array sizes */

#define INITIAL_MSGQUEUE_SIZE	4
#define INITIAL_PARSTACK_SIZE	8
#define INITIAL_REDQUEUE_SIZE	8
#define INITIAL_TKNQUEUE_SIZE	8
#define INITIAL_ERRSTACK_SIZE	8
#define INITIAL_LCLSTACK_SIZE	8
#define INITIAL_STASTACK_SIZE	8
#define INITIAL_CHKQUEUE_SIZE	8
#define INITIAL_SCNSTACK_SIZE	4
#define INITIAL_DELETION_SIZE	4
#define INITIAL_INSERTION_SIZE	4

/* Access definitions for dynamic arrays */

#define	CHRSTRING(i)	(DYNARRAY(char, tables->chrstring,  (i)))
#define CHRELEMENT	(DYNELEMENT(tables->chrstring))
#define	CHRCOUNT	(DYNCOUNT(tables->chrstring))
#define CHRSIZE		(DYNSIZE(tables->chrstring))
#define	MSGQUEUE(i)	(DYNARRAY(errorentry, tables->msgqueue,  (i)))
#define MSGELEMENT	(DYNELEMENT(tables->msgqueue))
#define	MSGCOUNT	(DYNCOUNT(tables->msgqueue))
#define MSGSIZE		(DYNSIZE(tables->msgqueue))
#define PARSTACK(i)	(DYNARRAY(parseentry,  tables->parstack,  (i)))
#define PARELEMENT	(DYNELEMENT(tables->parstack))
#define	PARCOUNT	(DYNCOUNT(tables->parstack))
#define PARSIZE		(DYNSIZE(tables->parstack))
#define REDQUEUE(i)	(DYNARRAY(reduceentry, tables->redqueue,  (i)))
#define REDELEMENT	(DYNELEMENT(tables->redqueue))
#define	REDCOUNT	(DYNCOUNT(tables->redqueue))
#define REDSIZE		(DYNSIZE(tables->redqueue))
#define TKNQUEUE(i)	(DYNARRAY(tokenentry,  tables->tknqueue,  (i)))
#define TKNELEMENT	(DYNELEMENT(tables->tknqueue))
#define	TKNCOUNT	(DYNCOUNT(tables->tknqueue))
#define TKNSIZE		(DYNSIZE(tables->tknqueue))
#define ERRSTACK(i)	(DYNARRAY(int,         tables->errstack,  (i)))
#define ERRELEMENT	(DYNELEMENT(tables->errstack))
#define	ERRCOUNT	(DYNCOUNT(tables->errstack))
#define ERRSIZE		(DYNSIZE(tables->errstack))
#define LCLSTACK(i)	(DYNARRAY(int,         tables->lclstack,  (i)))
#define LCLELEMENT	(DYNELEMENT(tables->lclstack))
#define	LCLCOUNT	(DYNCOUNT(tables->lclstack))
#define LCLSIZE		(DYNSIZE(tables->lclstack))
#define STASTACK(i)	(DYNARRAY(int,         tables->stastack,  (i)))
#define STAELEMENT	(DYNELEMENT(tables->stastack))
#define	STACOUNT	(DYNCOUNT(tables->stastack))
#define STASIZE		(DYNSIZE(tables->stastack))
#define CHKQUEUE(i)	(DYNARRAY(int,         tables->chkqueue,  (i)))
#define CHKELEMENT	(DYNELEMENT(tables->chkqueue))
#define	CHKCOUNT	(DYNCOUNT(tables->chkqueue))
#define CHKSIZE		(DYNSIZE(tables->chkqueue))
#define SCNSTACK(i)	(DYNARRAY(tokenentry,  tables->scnstack,  (i)))
#define SCNELEMENT	(DYNELEMENT(tables->scnstack))
#define	SCNCOUNT	(DYNCOUNT(tables->scnstack))
#define SCNSIZE		(DYNSIZE(tables->scnstack))
#define DELETION(i)	(DYNARRAY(tokenentry,  tables->deletion,  (i)))
#define DELELEMENT	(DYNELEMENT(tables->deletion))
#define	DELCOUNT	(DYNCOUNT(tables->deletion))
#define DELSIZE		(DYNSIZE(tables->deletion))
#define INSERTION(i)	(DYNARRAY(insertentry, tables->insertion, (i)))
#define INSELEMENT	(DYNELEMENT(tables->insertion))
#define	INSCOUNT	(DYNCOUNT(tables->insertion))
#define INSSIZE		(DYNSIZE(tables->insertion))


struct buffer			/* One block of data from the file */
{
   struct buffer *next;		/* Next input buffer in list */
   int		  order;	/* Input buffer sequence number */
   int		  count;	/* Amount of data in the buffer */
   unsigned char  buffer[MAXBUFFER]; /* Data read from file */
};

struct location			/* Position within an input buffer */
{
   bufferentry *buffer;		/* Buffer containing character */
   int		offset;		/* Offset of character in buffer */
};

/* Scanner/syntax/semantic error message structure For scanner */
/* errors message is null and the string of ignored characters */
/* runs from point to last.  For syntax and semantic errors    */
/* point is the location at which message should be displayed  */

struct errorentry		/* Error location and (optional) message */
{
   location point;		/* Location for error message display */
   location last;		/* Last consecutive ignored character */
   char	   *message;		/* Error message */
};

struct tokenentry		/* One entry on the token stack */
{
   int		  token;	/* Token number for parser */
   unsigned char *symbol;	/* Token string (if installed) */
   location	  locus;	/* Start of containing line */
   location	  where;	/* Token start position */
};


struct name	/* Name to token number mapping */
{
   unsigned char *name;		/* Terminal/nonterminal name without quotes */
   int		  type;		/* TERMINAL or NONTERMINAL */
   int		  token;	/* Corresponding token number */
   nameentry	 *next;		/* Next entry in hash table bucket */
};

struct parseentry		/* One entry on the parse stack */
{
   int		  state;	/* State number */
   location	  where;	/* Start of token which created this entry */
   int		  token;	/* Token number */
   unsigned char *symbol;	/* Token string (if installed) */
};

struct reduceentry		/* One entry in the reduce queue */
{
   int	number;			/* Production number */
   int	pointer;		/* Pointer after RHS popped */
   int	state;			/* State for LHS shift */
};

struct insertentry		/* One entry in the continuation string */
{
   int		  token;	/* Token number */
   unsigned char *symbol;	/* Symbol string if appropriate */
   int		  cost;		/* Cost up to this token */
   bool		  known;	/* TRUE if Followset known */
};

struct errorrepair		/* Possible error repair */
{
   int token;			/* Token insertion */
   int prefix;			/* Continuation prefix insertion */
   int cost;			/* Error repair cost */
};
#endif /* _INCLUDED_PARSER_DEFINITIONS_H */
