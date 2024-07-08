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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser_definitions.h"
#include "tables_definitions.h"
#include "utility_definitions.h"

#include "dynarray_functions.h"
#include "parser_functions.h"
#include "utility_functions.h"


static void append_message(sdt_tables *, char *, ...);
static void build_continuation(sdt_tables *);
static int  decode_action(sdt_tables *, int, int, int *);
static int  decode_goto(sdt_tables *, int, int, int *);
static void enqueue_error(sdt_tables *, location *, char *);
static int  error_value(sdt_tables *);
static int  input_char(sdt_tables *, location *);
static void input_token(sdt_tables *);
static int  look_ahead(sdt_tables *, int, int, int);
static void perform_reduces(sdt_tables *, location *);
static bool read_buffer(sdt_tables *, location *);
static void record_repair(sdt_tables *, int);
static void repair_error(sdt_tables *);
static void write_line(sdt_tables *);


static void append_message
(
   sdt_tables *tables,
   char	      *fmt,
   ...
)
{
/* Try to append the string onto the end of the error message */

   va_list args;
   int	   count;

   va_start(args, fmt);
   count = vsnprintf(&CHRSTRING(CHRCOUNT), CHRSIZE - CHRCOUNT, fmt, args) + 1;
   va_end(args);
   if (count <= 0)
   {
      fprintf(stderr, "error encountered while appending string \"%s\" (glibc < 2.0.6?)\n", fmt);
      exit(1);
   }

/* Double the size of the error message until it can hold the new string */

   while (CHRSIZE - CHRCOUNT < count)
      dynresize(&tables->chrstring, CHRSIZE * 2);

   va_start(args, fmt);
   CHRCOUNT += vsnprintf(&CHRSTRING(CHRCOUNT), CHRSIZE - CHRCOUNT, fmt, args);
   va_end(args);
}


static void build_continuation
(
   sdt_tables *tables
)
{
/* Create the continuation string and its associated followset values */

   int value;			/* Error repair value */
   int action;			/* Type of parsing action */
   int entry;			/* Next state/production number */
   int i;

/* Set up local parse stack for the time of the syntax error */

   if (LCLSIZE < ERRSIZE)
      dynresize(&tables->lclstack, ERRSIZE);

   memcpy(&LCLSTACK(0), &ERRSTACK(0), (LCLCOUNT = ERRCOUNT) * ERRELEMENT);

/* Initialize the continuation string */

   INSERTION(0).known  = false;
   INSERTION(0).token  = 0;
   INSERTION(0).symbol = NULL;
   INSERTION(0).cost   = 0;
   INSCOUNT            = 1;
   for (i = 0; i <= tables->tnumber; i++)
      tables->followset[i] = -1;

/* Build continuation by parsing to acceptance using error correction tables */

   do
   {
/*    Decode value from error repair table */

      if ((value = error_value(tables)) < 0)
      {
	 entry  = -value;
	 action = REDUCE;
      }
      else
	 action = decode_action(tables, LCLSTACK(LCLCOUNT - 1), value, &entry);

/*    Consume state stack using error repair actions */

      switch (action)
      {
	 case SHIFT: case SHIFTREDUCE:
	    dyncheck(&tables->lclstack, LCLSIZE * 2);

	    LCLSTACK(LCLCOUNT++) = entry;

	    if (action == SHIFT)
	       break;

	 case REDUCE:
	    do
	    {
	       LCLCOUNT -= tables->rhslength[entry];

	       action = decode_goto(tables, LCLSTACK(LCLCOUNT - 1), tables->lhsymbol[entry], &entry);

	       dyncheck(&tables->lclstack, LCLSIZE * 2);

	       LCLSTACK(LCLCOUNT++) = entry;
	    }
	    while (action == SHIFTREDUCE);
      }
   }
   while (action != ACCEPT);
}


static int decode_action
(
   sdt_tables *tables,
   int	       state,
   int	       token,
   int	      *entry
)
{
/* Determine parsing action for this state and terminal symbol */

   int i;				/* Index into table for state/symbol */
   int next;				/* Next state or production number */

   if (tables->pcheck[i = tables->pbase[state] + token] == state)

/*    The table entry is valid.  Decode it into an action and a state or production number */

      if ((next = tables->pnext[i]) < 0)
      {
	 *entry = -next;
	 return(REDUCE);
      }
      else
	 if (next > SHIFT_OFFSET)
	 {
	    *entry = next - SHIFT_OFFSET;
	    return(SHIFT);
	 }
	 else
	 {
	    *entry = next;
	    return(SHIFTREDUCE);
	 }

/*    The table entry is invalid */

   return(ERROR);
}


static int decode_goto
(
   sdt_tables *tables,
   int	       state,
   int	       token,
   int	      *entry
)
{
/* Determine parsing action for this state and nonterminal symbol */

   int next;				/* Next state or production number */

/* Since the nonterminal token was produced by a reduce the table entry it */
/* selects must be valid.  It will be either shift/shiftreduce to process  */
/* the nonterminal or the ACCEPT action					   */

   if ((next = tables->pnext[tables->pbase[state] + token]) > SHIFT_OFFSET)
   {
      *entry = next - SHIFT_OFFSET;
      return(SHIFT);
   }
   else
      if (next > 0)
      {
	 *entry = next;
	 return(SHIFTREDUCE);
      }
      else
      {
	 *entry = 0;
	 return(ACCEPT);
      }
}


