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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynarray_definitions.h"
#include "scangen_definitions.h"
#include "sdtgen_definitions.h"

#include "dynarray_functions.h"
#include "intset_functions.h"
#include "utility_functions.h"


#define INITIAL_NAME_SIZE	8


static void compare_scanner(int **, int, int ***);
static void complete_scanner(int **, int, int *, int *, dynarray *, dynarray *, int *);
static void compress_parser(int **, int, int, int *, dynarray *, dynarray *);
static void compress_scanner(int **, int *, int, int **, int *, int *, dynarray *, dynarray *, int *);
static void compute_average(int **, int, double **);
static void copy_string(int, FILE *, FILE *);
static void insert_scanner(int **, int, int, int *, int *, dynarray *, dynarray *, int **);
static void load_actions(FILE *, int, int, int **, int ***);
static void load_transitions(FILE *, int, int ***);
static void read_name(dynarray *, FILE *);
static int  read_table(int **, int, FILE *);
static void sort_parser(int *, int, int **);
static void sort_scanner(double *, int, int **);
static int  state_mismatch(int **, int, int);
static void write_table(int *, int, FILE *);


static void compare_scanner
(
   int	**actions,
   int	  states,
   int ***compare
)
{
   int *index;
   int	i, j;

/* Allocate and initialize a two dimensional state transition comparison array */

   if (*compare = (int **) malloc(states * (sizeof(**compare) + states * sizeof(***compare))))
   {
      memset(*compare, 0, states * (sizeof(**compare) + states * sizeof(***compare)));
      for (index = (int *) &(*compare)[states], i = 0; i < states; i++)
      {
	 (*compare)[i] = index;
	 index        += states;
      }
   }
   else
      out_of_memory();

/* Calculate the number of transition mismatches between states i and j */

   for (i = 0; i < states; i++)
      for (j = i; j < states; j++)
	 (*compare)[i][j] = (*compare)[j][i] = state_mismatch(actions, i, j);
}


static void complete_scanner
(
   int	   **actions,
   int	     states,
   int	    *tdefault,
   int	    *tbase,
   dynarray *tcheck,
   dynarray *tnext,
   int	    *chain
)
{
   int *index;
   int	save;
   int	i, j;

/* Create a state index and sort it by the number of probes */

   if (!(index = (int *) malloc(states * sizeof(*index))))
      out_of_memory();

   for (i = 0; i < states; i++)
      index[i] = i;
   for (i = 1; i < states; i++)
   {
      save = index[i];
      for (j = i - 1; j >= 0 && chain[index[j]] < chain[save]; j--)
	 index[j + 1] = index[j];
      index[j + 1] = save;
   }

/* Examine states from longest to shortest chain filling in unused table entries */

   for (i = 0; i < states; i++)
      for (j = 0; j < MAPCOUNT; j++)
	 if (DYNARRAY(int, *tcheck, tbase[index[i]] + j) == 0)
	 {
	    DYNARRAY(int, *tcheck, tbase[index[i]] + j) = index[i] + 1;
	    DYNARRAY(int, *tnext, tbase[index[i]] + j)  = actions[index[i]][j];
	 }
   free(index);
}


static void compress_parser
(
   int	   **actions,
   int	     state,
   int	     tokens,
   int	    *tbase,
   dynarray *tcheck,
   dynarray *tnext
)
{
   int size;
   int i, j;

/* Make sure there are at least MAPCOUNT entries free after the end of the tables */

   while (DYNCOUNT(*tcheck) + tokens > DYNSIZE(*tcheck))
   {
      size = DYNSIZE(*tcheck);
      dynresize(tcheck, size * 2);
      memset(&DYNARRAY(int, *tcheck, size), 0, size * DYNELEMENT(*tcheck));
      dynresize(tnext, size * 2);
      memset(&DYNARRAY(int, *tnext, size), 0, size * DYNELEMENT(*tnext));
   }

/* Find an insertion point where the non-zero entries will fit in the compressed tables */

   for (i = 0; i < DYNCOUNT(*tcheck); i++)
   {
      for (j = 0; j < tokens && (actions[state][j] == 0 || DYNARRAY(int, *tcheck, i + j) == 0); j++)
	 ;
      if (j >= tokens)
	 break;
   }
   tbase[state] = i;

/* Insert the non-zero entries into the compressed tables */

   for (j = 0; j < tokens; j++)
      if (actions[state][j])
      {
	 DYNARRAY(int, *tcheck, i + j) = state + 1;
	 DYNARRAY(int, *tnext, i + j)  = actions[state][j];
      }
   if (DYNCOUNT(*tcheck) < i + tokens)
   {
      DYNCOUNT(*tcheck) = i + tokens;
      DYNCOUNT(*tnext)  = i + tokens;
   }
}


