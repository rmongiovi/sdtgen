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

#if !defined(_INCLUDED_TABLES_DEFINITIONS_H)
#define	  _INCLUDED_TABLES_DEFINITIONS_H

typedef struct sdt_tables sdt_tables;


#include <stdbool.h>

#include "dynarray_definitions.h"
#include "parser_definitions.h"
#include "utility_definitions.h"


struct sdt_tables
{
/* Tables for sdtgen scanner and parser */

   int		  ntokens;		/* Number of defined tokens (including ignored) */
   int		  tnumber;		/* Number of terminal symbols in language */
   int		  ntnumber;		/* Number of nonterminal symbols in language */
   int		  context;		/* Number of error repair context tokens */
   int		  defcost;		/* Assumed default cost of an error repair */
   int		 *tokenindex;		/* Index of end of token values in tokentable */
   int		 *tokentable;		/* Concatenated end of token values */
   int		 *final;		/* Final token value for each scanner state */
   char		 *install;		/* String matching token is recorded on parse stack */
   int		 *sdefault;		/* Default state for each compressed scanner state */
   int		 *sbase;		/* Index of transitions for each compressed scanner state */
   int		 *scheck;		/* State for which this transition is valid */
   int		 *snext;		/* Next state index for this transition */
   int		 *inscost;		/* Insertion cost for each terminal */
   int		 *delcost;		/* Deletion cost for each terminal */
   int		 *lhsymbol;		/* Nonterminal value for each production LHS */
   int		 *rhslength;		/* Number of terminals/nonterminals on each production RHS */
   int		 *semantics;		/* Semantic routine number for each production */
   int		 *repair;		/* Error repair token for each parser state */
   int		 *stringindex;		/* Index into stringtable for each token in language */
   char		 *stringtable;		/* Concatenated string of all token names */
   int		 *pbase;		/* Index of actions for each compressed parser state */
   int		 *pcheck;		/* Token for which this parsing action is valid */
   int		 *pnext;		/* Parsing action for current state and input token */

/* Data for sdtgen scanner and parser */

   int		  inputfd;		/* Input file descriptor */
   void		  (*action)(sdt_tables *, int);
   void		  (*token)(sdt_tables *, tokenentry *);
   bool		  listing;		/* True if input listing to be generated */
   bufferentry	 *bufferlist;		/* Linked list of input buffers */
   bufferentry	 *bufferend;		/* Last buffer in linked list */
   location	  position;		/* Current input buffer position */
   bool		  newline;		/* True if the next character starts a line */
   bool		  endfile;		/* True after end of file detected */
   int		  lineno;		/* Number of last line written */
   location	  unwritten;		/* Beginning of first unwritten line */
   bool		  msgwritten;		/* True if error message has been written */
   location	  beginning;		/* Beginning of current input line */
   location	 *tokenend;		/* End of token values */
   int		 *followset;		/* Minimal continuation insertion for valid token */
   dynarray	  chrstring;		/* Character array to build strings */
   dynarray	  msgqueue;		/* Error message queue */
   dynarray	  parstack;		/* Parse stack */
   dynarray	  redqueue;		/* Delayed reduces to simulate LR */
   dynarray	  tknqueue;		/* Input token queue */
   dynarray	  errstack;		/* State stack at time of error */
   dynarray	  lclstack;		/* State stack used by repair_error */
   dynarray	  stastack;		/* State stack used by error_token and look_ahead */
   dynarray	  chkqueue;		/* Token queue for look_ahead */
   dynarray	  scnstack;		/* Deletion candidate tokens */
   dynarray	  deletion;		/* Tokens actually deleted by repair */
   dynarray	  insertion;		/* Continuation automaton token string */
   nameentry	 *nametable[HASH_TABLE_SIZE];	/* Hash table for name to token number map */
#ifdef	  PARSER_STATS
   int		  buffercount;		/* Number of buffers currently in use */
   int		  bufferrange;		/* Maximum number of input buffers used */
   int		  messagerange;		/* Maximum number of error messages */
   int		  parserange;		/* Maximum depth of parse stack */
   int		  reducerange;		/* Maximum number of delayed reduces */
   int		  tokenrange;		/* Maximum number of tokens in stack */
   int		  scanrange;		/* Maximum number of deletion candidates */
   int		  deleterange;		/* Maximum number of tokens deleted */
   int		  insertrange;		/* Maximum number of tokens in continuation */
#endif /* PARSER_STATS */

/* The structure members defined above are required for the operation of    */
/* the SDTGEN scanner and parser.  Additional members should be added below */
/* this point in order to implement whatever language has been defined by   */
/* the SDTGEN input file being used with this particular set of tables.	    */
};

#endif /* _INCLUDED_TABLES_DEFINITIONS_H */