static void enqueue_error
(
   sdt_tables *tables,
   location   *point,
   char	      *message
)
{
/* Insert new error message at the correct point in the queue */

   location where;
   int	    i;

   if (!MSGCOUNT)
   {
/*    Insert the first message in the queue */

      dyncheck(&tables->msgqueue, MSGSIZE * 2);

      MSGQUEUE(MSGCOUNT  ).point   = *point;
      MSGQUEUE(MSGCOUNT  ).last    = *point;
      MSGQUEUE(MSGCOUNT++).message = (message) ? strdup(message) : NULL;

#ifdef	  PARSER_STATS
      if (MSGCOUNT > tables->messagerange)
	 tables->messagerange = MSGCOUNT;
#endif /* PARSER_STATS */
      return;
   }

/* If this is a scanner error and the most recent error in the queue */
/* is an adjacent scanner error then just append this to the last    */

   if (!message && !MSGQUEUE(MSGCOUNT - 1).message)
   {
      where = MSGQUEUE(MSGCOUNT - 1).last;
      where.offset++;
      if (where.offset >= where.buffer->count)
      {
	 where.buffer = where.buffer->next;
	 where.offset = 0;
      }

      if (point->buffer == where.buffer && point->offset == where.offset)
      {
	 MSGQUEUE(MSGCOUNT - 1).last = *point;
	 return;
      }
   }

/* Other errors are inserted in the correct position in the queue */

   dyncheck(&tables->msgqueue, MSGSIZE * 2);

   for (i = MSGCOUNT; i > 0; i--)
      if (MSGQUEUE(i - 1).point.buffer->order >  point->buffer->order ||
	  MSGQUEUE(i - 1).point.buffer->order == point->buffer->order &&
	  MSGQUEUE(i - 1).point.offset        >  point->offset)
	 MSGQUEUE(i) = MSGQUEUE(i - 1);
      else
	 break;

   MSGQUEUE(i).point   = *point;
   MSGQUEUE(i).last    = *point;
   MSGQUEUE(i).message = (message) ? strdup(message) : NULL;
   MSGCOUNT++;

#ifdef	  PARSER_STATS
   if (MSGCOUNT > tables->messagerange)
      tables->messagerange = MSGCOUNT;
#endif /* PARSER_STATS */
}


static int error_value
(
   sdt_tables *tables
)
{
/* Get the next token from the parser error tables (if any) */

   int value;				/* Error repair table value */
   int action;				/* Type of parsing action */
   int entry;				/* State or production of action */
   int i;

   if (!(value = tables->repair[LCLSTACK(LCLCOUNT - 1)]))
   {
/*    Record a fatal syntax error and write the current line */

      record_error(tables, &TKNQUEUE(0).where, "Syntax error");
      while (tables->unwritten.buffer->order < TKNQUEUE(0).locus.buffer->order ||
	     tables->unwritten.buffer == TKNQUEUE(0).locus.buffer && tables->unwritten.offset <= TKNQUEUE(0).locus.offset)
	 write_line(tables);
      exit(1);
   }

/* Because of reduce actions this prefix of the continuation string will */
/* potentially be examined multiple times.  There's no need to determine */
/* the followset more than once						 */

   if (!INSERTION(INSCOUNT - 1).known)
   {
      if (STASIZE < LCLSIZE)
	 dynresize(&tables->stastack, LCLSIZE);

/*    Determine what terminals are legal at the current point in the continuation */

      for (i = 1; i <= tables->tnumber; i++)
	 if (tables->followset[i] < 0)

/*	    The terminal is not legal after a shorter prefix of the continuation string */

	    if ((action = decode_action(tables, LCLSTACK(LCLCOUNT - 1), i, &entry)) == SHIFT || action == SHIFTREDUCE)

/*	       Since the current state shifts the token, it is legal (by inspection) */

	       tables->followset[i] = INSCOUNT - 1;
	    else

/*	       Otherwise it is only legal if it signals a reduce in the current state */
/*	       and is eventually shifted by parsing forward from the current position */

	       if (action == REDUCE)
	       {
		  memcpy(&STASTACK(0), &LCLSTACK(0), (STACOUNT = LCLCOUNT) * LCLELEMENT);

		  do
		  {
		     do
		     {
			STACOUNT -= tables->rhslength[entry];

			action = decode_goto(tables, STASTACK(STACOUNT - 1), tables->lhsymbol[entry], &entry);

			dyncheck(&tables->stastack, STASIZE * 2);
			STASTACK(STACOUNT++) = entry;
		     }
		     while (action == SHIFTREDUCE);

		     if (action != ACCEPT)
			action = decode_action(tables, STASTACK(STACOUNT - 1), i, &entry);
		     else
			break;
		  }
		  while (action == REDUCE);

		  if (action == SHIFT || action == SHIFTREDUCE || action == ACCEPT)
		     tables->followset[i] = INSCOUNT - 1;
	       }
      INSERTION(INSCOUNT - 1).known = true;
   }

   if (value > 0)
   {
/*    This error value is a token and becomes part of the continuation string */

      dyncheck(&tables->insertion, INSSIZE * 2);

      INSERTION(INSCOUNT  ).token  = value;
      INSERTION(INSCOUNT  ).symbol = NULL;
      INSERTION(INSCOUNT  ).cost   = INSERTION(INSCOUNT - 1).cost + tables->inscost[value];
      INSERTION(INSCOUNT++).known  = false;

#ifdef	  PARSER_STATS
      if (INSCOUNT > tables->insertrange)
	 tables->insertrange = INSCOUNT;
#endif /* PARSER_STATS */
   }
   return(value);
}


void free_parser
(
   sdt_tables *tables
)
{
   bufferentry *nextbuff;
   nameentry   *nextname;
   int		i;

/* We're done reading the file so we can close it */

   close(tables->inputfd);

/* Free any leftover input buffers */

   while (tables->bufferlist)
   {
      nextbuff = tables->bufferlist->next;
      free(tables->bufferlist);
      tables->bufferlist = nextbuff;
   }
   tables->bufferlist = NULL;
   tables->bufferend  = NULL;

/* Free the scanner token tables */

   free(tables->tokenend);
   tables->tokenend  = NULL;
   free(tables->followset);
   tables->followset = NULL;

/* Free all the working buffers */

   dynfree(&tables->chrstring);
   for (i = 0; i < MSGCOUNT; i++)
      free(MSGQUEUE(i).message);
   dynfree(&tables->msgqueue);
   for (i = 0; i < PARCOUNT; i++)
      free(PARSTACK(i).symbol);
   dynfree(&tables->parstack);
   dynfree(&tables->redqueue);
   for (i = 0; i < TKNCOUNT; i++)
      free(TKNQUEUE(i).symbol);
   dynfree(&tables->tknqueue);
   dynfree(&tables->errstack);
   dynfree(&tables->lclstack);
   dynfree(&tables->stastack);
   dynfree(&tables->chkqueue);
   for (i = 0; i < SCNCOUNT; i++)
      free(SCNSTACK(i).symbol);
   dynfree(&tables->scnstack);
   for (i = 0; i < DELCOUNT; i++)
      free(DELETION(i).symbol);
   dynfree(&tables->deletion);
   for (i = 0; i < INSCOUNT; i++)
      free(INSERTION(i).symbol);
   dynfree(&tables->insertion);

/* And free the symbol name to token number symbol table */

   for (i = 0; i < HASH_TABLE_SIZE; i++)
      while (tables->nametable[i])
      {
	 nextname = tables->nametable[i]->next;
	 free(tables->nametable[i]->name);
	 free(tables->nametable[i]);
	 tables->nametable[i] = nextname;
      }
}