static void compress_scanner
(
   int	   **actions,
   int	    *index,
   int	     entry,
   int	   **compare,
   int	    *tdefault,
   int	    *tbase,
   dynarray *tcheck,
   dynarray *tnext,
   int	    *chain
)
{
   int	value;
   int	min;
   bool diff[MAPCOUNT];
   int	size;
   int	i, j;

/* Find the first previously inserted state which is most similar to this one */

   for (value = MAPCOUNT + 1, min = i = 0; i < entry; i++)
      if (compare[index[entry]][index[i]] < value)
      {
	 value = compare[index[entry]][index[i]];
	 min   = i;
      }
   tdefault[index[entry]] = index[min] + 1;
   chain[index[entry]]    = chain[index[min]] + 1;

/* Find the transitions which differ between this state and its default */

   for (i = 0; i < MAPCOUNT; i++)
      diff[i] = (actions[index[entry]][i] != actions[index[min]][i]);

/* Make sure there are at least MAPCOUNT entries free after the end of the tables */

   while (DYNCOUNT(*tcheck) + MAPCOUNT > DYNSIZE(*tcheck))
   {
      size = DYNSIZE(*tcheck);
      dynresize(tcheck, size * 2);
      memset(&DYNARRAY(int, *tcheck, size), 0, size * DYNELEMENT(*tcheck));
      dynresize(tnext, size * 2);
      memset(&DYNARRAY(int, *tnext, size), 0, size * DYNELEMENT(*tnext));
   }

/* Find an insertion point where these differences will fit in the compressed tables */

   for (i = 0; i < DYNCOUNT(*tcheck); i++)
   {
      for (j = 0; j < MAPCOUNT && (!diff[j] || DYNARRAY(int, *tcheck, i + j) == 0); j++)
	 ;
      if (j >= MAPCOUNT)
	 break;
   }
   tbase[index[entry]] = i;

/* Insert the needed entries into the compressed tables */

   for (j = 0; j < MAPCOUNT; j++)
      if (diff[j])
      {
	 DYNARRAY(int, *tcheck, i + j) = index[entry] + 1;
	 DYNARRAY(int, *tnext, i + j)  = actions[index[entry]][j];
      }
   if (DYNCOUNT(*tcheck) < i + MAPCOUNT)
   {
      DYNCOUNT(*tcheck) = i + MAPCOUNT;
      DYNCOUNT(*tnext)  = i + MAPCOUNT;
   }
}


static void compute_average
(
   int	  **compare,
   int	    states,
   double **average
)
{
   double numerator;
   double denominator;
   double weight;
   int	  i, j, k;

/* Compute distance-weighted mean of compare values */

   if (!((*average) = (double *) malloc(states * sizeof(**average))))
      out_of_memory();
   for (i = 0; i < states; i++)
   {
      numerator   = 0.0;
      denominator = 0.0;
      for (j = 0; j < states; j++)
	 if (j != i)
	 {
	    for (weight = 0.0, k = 0; k < states; k++)
	       if (k != i)
		  weight += fabs((double) compare[i][j] - (double) compare[i][k]);
	    weight = (states - 1 - 1) / weight;
	    numerator   += weight * (double) compare[i][j];
	    denominator += weight;
	 }
      (*average)[i] = numerator / denominator;
   }
}


static void copy_string
(
   int	 count,
   FILE *input,
   FILE *output
)
{
   int size;
   int done;
   int length;
   int i;

/* Read and write the size of a single line */

   fscanf(input, "%d", &size);
   fprintf(output, "%d\n", MAXLINE);

/* Skip to the start of the concatenated string */

   while (fgetc(input) != '\n')
      ;

/* Now copy over the string data */

   for (length = done = i = 0; i < count; i++)
   {
/*    If we've filled up an output line start a new one */

      if (length + 1 > MAXLINE)
      {
         fputc('\n', output);
	 length = 0;
      }

/*    Copy over a single character */

      fputc(fgetc(input), output);
      length++;
      done++;

/*    If we've reached the end of an input line skip past newline */

      if (done >= size)
      {
	 while (fgetc(input) != '\n')
	    ;
	 done = 0;
      }
   }
   if (length)
      fputc('\n', output);
}


static void insert_scanner
(
   int	   **actions,
   int	     states,
   int	     index,
   int	    *tdefault,
   int	    *tbase,
   dynarray *tcheck,
   dynarray *tnext,
   int	   **chain
)
{
   int size;
   int i;

   if (*chain = (int *) malloc(states * sizeof(*chain)))
      memset(*chain, 0, states * sizeof(*chain));
   else
      out_of_memory();

/* Insert first state's transitions into the compressed tables */

   tdefault[index] = 0;
   (*chain)[index] = 1;
   tbase[index]    = DYNCOUNT(*tcheck);
   if (DYNCOUNT(*tcheck) + MAPCOUNT > DYNSIZE(*tcheck))
   {
      size = DYNSIZE(*tcheck);
      dynresize(tcheck, size * 2);
      memset(&DYNARRAY(int, *tcheck, size), 0, size * DYNELEMENT(*tcheck));
      dynresize(tnext, size * 2);
      memset(&DYNARRAY(int, *tnext, size), 0, size * DYNELEMENT(*tnext));
   }
   for (i = 0; i < MAPCOUNT; i++)
   {
      DYNARRAY(int, *tcheck, tbase[index] + i) = index + 1;
      DYNARRAY(int, *tnext, tbase[index] + i)  = actions[index][i];
   }
   DYNCOUNT(*tcheck) += MAPCOUNT;
   DYNCOUNT(*tnext)  += MAPCOUNT;
}


static void load_actions
(
   FILE	 *input,
   int	  states,
   int	  tokens,
   int	**count,
   int ***actions
)
{
   int *index;
   int	tkn;
   int	next;
   int	i, j;

/* Allocate and initialize a two dimensional transition table */

   if (*count = (int *) malloc(states * sizeof(**count)))
      memset(*count, 0, states * sizeof(**count));
   else
      out_of_memory();
   if (*actions = (int **) malloc(states * (sizeof(**actions) + tokens * sizeof(***actions))))
   {
      memset(*actions, 0, states * (sizeof(**actions) + tokens * sizeof(***actions)));
      for (index = (int *) &(*actions)[states], i = 0; i < states; i++)
      {
	 (*actions)[i] = index;
	 index        += tokens;
      }
   }
   else
      out_of_memory();

/* Read all the transitions into the table */

   for (i = 0; i < states; i++)
   {
      fscanf(input, "%d", &(*count)[i]);
      for (j = 0; j < (*count)[i]; j++)
      {
	 fscanf(input, "%d %d", &tkn, &next);
	 (*actions)[i][tkn - 1] = next;
      }
   }
}


static void load_transitions
(
   FILE	 *input,
   int	  states,
   int ***actions
)
{
   int *index;
   int	count;
   int	ch;
   int	next;
   int	i, j;

/* Allocate and initialize a two dimensional transition table */

   if (*actions = (int **) malloc(states * (sizeof(**actions) + MAPCOUNT * sizeof(***actions))))
   {
      memset(*actions, 0, states * (sizeof(**actions) + MAPCOUNT * sizeof(***actions)));
      for (index = (int *) &(*actions)[states], i = 0; i < states; i++)
      {
	 (*actions)[i] = index;
	 index        += MAPCOUNT;
      }
   }
   else
      out_of_memory();

/* Read all the transitions into the table */

   for (i = 0; i < states; i++)
   {
      fscanf(input, "%d", &count);
      for (j = 0; j < count; j++)
      {
	 fscanf(input, "%d %d", &ch, &next);
	 (*actions)[i][ch] = next;
      }
   }
}


static void read_name
(
   dynarray *name,
   FILE	    *fp
)
{
   int ch;

   dynalloc(name, sizeof(char), INITIAL_NAME_SIZE);

/* Advance to first non-blank character */

   do
      ch = fgetc(fp);
   while (ch == ' ' || ch == '\t');

/* Accumulate non-blank characters into the name */

   while (ch != ' ' && ch != '\t' && ch != '\n' && ch != EOF)
   {
      dyncheck(name, DYNSIZE(*name) * 2);
      DYNARRAY(char, *name, DYNCOUNT(*name)) = ch;
      DYNCOUNT(*name)++;
      ch = fgetc(fp);
   }
   dyncheck(name, DYNSIZE(*name) * 2);
   DYNARRAY(char, *name, DYNCOUNT(*name)) = '\0';
}