void init_parser
(
   sdt_tables *tables,
   int	       fd,
   void	     (*action)(sdt_tables *, int),
   void	     (*token)(sdt_tables *, tokenentry *)
)
{
   int length;
   int i;

   tables->inputfd = fd;

/* Save perform_action and install_token callbacks */

   tables->action  = action;
   tables->token   = token;

   tables->listing = false;

/* Allocate initial input buffer */

   if (tables->bufferlist = (bufferentry *) malloc(sizeof(*tables->bufferlist)))
   {
      tables->bufferlist->next  = NULL;
      tables->bufferlist->order = 0;
      tables->bufferlist->count = 0;
      tables->bufferend         = tables->bufferlist;
   }
   else
      out_of_memory();

   tables->position.buffer = tables->bufferlist;
   tables->position.offset = 0;
   tables->newline         = true;
   tables->endfile         = false;
   tables->lineno          = 0;

/* And record the current position in the buffer */

   tables->unwritten  = tables->position;
   tables->msgwritten = false;
   tables->beginning  = tables->position;

   if (!(tables->tokenend  = (location *) malloc((tables->ntokens + 2) * sizeof(*tables->tokenend))) ||
       !(tables->followset = (int *)      malloc((tables->tnumber + 1) * sizeof(*tables->followset))))
      out_of_memory();

/* Allocate and initialize reallocatable arrays */

   dynalloc(&tables->chrstring, sizeof(char), 80);
   dynalloc(&tables->msgqueue, sizeof(errorentry), INITIAL_MSGQUEUE_SIZE);
   dynalloc(&tables->parstack, sizeof(parseentry), INITIAL_PARSTACK_SIZE);
   dynalloc(&tables->redqueue, sizeof(reduceentry), INITIAL_REDQUEUE_SIZE);
   dynalloc(&tables->tknqueue, sizeof(tokenentry), INITIAL_TKNQUEUE_SIZE);
   dynalloc(&tables->errstack, sizeof(int), INITIAL_ERRSTACK_SIZE);
   dynalloc(&tables->lclstack, sizeof(int), INITIAL_LCLSTACK_SIZE);
   dynalloc(&tables->stastack, sizeof(int), INITIAL_STASTACK_SIZE);
   dynalloc(&tables->chkqueue, sizeof(int), INITIAL_CHKQUEUE_SIZE);
   dynalloc(&tables->scnstack, sizeof(tokenentry), INITIAL_SCNSTACK_SIZE);
   dynalloc(&tables->deletion, sizeof(tokenentry), INITIAL_DELETION_SIZE);
   dynalloc(&tables->insertion, sizeof(insertentry), INITIAL_INSERTION_SIZE);

/* Initialize map of symbol names to token numbers */

   for (i = 0; i < HASH_TABLE_SIZE; i++)
      tables->nametable[i] = NULL;
   for (i = 1; i <= tables->tnumber; i++)
   {
      length = tables->stringindex[i + 1] - tables->stringindex[i];

/*    Double the size of the string array until it can hold the name */

      while (CHRSIZE < length + 1)
	 dynresize(&tables->chrstring, CHRSIZE * 2);

/*    Save the token name and number */

      snprintf(&CHRSTRING(0), CHRSIZE, "%.*s", length, &tables->stringtable[tables->stringindex[i]]);
      lookup_token(tables, &CHRSTRING(0), TERMINAL, INSERT)->token = i;
   }
   for (i = tables->tnumber + 1; i <= tables->tnumber + tables->ntnumber; i++)
   {
      length = tables->stringindex[i + 1] - tables->stringindex[i];
      while (CHRSIZE < length + 1)
	 dynresize(&tables->chrstring, CHRSIZE * 2);
      snprintf(&CHRSTRING(0), CHRSIZE, "%.*s", length, &tables->stringtable[tables->stringindex[i]]);
      lookup_token(tables, &CHRSTRING(0), NONTERMINAL, INSERT)->token = i;
   }

#ifdef	  PARSER_STATS
   tables->buffercount  = 1;
   tables->bufferrange  = tables->buffercount;
   tables->messagerange = 0;
   tables->parserange   = 0;
   tables->reducerange  = 0;
   tables->tokenrange   = 0;
   tables->scanrange    = 0;
   tables->deleterange  = 0;
   tables->insertrange  = 0;
#endif /* PARSER_STATS */
}


static int input_char
(
   sdt_tables *tables,
   location   *where
)
{
/* Get next character from the current input buffer */

   int ch;		/* Current input character */

/* If there is no character at the current position  */
/* and no further characters in the file, return EOF */

   if (tables->position.offset >= tables->position.buffer->count && !read_buffer(tables, &tables->position))
   {
/*    End of file is hypothetically the start of the next line */

      *where            = tables->position;
      tables->beginning = tables->position;
      return(ENDFILE);
   }

   *where = tables->position;
   if (tables->newline)
   {
      tables->beginning = tables->position;
      tables->newline   = false;
   }

/* If the current character is a newline the next character is the start of a new line */

   if ((ch = tables->position.buffer->buffer[tables->position.offset++]) == '\n')
      tables->newline = true;
   return(ch);
}


static void input_token
(
   sdt_tables *tables
)
{
/* Get the next token from the input file */

   int	    ch;				/* Current character in token */
   int	    final;			/* Number of last final state */
   int	    state;			/* Current scanner state number */
   location where;			/* Current position in token */
   int	    i;

/* Interpret the scanner tables to determine the next token */

   dyncheck(&tables->tknqueue, TKNSIZE * 2);

   for (;;)
   {
/*    Record the start of line and the current position of the token */

      ch = input_char(tables, &where);
      TKNQUEUE(TKNCOUNT).locus = tables->beginning;
      TKNQUEUE(TKNCOUNT).where = where;

/*    Initialize the number of the last encountered final state */

      final = -1;

/*    Run through the finite-state automaton until no transition is possible */

      state = 1;
      do
      {
/*	 Record the end of token position for all tokens ending in this state */

	 for (i = tables->tokenindex[state]; i < tables->tokenindex[state + 1]; i++)
	    tables->tokenend[tables->tokentable[i]] = where;

/*	 Remember the last final state encountered */

	 if (tables->final[state])
	    final = state;

/*	 Search through the scanner default state chain until a valid transition is found */

	 while (tables->scheck[i = tables->sbase[state] + ch] != state && (state = tables->sdefault[state]))
	    ;

/*	 If a new state must be checked get the next input character */

	 if (state && (state = tables->snext[i]))
	    ch = input_char(tables, &where);
      }
      while (state);

      if (final < 0)
      {
/*	 Since we have encountered no final state, record a lexical error, */
/*	 skip a character in the input buffer, and look for a token again  */

	 record_error(tables, &TKNQUEUE(TKNCOUNT).where, NULL);

	 tables->position = TKNQUEUE(TKNCOUNT).where;
	 tables->position.offset++;
      }
      else
      {
/*	 Reset the position in the buffer to the end of the token encountered */

	 tables->position = tables->tokenend[tables->final[final]];

/*	 If this is not an ignored token, we're done */

	 if (tables->final[final] <= tables->tnumber)
	    break;
      }
   }

/* Put token value on token stack */

   TKNQUEUE(TKNCOUNT).token = tables->final[final];

   if (tables->install[final])
   {
/*    Since the token install flag is set, record the token string along  */
/*    with the token number on the stack, and invoke a procedure to check */
/*    the token (and possibly change the token number determined by the   */
/*    scanner).  It may also record the string in the symbol table, etc.  */

/*    First, determine the length of the token and allocate a buffer for it */

      i     = 0;
      where = TKNQUEUE(TKNCOUNT).where;
      if (where.buffer != tables->position.buffer)
      {
	 i += where.buffer->count - where.offset;
	 where.buffer = where.buffer->next;
	 where.offset = 0;

	 while (where.buffer != tables->position.buffer)
	 {
	    i += where.buffer->count;
	    where.buffer = where.buffer->next;
	 }
      }
      i += tables->position.offset - where.offset;

/*    Now copy the token into a contiguous buffer */

      if (TKNQUEUE(TKNCOUNT).symbol = malloc(i + 1))
      {
	 i     = 0;
	 where = TKNQUEUE(TKNCOUNT).where;
	 while (where.offset != tables->position.offset || where.buffer != tables->position.buffer)
	 {
	    if (where.offset >= where.buffer->count)
	    {
	       where.buffer = where.buffer->next;
	       where.offset = 0;
	    }

	    TKNQUEUE(TKNCOUNT).symbol[i++] = where.buffer->buffer[where.offset++];
	 }
	 TKNQUEUE(TKNCOUNT).symbol[i] = '\0';
      }
      else
	 out_of_memory();

      (*tables->token)(tables, &TKNQUEUE(TKNCOUNT));
   }
   else
      TKNQUEUE(TKNCOUNT).symbol = NULL;

   TKNCOUNT++;
}


static int look_ahead
(
   sdt_tables *tables,
   int	       token,			/* Valid token to check, else 0 */
   int	       count,			/* Number of continuation tokens */
   int	       number			/* Number of input tokens to check */
)
{
/* Parse ahead with a copy of the current parse stack and a token stack	 */
/* consisting of the token "token" or by "count" tokens from the current */
/* continuation string which are then followed by "number" tokens from	 */
/* the input stream.  Return the number of tokens remaining when the	 */
/* parser finds an error, or 0 if all the tokens are consumed without	 */
/* a syntax error							 */

   int pointer;				/* Top of state stack */

   int action;				/* Type of parsing action */
   int entry;				/* Next state/production number */
   int i;				/* Temporaries */

/* Make a local copy of the states on the parse stack */

   memcpy(&STASTACK(0), &ERRSTACK(0), (STACOUNT = ERRCOUNT) * ERRELEMENT);

/* If a special token is to be checked, put it onto the local token stack */

   if (CHKSIZE < (i = ((token > 0) ? 1 : 0) + count + number))
      dynresize(&tables->chkqueue, i);

   CHKCOUNT = 0;
   if (token > 0)
      CHKQUEUE(CHKCOUNT++) = token;

/* Otherwise copy "count" tokens from the continuation string onto the stack */

   for (i = 1; i <= count; i++)
      CHKQUEUE(CHKCOUNT++) = INSERTION(i).token;

/* If "number" input tokens are not available, read ahead to obtain them */

   while (TKNCOUNT < number)
      input_token(tables);

/* Copy the requested number of input tokens to the local token stack */

   for (i = 0; i < number; i++)
      CHKQUEUE(CHKCOUNT++) = TKNQUEUE(i).token;

/* Now parse forward until the local token stack is consumed or an error occurs */

   pointer = STACOUNT - 1;
   i       = 0;
   do
      switch (action = decode_action(tables, STASTACK(pointer), CHKQUEUE(i), &entry))
      {
	 case SHIFT: case SHIFTREDUCE:
	    if (++pointer >= STASIZE)
	       dynresize(&tables->stastack, STASIZE * 2);

	    STASTACK(pointer) = entry;
	    if (++i >= CHKCOUNT)
	       return(0);

	    if (action == SHIFT)
	       break;

	 case REDUCE:
	    do
	    {
	       action = decode_goto(tables, STASTACK(pointer -= tables->rhslength[entry]), tables->lhsymbol[entry], &entry);

	       if (++pointer >= STASIZE)
		  dynresize(&tables->stastack, STASIZE * 2);

	       STASTACK(pointer) = entry;
	    }
	    while (action == SHIFTREDUCE);
	    break;

	 case ERROR:
	    return(CHKCOUNT - i);
      }
   while (action != ACCEPT);
   return(0);
}


nameentry *lookup_token
(
   sdt_tables	 *tables,
   unsigned char *name,
   int		  type,
   int		  action
)
{
/* Look up symbols defined in the generated scanner and parser file */

   int	      hash;		/* Hashed value of name to look up */
   nameentry *chain;		/* Current entry in nametable bucket */

/* Search symbol table chain for existing symbol */

   chain = tables->nametable[hash = hash_string(name)];
   while (chain && (chain->type != type || strcmp(chain->name, name)))
      chain = chain->next;

   if (!chain && action == INSERT)
   {
/*    Allocate and initialize a new nametable entry */

      if ((chain = (nameentry *) malloc(sizeof(*chain))) && (chain->name = strdup(name)))
	 chain->type = type;
      else
	 out_of_memory();

/*    Add it to the front of the chain */

      chain->next	      = tables->nametable[hash];
      tables->nametable[hash] = chain;
   }
   return(chain);
}