static int read_table
(
   int **table,
   int	 size,
   FILE *fp
)
{
   int i;

   if (!(*table = (int *) malloc(size * sizeof(**table))))
      out_of_memory();
   for (i = 0; i < size; i++)
      fscanf(fp, "%d", &(*table)[i]);
   return((*table)[size - 1]);
}


static void sort_parser
(
   int	*count,
   int	 states,
   int **index
)
{
   int save;
   int i, j;

/* Initialize index into actions */

   if (*index = (int *) malloc(states * sizeof(**index)))
      for (i = 0; i < states; i++)
	 (*index)[i] = i;
   else
      out_of_memory();

/* Sort the state index by the number of actions in the state */

   for (i = 1; i < states; i++)
   {
      save = (*index)[i];
      for (j = i - 1; j >= 0 && count[(*index)[j]] < count[save]; j--)
	 (*index)[j + 1] = (*index)[j];
      (*index)[j + 1] = save;
   }
}


static void sort_scanner
(
   double *average,
   int	   states,
   int	 **index
)
{
   int save;
   int i, j;

/* Initialize index into actions */

   if (*index = (int *) malloc(states * sizeof(**index)))
      for (i = 0; i < states; i++)
	 (*index)[i] = i;
   else
      out_of_memory();

/* Sort the state index by the number of transitions in the state */

   for (i = 1; i < states; i++)
   {
      save = (*index)[i];
      for (j = i - 1; j >= 0 && average[(*index)[j]] > average[save]; j--)
	 (*index)[j + 1] = (*index)[j];
      (*index)[j + 1] = save;
   }
}


static int state_mismatch
(
   int **actions,
   int	 state1,
   int	 state2
)
{
   int fail;
   int i;

   if (state1 == state2)
      fail = 0;
   else
      for (fail = i = 0; i < MAPCOUNT; i++)
	 if (actions[state1][i] != actions[state2][i])
	    fail++;
   return(fail);
}


static void write_table
(
   int	*table,
   int	 size,
   FILE *fp
)
{
   int	width;
   bool full;
   int	length;
   int	i;

   for (width = i = 0; i < size; i++)
      if (table[i] < 0)
      {
/*	 For negative numbers we multiply the positive by 10 to make up for the minus sign */

	 if (-table[i] * 10 > width)
	    width = -table[i] * 10;
      }
      else
	 if (table[i] > width)
	    width = table[i];
   width = digit_count(width);

   for (full = false, length = i = 0; i < size; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, table[i]);
      length += width;
      if (i < size - 1 && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);
}