void parse_input
(
   sdt_tables *tables
)
{
/* Parse input with error correction using LR(1) tables */

   int	    state;			/* Current state for simulating reduces */
   int	    pointer;			/* Parse pointer for simulating reduces */
   int	    knownptr;			/* Part of stack unaffected by delayed reduces */
   int	    action;			/* Type of parsing action */
   int	    entry;			/* Next state/production number */
   location where;			/* Position of last token on stack */
   int	    i;

   PARSTACK(PARCOUNT  ).state        = 1;
   PARSTACK(PARCOUNT  ).where.buffer = NULL;
   PARSTACK(PARCOUNT  ).where.offset = 0;
   PARSTACK(PARCOUNT  ).token        = 0;
   PARSTACK(PARCOUNT++).symbol       = NULL;

/* Current state and top of parse stack unaffected by postponed reduces */

   state    = 1;
   pointer  = 0;
   knownptr = 0;
   do
   {
/*    If there is no input token, fetch the next one */

      if (!TKNCOUNT)
	 input_token(tables);

/*    Determine the parsing action for the current state and token pair, and perform it */

      switch (action = decode_action(tables, state, TKNQUEUE(0).token, &entry))
      {
	 case SHIFT: case SHIFTREDUCE:

/*	    Since we are about to shift a terminal, it is time to perform all delayed reduces */

	    where = PARSTACK(PARCOUNT - 1).where;
	    perform_reduces(tables, &where);

/*	    Shift the terminal (or perform the shift half of a shiftreduce) */

	    dyncheck(&tables->parstack, PARSIZE * 2);

	    state    = (action == SHIFT) ? entry : 0;
	    pointer  = PARCOUNT;
	    knownptr = pointer;

	    PARSTACK(pointer).state  = state;
	    PARSTACK(pointer).where  = TKNQUEUE(0).where;
	    PARSTACK(pointer).token  = TKNQUEUE(0).token;
	    PARSTACK(pointer).symbol = TKNQUEUE(0).symbol;
	    PARCOUNT++;

#ifdef	  PARSER_STATS
	    if (PARCOUNT > tables->parserange)
	       tables->parserange = PARCOUNT;
#endif /* PARSER_STATS */

/*	    Since we are shifting a terminal, all lines up to the current are complete */

	    while (tables->unwritten.buffer->order < TKNQUEUE(0).locus.buffer->order ||
		   tables->unwritten.buffer == TKNQUEUE(0).locus.buffer && tables->unwritten.offset < TKNQUEUE(0).locus.offset)
	       write_line(tables);

	    if (--TKNCOUNT)
	       memmove(&TKNQUEUE(0), &TKNQUEUE(1), TKNCOUNT * TKNELEMENT);

	    if (action == SHIFT)
	       break;

	 case REDUCE:

/*	    To prevent a semantic routine from being called before a potential	*/
/*	    syntax error has been repaired we create a queue of reduce entries	*/
/*	    and save them until the shift of a terminal token has been selected */

	    do
	    {
	       dyncheck(&tables->redqueue, REDSIZE * 2);

	       REDQUEUE(REDCOUNT).number = entry;

/*	       Simulate popping the right hand side of this reduction, and record */
/*	       the maximum amount that a delayed reduce has popped from the stack */

	       if ((pointer -= tables->rhslength[entry]) < knownptr)
		  knownptr = pointer;

	       if (pointer > knownptr)
	       {
/*		  We are within the part of the parse stack that has been affected by  */
/*		  delayed reduces.  Search the reduce queue for the most recent reduce */
/*		  that popped the stack to the current position.  If one is found, it  */
/*		  will indicate the new state number.  If one is not found, then this  */
/*		  is a reduction of the type <LHS> --> "", and the state is unchanged. */

		  for (i = REDCOUNT - 1; i >= 0 && REDQUEUE(i).pointer > pointer; i--)
		     ;
		  if (REDQUEUE(i).pointer == pointer)
		     state = REDQUEUE(i).state;
	       }
	       else

/*		  We are in the part of the stack that is unaffected by delayed reduces  */
/*		  The new current state may therefore be determined from the parse stack */

		  state = PARSTACK(pointer).state;

/*	       Look up parsing action indicated by recognizing the left hand side symbol */

	       if ((action = decode_goto(tables, state, tables->lhsymbol[i = entry], &entry)) == SHIFT)
		  state = entry;
	       else
		  state = 0;

/*	       Save the reduce entry for the next shift of a token */

	       REDQUEUE(REDCOUNT  ).pointer = ++pointer;
	       REDQUEUE(REDCOUNT++).state   = state;

#ifdef	  PARSER_STATS
	       if (REDCOUNT > tables->reducerange)
		  tables->reducerange = REDCOUNT;
#endif /* PARSER_STATS */
	    }
	    while (action == SHIFTREDUCE);
	    break;

	 case ERROR:
	    repair_error(tables);
      }
   }
   while (action != ACCEPT);

/* Finish off any postponed reduce actions left over by the ACCEPT */

   perform_reduces(tables, &where);

/* Since there is no "next line" after the end of the file */
/* Call write_line to display all remaining queued errors  */

   while (MSGCOUNT)
      write_line(tables);

#ifdef	  PARSER_STATS
   fputs("\nNumber of entries used in scanner and parser arrays:\n", stdout);
   printf("   %d input buffers\n", tables->bufferrange);
   printf("   %d ignored characters\n", tables->ignorerange);
   printf("   %d parser stack entries\n", tables->parserange);
   printf("   %d queued reduce actions\n", tables->reducerange);
   printf("   %d input tokens\n", tables->tokenrange);
   printf("   %d lookahead tokens\n", tables->scanrange);
   printf("   %d deleted tokens\n", tables->deleterange);
   printf("   %d continuation tokens\n", tables->insertrange);
#endif /* PARSER_STATS */
}