int main
(
   int   argc,
   char *argv[]
)
{
   FILE	   *input;
   FILE	   *output;
   int	    type;		/* Table type (0 for uncompressed tables */
   int	    tnumber;		/* Number of terminals in the language */
   int	    ntokens;		/* Number of tokens including ignored */
   int	    snumber;		/* Number of states in the scanner */
   int	    ntnumber;		/* Number of nonterminals in the language */
   int	    gnumber;		/* Number of productions in the grammar */
   int	    pnumber;		/* Number of states in the parser */
   int	    context;		/* Number of error repair context tokens */
   int	    defcost;		/* Assumed cost to repair a single error */
   dynarray name;		/* Identifying name for tables */
   int	   *table;		/* Generic table of integer values */
   int	    length;		/* Table length returned by index table */
   int	  **actions;		/* Two dimensional state action array */
   int	  **compare;		/* Two dimensional comparison of actions */
   double  *average;		/* Distance-weighted mean of compare */
   int	   *index;		/* Index of sorted actions array */
   int	   *tdefault;		/* Compressed tables default state */
   int	   *tbase;		/* Compressed tables state base index */
   dynarray tcheck;		/* Compressed tables check value */
   dynarray tnext;		/* Compressed tables action */
   int	   *chain;		/* Length of default state chains */
   double   before;		/* Table size before compression */
   double   after;		/* Table size after compression */
   double   total;		/* Total of all scanner table chain lengths */
   int	    max;		/* Maximum scanner table chain length */
   int	   *count;		/* Number of actions per parser state */
   int	    i;


   if (argc > 3)
   {
      fprintf(stderr, "usage: %s [ input [ output ] ]\n", argv[0]);
      exit(1);
   }

   if (argc <= 1 || !strcmp(argv[1], "-"))
      input = stdin;
   else
      if (!(input = fopen(argv[1], "r")))
      {
	 fprintf(stderr, "%s: can't open: %s\n", argv[1], strerror(errno));
	 exit(1);
      }
   if (argc <= 2 || !strcmp(argv[2], "-"))
      output = stdout;
   else
      if (!(output = fopen(argv[2], "w")))
      {
	 fprintf(stderr, "%s: can't create: %s\n", argv[2], strerror(errno));
	 exit(1);
      }

/* Read tables header */

   fscanf(input, "%d %d %d %d %d %d %d %d %d",
      &type, &tnumber, &ntokens, &snumber, &ntnumber,
      &gnumber, &pnumber, &context, &defcost);
   if (type != 0)
   {
      fputs("input tables were not produced by sdtgen\n", stderr);
      exit(1);
   }
   read_name(&name, input);

   fprintf(output, "1 %d %d %d %d %d %d %d %d %s\n",
      tnumber, ntokens, snumber, ntnumber,
      gnumber, pnumber, context, defcost,
      &DYNARRAY(char, name, 0));
   dynfree(&name);

   fprintf(stderr, "Packing language with %d terminals (plus %d ignored tokens) and %d nonterminals\n",
      tnumber, ntokens - tnumber, ntnumber);
   fprintf(stderr, "The scanner tables have %d states occupying %d x %d = %d entries\n",
      snumber, snumber, MAPCOUNT, snumber * MAPCOUNT);

/* Copy the end of token table index values and record the length of the table */

   length = read_table(&table, snumber + 1, input);
   write_table(table, snumber + 1, output);
   free(table);

/* Copy the concatenated end of token table */

   read_table(&table, length, input);
   write_table(table, length, output);
   free(table);

/* Copy the final state table */

   read_table(&table, snumber, input);
   write_table(table, snumber, output);
   free(table);

/* Copy scanner install flags for each state */

   read_table(&table, snumber, input);
   write_table(table, snumber, output);
   free(table);

/* Compress scanner transition table */

   load_transitions(input, snumber, &actions);
   compare_scanner(actions, snumber, &compare);
   compute_average(compare, snumber, &average);
   sort_scanner(average, snumber, &index);

/* Insert states into the compressed tables starting with the most similar to */
/* other states and proceeding to those that are most different.  The first   */
/* state is inserted completely with default state 0.  For the subsequent     */
/* states find the previously inserted state which is most like the state     */
/* being inserted and use it as the default.  Fit the transitions which	      */
/* differ from the default state into the compressed tables using first fit.  */
/* Finally, starting with the states that have the longest lookup chains down */
/* to the shortest, fill in any unused table entries to prevent unnecessary   */
/* reference to the default state					      */

   if ((tdefault = (int *) malloc(snumber * sizeof(*tdefault))) && (tbase = (int *) malloc(snumber * sizeof(*tbase))))
   {
      memset(tdefault, 0, snumber * sizeof(*tdefault));
      memset(tbase, 0, snumber * sizeof(*tbase));
   }
   else
      out_of_memory();
   dynalloc(&tcheck, sizeof(int), MAPCOUNT);
   dynalloc(&tnext, sizeof(int), MAPCOUNT);
   insert_scanner(actions, snumber, index[0], tdefault, tbase, &tcheck, &tnext, &chain);
   for (i = 1; i < snumber; i++)
      compress_scanner(actions, index, i, compare, tdefault, tbase, &tcheck, &tnext, chain);
   if (DYNCOUNT(tcheck) != DYNCOUNT(tnext))
   {
      fputs("internal error\n", stderr);
      exit(1);
   }
   complete_scanner(actions, snumber, tdefault, tbase, &tcheck, &tnext, chain);

   fprintf(stderr, "The packed scanner tables occupy %d + %d + %d + %d = %d entries\n",
      snumber, snumber, DYNCOUNT(tcheck), DYNCOUNT(tnext), snumber + snumber + DYNCOUNT(tcheck) + DYNCOUNT(tnext));
   before = snumber * MAPCOUNT;
   after  = snumber + snumber + DYNCOUNT(tcheck) + DYNCOUNT(tnext);
   fprintf(stderr, "This is a reduction of %.1f%% in scanner table size\n", 100.0 * (before - after) / before);
   for (total = 0.0, max = 0, i = 0; i < snumber; i++)
   {
      total += chain[i];
      if (chain[i] > max)
	 max = chain[i];
   }
   fprintf(stderr, "Average default state chain length is %.1f, maximum %d\n", total / snumber, max);

/* Write out compressed scanner */

   write_table(tdefault, snumber, output);
   write_table(tbase, snumber, output);
   fprintf(output, "%d\n", DYNCOUNT(tcheck));
   write_table(&DYNARRAY(int, tcheck, 0), DYNCOUNT(tcheck), output);
   write_table(&DYNARRAY(int, tnext, 0), DYNCOUNT(tnext), output);

   free(actions);
   free(compare);
   free(average);
   free(index);
   free(tdefault);
   free(tbase);
   dynfree(&tcheck);
   dynfree(&tnext);
   free(chain);

   fprintf(stderr, "The parser tables have %d states occupying %d x %d = %d entries\n",
      pnumber, pnumber, tnumber + ntnumber, pnumber * (tnumber + ntnumber));

/* Copy terminal insertion costs */

   read_table(&table, tnumber, input);
   write_table(table, tnumber, output);
   free(table);

/* Copy terminal deletion costs */

   read_table(&table, tnumber, input);
   write_table(table, tnumber, output);
   free(table);

/* Copy production left hand side token numbers */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, output);
   free(table);

/* Copy production right hand side lengths */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, output);
   free(table);

/* Copy production semantic routine numbers */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, output);
   free(table);

/* Copy error repair values */

   read_table(&table, pnumber, input);
   write_table(table, pnumber, output);
   free(table);

/* Copy the symbol name table index values and record the length of the table */

   length = read_table(&table, tnumber + ntnumber + 1, input);
   write_table(table, tnumber + ntnumber + 1, output);
   free(table);

/* Copy the concatenated symbol name string */

   copy_string(length, input, output);

/* Compress parser action table */

   load_actions(input, pnumber, tnumber + ntnumber, &count, &actions);
   sort_parser(count, pnumber, &index);

   if (tbase = (int *) malloc(pnumber * sizeof(*tbase)))
      memset(tbase, 0, pnumber * sizeof(*tbase));
   else
      out_of_memory();
   dynalloc(&tcheck, sizeof(int), tnumber + ntnumber);
   memset(&DYNARRAY(int, tcheck, 0), 0, (tnumber + ntnumber) * DYNELEMENT(tcheck));
   dynalloc(&tnext, sizeof(int), tnumber + ntnumber);
   memset(&DYNARRAY(int, tnext, 0), 0, (tnumber + ntnumber) * DYNELEMENT(tnext));
   for (i = 0; i < pnumber; i++)
      compress_parser(actions, index[i], tnumber + ntnumber, tbase, &tcheck, &tnext);
   if (DYNCOUNT(tcheck) != DYNCOUNT(tnext))
   {
      fputs("internal error\n", stderr);
      exit(1);
   }

   fprintf(stderr, "The packed parser tables occupy %d + %d + %d = %d entries\n",
      pnumber, DYNCOUNT(tcheck), DYNCOUNT(tnext), pnumber + DYNCOUNT(tcheck) + DYNCOUNT(tnext));
   before = pnumber * (tnumber + ntnumber);
   after  = pnumber + DYNCOUNT(tcheck) + DYNCOUNT(tnext);
   fprintf(stderr, "This is a reduction of %.1f%% in parser table size\n", 100.0 * (before - after) / before);

/* Write out compressed parser */

   write_table(tbase, pnumber, output);
   fprintf(output, "%d\n", DYNCOUNT(tcheck));
   write_table(&DYNARRAY(int, tcheck, 0), DYNCOUNT(tcheck), output);
   write_table(&DYNARRAY(int, tnext, 0), DYNCOUNT(tnext), output);

   free(count);
   free(actions);
   free(index);
   free(tbase);
   dynfree(&tcheck);
   dynfree(&tnext);

   fclose(input);
   fclose(output);
   exit(0);
}