static void perform_reduces
(
   sdt_tables *tables,
   location   *where
)
{
/* Perform all the reduce actions currently in the queue */

   int i;

   for (i = 0; i < REDCOUNT; i++)
   {
      if (tables->semantics[REDQUEUE(i).number])
	 (*tables->action)(tables, tables->semantics[REDQUEUE(i).number]);

/*    Remove the right hand side from the parse stack */

      while (PARCOUNT > REDQUEUE(i).pointer)
	 free(PARSTACK(--PARCOUNT).symbol);

/*    And push the left hand side symbol */

      dyncheck(&tables->parstack, PARSIZE * 2);

      PARSTACK(PARCOUNT  ).state  = REDQUEUE(i).state;
      PARSTACK(PARCOUNT  ).where  = *where;
      PARSTACK(PARCOUNT  ).token  = tables->lhsymbol[REDQUEUE(i).number];
      PARSTACK(PARCOUNT++).symbol = NULL;

#ifdef	  PARSER_STATS
      if (PARCOUNT > tables->parserange)
	 tables->parserange = PARCOUNT;
#endif /* PARSER_STATS */
   }
   REDCOUNT = 0;
}


static bool read_buffer
(
   sdt_tables *tables,
   location   *where
)
{
/* Read more data into buffer chain.  Returns true if another character is available */

   int count;

   if (where->buffer->next)
   {
      where->buffer = where->buffer->next;
      where->offset = 0;
   }
   else
      if (!tables->endfile)
      {
	 if (where->buffer->count >= MAXBUFFER)
	 {
	    if (!(where->buffer = (bufferentry *) malloc(sizeof(*tables->bufferlist))))
	       out_of_memory();

	    where->buffer->next  = NULL;
	    where->buffer->order = tables->bufferend->order + 1;
	    where->buffer->count = 0;

	    tables->bufferend->next = where->buffer;
	    tables->bufferend       = where->buffer;

#ifdef	  PARSER_STATS
	    if (++tables->buffercount > tables->bufferrange)
	       tables->bufferrange = tables->buffercount;
#endif /* PARSER_STATS */

	    where->offset = 0;
	 }

/*       Read data into buffer at end of array */

	 if ((count = read(tables->inputfd, &tables->bufferend->buffer[tables->bufferend->count], MAXBUFFER - tables->bufferend->count)) < 0)
	 {
	    perror("error reading input file");
	    exit(1);
	 }

	 if (count)
	    tables->bufferend->count += count;
	 else
	    tables->endfile = true;
      }
   return(where->offset < where->buffer->count);
}


void record_error
(
   sdt_tables *tables,
   location   *point,
   char	      *fmt,
   ...
)
{
/* Format syntax/semantic error and enqueue it */

   va_list args;

   if (fmt)
   {
/*    Try to format the error message into a string */

      va_start(args, fmt);
      CHRCOUNT = vsnprintf(&CHRSTRING(0), CHRSIZE, fmt, args);
      va_end(args);
      if (CHRCOUNT < 0)
      {
	 fprintf(stderr, "error encountered while recording message \"%s\" (glibc < 2.0.6?)\n", fmt);
	 exit(1);
      }

      if (CHRCOUNT >= CHRSIZE)
      {
/*	 Increase the string length to accomodate the error message */

	 dynresize(&tables->chrstring, CHRCOUNT + 1);

	 va_start(args, fmt);
	 CHRCOUNT = vsnprintf(&CHRSTRING(0), CHRSIZE, fmt, args);
	 va_end(args);
      }

      enqueue_error(tables, point, &CHRSTRING(0));
   }
   else

/*    This is an undefined character encountered by the scanner */

      enqueue_error(tables, point, NULL);
}


static void record_repair
(
   sdt_tables *tables,
   int	       insert
)
{
/* Report error repair as one or more syntax errors */

   location where;
   int	    token;
   char	   *msg;
   int	    i, j;

   i = 0;
   while (i < DELCOUNT)
   {
      where = DELETION(i).where;

/*    If there is no insertion or the deleted tokens are  */
/*    on multiple lines, report this line as deletions    */
/*    only.  If there is an insertion and all the deleted */
/*    tokens are on one line report it as a replacement   */

      for (j = i + 1; j < DELCOUNT; j++)
	 if (DELETION(j).locus.offset != DELETION(j - 1).locus.offset || DELETION(j).locus.buffer != DELETION(j - 1).locus.buffer)
	    break;

      CHRCOUNT = 0;
      append_message(tables, "%s", (j < DELCOUNT || !insert) ? "Deleted:" : "Replaced:");

      while (i < j)
	 if (!DELETION(i).symbol)
	 {
	    token = DELETION(i++).token;
	    append_message(tables, " %.*s", tables->stringindex[token + 1] - tables->stringindex[token], &tables->stringtable[tables->stringindex[token]]);
	 }
	 else
	    append_message(tables, " %s", DELETION(i++).symbol);

/*    If this message is complete, record it */

      if (i < DELCOUNT || !insert)
      {
	 msg = strdup(&CHRSTRING(0));
	 record_error(tables, &where, "%s", msg);
	 free(msg);
      }
   }

/* If tokens are being inserted display them as a replacement or insertion */

   if (insert)
   {
      if (!DELCOUNT)
      {
/*	 If there are no deleted tokens we start an insertion message */

	 where = TKNQUEUE(0).where;

	 CHRCOUNT = 0;
	 append_message(tables, "%s", "Inserted:");
      }
      else
      {
/*	 Otherwise we append the inserted tokens to the existing replacement message */

	 append_message(tables, "%s", "  with ");

/*	 If any of the tokens being inserted are the same as tokens that were */
/*	 deleted, move the deleted symbol's value to the insertion string     */

	 for (i = 1; i <= insert; i++)
	 {
/*	    Search the deleted tokens for the same token value with a symbol */

	    for (j = 0; j < DELCOUNT; j++)
	       if (DELETION(j).token == INSERTION(i).token && DELETION(j).symbol)
	       {
		  INSERTION(i).symbol = DELETION(j).symbol;
		  DELETION(j).symbol  = NULL;
		  break;
	       }
	 }
      }

/*    Append the inserted tokens */

      for (i = 1; i <= insert; i++)
	 if (!INSERTION(i).symbol)
	 {
	    token = INSERTION(i).token;
	    append_message(tables, " %.*s", tables->stringindex[token + 1] - tables->stringindex[token], &tables->stringtable[tables->stringindex[token]]);
	 }
	 else
	    append_message(tables, " %s", INSERTION(i).symbol);

/*    And record the completed error message */

      msg = strdup(&CHRSTRING(0));
      record_error(tables, &where, "%s", msg);
      free(msg);
   }
}


static void repair_error
(
   sdt_tables *tables
)
{
/* Determine the locally least-cost error repair for this syntax error */

   errorrepair choice;			/* Least cost repair (insert or prefix) */
   errorrepair insert;			/* Valid token insertion repair */
   errorrepair prefix;			/* Continuation prefix insertion repair */
   int	       delete;			/* Total cost of deleted tokens */
   int	       token;			/* Current token being checked */
   int	       cost;			/* Cost of current correction */
   int	       i;

/* Make a local copy of the states on the parse stack */

   if (ERRSIZE < PARSIZE)
      dynresize(&tables->errstack, PARSIZE);
   for (i = 0; i < PARCOUNT; i++)
      ERRSTACK(i) = PARSTACK(i).state;
   ERRCOUNT = PARCOUNT;

/* Because of shiftreduce actions, the state on the top of the parse stack */
/* may not be real.  Perform queued reduce actions until the parser is in  */
/* a real state.  Because shiftreduce actions have no corresponding state  */
/* applying them has no impact on future error repair quality.		   */

   for (i = 0; !ERRSTACK(ERRCOUNT - 1); i++)
   {
      ERRCOUNT = REDQUEUE(i).pointer;

      dyncheck(&tables->errstack, ERRSIZE * 2);

      ERRSTACK(ERRCOUNT++) = REDQUEUE(i).state;
   }

/* Build the continuation string and determine what tokens become */
/* legal after each prefix of the continuation has been inserted  */

   build_continuation(tables);

/* Use the valid tokens that have been generated to perform */
/* a locally least-cost correction of the syntax error      */

   if (STASIZE < ERRSIZE)
      dynresize(&tables->stastack, ERRSIZE);

/* Initialize the actual correction we have decided upon */

   choice.token  = -1;
   choice.prefix = -1;
   choice.cost   = MAXCOST;
   delete        = 0;

   SCNCOUNT = 0;
   DELCOUNT = 0;

/* Now search ahead trying different corrections */

   for (;;)
   {
/*    Find the cheapest terminal symbol which may be */
/*    inserted to make the next input token legal    */

      insert.token  = -1;
      insert.prefix = -1;
      insert.cost   = MAXCOST;

      for (token = 1; token <= tables->tnumber; token++)
	 if (!tables->followset[token] && token != INSERTION(1).token && !look_ahead(tables, token, 0, 1))
	 {
/*	    This token is legal in the current state, is not the first token */
/*	    of the continuation string, and makes the next input token valid */

/*	    Calculate the repair cost including a percentage of the default repair */
/*	    cost depending on the number of valid tokens preceding another error   */

	    cost = delete + tables->inscost[token];
	    if (tables->context > 1)
	       cost += (look_ahead(tables, token, 0, tables->context) * tables->defcost) / tables->context;

	    if (cost < insert.cost)
	    {
/*	       This terminal is a new cheapest special-case insertion */

	       insert.token = token;
	       insert.cost  = cost;
	    }
	 }

/*    Ensure that the next input token is available */

      if (!TKNCOUNT)
	 input_token(tables);

      token        = TKNQUEUE(0).token;
      prefix.token = -1;
      if (tables->followset[token] >= 0)
      {
/*	 Calculate the cost of inserting this continuation prefix */

	 cost = delete + INSERTION(tables->followset[token]).cost;
	 if (tables->context > 0)
	    cost += (look_ahead(tables, 0, tables->followset[token], tables->context) * tables->defcost) / tables->context;

	 prefix.prefix = tables->followset[token];
	 prefix.cost   = cost;
      }
      else
      {
	 prefix.prefix = 0;
	 prefix.cost   = MAXCOST;
      }

/*    If either of these new repairs is cheaper than the previous */
/*    least cost error repair we've found a new best repair       */

      if (insert.cost < choice.cost || prefix.cost < choice.cost)
      {
	 choice = (insert.cost <= prefix.cost) ? insert : prefix;

/*	 This is a new locally least-cost repair so delete */
/*	 the tokens skipped over to get to this point      */

	 if ((i = DELCOUNT + SCNCOUNT) > DELSIZE)
	    dynresize(&tables->deletion, i);

	 if (SCNCOUNT)
	 {
	    memcpy(&DELETION(DELCOUNT), &SCNSTACK(0), SCNCOUNT * SCNELEMENT);
	    DELCOUNT += SCNCOUNT;

#ifdef	  PARSER_STATS
	    if (DELCOUNT > tables->deleterange)
	       tables->deleterange = DELCOUNT;
#endif /* PARSER_STATS */

	    SCNCOUNT = 0;
	 }
      }

/*    If the cost of deleting up to the next token is less */
/*    than the cost of our best repair so far keep looking */

      if (delete + tables->delcost[token] < choice.cost)
      {
/*	 Scan over the current token to look for a cheaper  */
/*	 correction at a position further ahead		 */

	 dyncheck(&tables->scnstack, SCNSIZE * 2);

	 SCNSTACK(SCNCOUNT++) = TKNQUEUE(0);
	 if (--TKNCOUNT)
	    memmove(&TKNQUEUE(0), &TKNQUEUE(1), TKNCOUNT * TKNELEMENT);

#ifdef	  PARSER_STATS
	 if (SCNCOUNT > tables->scanrange)
	    tables->scanrange = SCNCOUNT;
#endif /* PARSER_STATS */

/*	 Increment the deletion cost up to this point */

	 delete += tables->delcost[token];
      }
      else
	 break;
   }

/* Put scanned (but not deleted) tokens back onto the input stream */

   if ((i = TKNCOUNT + SCNCOUNT) > TKNSIZE)
      dynresize(&tables->tknqueue, i);
   if (TKNCOUNT)
      memmove(&TKNQUEUE(SCNCOUNT), &TKNQUEUE(0), TKNCOUNT * TKNELEMENT);
   for (i = 0; i < SCNCOUNT; i++)
      TKNQUEUE(i) = SCNSTACK(i);
   TKNCOUNT += SCNCOUNT;
   SCNCOUNT  = 0;

#ifdef	  PARSER_STATS
   if (TKNCOUNT > tables->tokenrange)
      tables->tokenrange = TKNCOUNT;
#endif /* PARSER_STATS */

/* If the best repair is a valid token insertion make it look like a */
/* continuation prefix insertion to simplify displaying the repair   */

   token = TKNQUEUE(0).token;
   if (choice.token > 0)
   {
      choice.prefix            = 1;
      INSERTION(1).token       = choice.token;
      tables->followset[token] = 1;
   }

   record_repair(tables, tables->followset[token]);

/* Clean up the deleted token symbol values */

   for (i = 0; i < DELCOUNT; i++)
      free(DELETION(i).symbol);
   DELCOUNT = 0;

/* Push the inserted tokens in front of the input, giving them the  */
/* same line and column of the token they are being inserted before */

   if (tables->followset[token] > 0)
   {
      if ((i = TKNCOUNT + tables->followset[token]) > TKNSIZE)
	 dynresize(&tables->tknqueue, i);

      memmove(&TKNQUEUE(tables->followset[token]), &TKNQUEUE(0), TKNCOUNT * TKNELEMENT);
      for (i = 0; i < tables->followset[token]; i++)
      {
	 TKNQUEUE(i).locus  = TKNQUEUE(tables->followset[token]).locus;
	 TKNQUEUE(i).where  = TKNQUEUE(tables->followset[token]).where;
	 TKNQUEUE(i).token  = INSERTION(i + 1).token;
	 TKNQUEUE(i).symbol = INSERTION(i + 1).symbol;
      }
      TKNCOUNT += tables->followset[token];
   }
   INSCOUNT = 0;

#ifdef	  PARSER_STATS
   if (TKNCOUNT > tables->tokenrange)
      tables->tokenrange = TKNCOUNT;
#endif /* PARSER_STATS */
}


static void write_line
(
   sdt_tables *tables
)
{
/* Skip over or write the line beginning at tables->unwritten */

   location	nextline;	/* Start of next line or EOF */
   location	where;		/* Current position in line */
   int		ch;		/* Current input character */
   int		column;		/* Current column in line */
   bufferentry *buffer;		/* Temporary buffer pointer */
   int		i;

/* If unwritten is already at EOF, pretend the start of the */
/* next line is EOF+1 otherwise search for newline or EOF   */

   nextline = tables->unwritten;
   if (nextline.offset >= nextline.buffer->count)
      nextline.offset = nextline.buffer->count + 1;
   else
      for (;;)
      {
	 if (nextline.offset >= nextline.buffer->count && !read_buffer(tables, &nextline))
	    break;
	 if (nextline.buffer->buffer[nextline.offset++] == '\n')
	 {
	    if (nextline.offset >= nextline.buffer->count)
	       read_buffer(tables, &nextline);
	    break;
	 }
      }

   tables->lineno++;

/* If a listing was requested or this line contains an error, print it */

   if (tables->listing || MSGCOUNT &&
      (MSGQUEUE(0).point.buffer->order < nextline.buffer->order ||
       MSGQUEUE(0).point.buffer == nextline.buffer && MSGQUEUE(0).point.offset < nextline.offset))
   {
/*    If the last displayed line included at least one error message skip a line  */

      if (tables->msgwritten)
      {
	 fputc('\n', stdout);
	 tables->msgwritten = false;
      }

      where = tables->unwritten;
      if (where.offset < where.buffer->count)
      {
/*	 Display normal line preceeded by 8 character line number prefix */
/*	 The length of the prefix was picked to be exactly one tab stop. */

	 printf("%6d: ", tables->lineno);

	 while (where.buffer->order < nextline.buffer->order || where.buffer == nextline.buffer && where.offset < nextline.offset)
	 {
	    ch = where.buffer->buffer[where.offset++];
	    if (where.offset >= where.buffer->count && where.buffer->next)
	    {
	       where.buffer = where.buffer->next;
	       where.offset = 0;
	    }

	    if (ch != '\n')
	       display_char(ch, RAW_CHAR, stdout);
	    else
	       break;
	 }
      }
      else
      {

/*	 Display a line for end of file (for insertions before EOF) */

	 fputs(" <EOF>:", stdout);

/*	 Advance nextline position to flush all remaining error messages */

	 nextline.offset++;
      }
      fputc('\n', stdout);

/*    Display all errors on the line that has just been written */

      where  = tables->unwritten;
      column = 0;
      while (MSGCOUNT && (MSGQUEUE(0).point.buffer->order < nextline.buffer->order ||
	     MSGQUEUE(0).point.buffer == nextline.buffer && MSGQUEUE(0).point.offset < nextline.offset))
      {
/*	 Calculate the column of the error message */

	 while (where.buffer->order < MSGQUEUE(0).point.buffer->order ||
		where.buffer == MSGQUEUE(0).point.buffer && where.offset < MSGQUEUE(0).point.offset)
	 {
	    column += char_width(where.buffer->buffer[where.offset++], RAW_CHAR, column);
	    if (where.offset >= where.buffer->count && where.buffer->next)
	    {
	       where.buffer = where.buffer->next;
	       where.offset = 0;
	    }
	 }

/*	 Write a caret pointing at the error location */

	 fputc('\t', stdout);
	 for (i = column; i >= 8; i -= 8)
	    fputc('\t', stdout);
	 printf("%*c\n", i + 1, '^');

/*	 And write the error message */

	 if (!MSGQUEUE(0).message)
	 {
	    fputs(" *****\tDeleted: ", stdout);
	    for (;;)
	    {
	       display_char(where.buffer->buffer[where.offset], RAW_CHAR, stdout);
	       column += char_width(where.buffer->buffer[where.offset++], RAW_CHAR, column);
	       if (where.offset >= where.buffer->count && where.buffer->next)
	       {
		  where.buffer = where.buffer->next;
		  where.offset = 0;
	       }

	       if (where.offset > MSGQUEUE(0).last.offset || where.buffer->order > MSGQUEUE(0).last.buffer->order)
		  break;
	    }
	    fputc('\n', stdout);
	 }
	 else
	 {
	    printf(" *****\t%s\n", MSGQUEUE(0).message);
	    free(MSGQUEUE(0).message);
	 }
	 tables->msgwritten = true;

/*	 And remove the error from the queue */

	 if (--MSGCOUNT)
	    memmove(&MSGQUEUE(0), &MSGQUEUE(1), MSGCOUNT * MSGELEMENT);
      }
   }

/* Move unwritten ahead one line */

   tables->unwritten = nextline;

/* Any input buffers that precede the first unwritten line are no longer needed */

   while (tables->bufferlist != tables->unwritten.buffer)
   {
      buffer             = tables->bufferlist;
      tables->bufferlist = tables->bufferlist->next;

      free(buffer);

#ifdef	  PARSER_STATS
      tables->buffercount--;
#endif /* PARSER_STATS */
   }
}
