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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynarray_definitions.h"
#include "sdtgen_definitions.h"

#include "dynarray_functions.h"
#include "intset_functions.h"
#include "utility_functions.h"


#define INITIAL_NAME_SIZE	8


static void format_string(int, char *, FILE *, FILE *);
static void read_name(dynarray *, FILE *);
static int  read_table(int **, int, FILE *);
static void write_table(int *, int, int, char *, FILE *);


static void format_string
(
   int	 count,
   char *define,
   FILE *input,
   FILE *output
)
{
   int size;
   int length;
   int done;
   int ch;
   int width;
   int i;

/* Read and write the size of a single line */

   fscanf(input, "%d", &size);

/* Skip to the start of the concatenated string */

   while (fgetc(input) != '\n')
      ;

   fprintf(output, "static char %s[%d] =\n", define, count + 1);
   fputs("{\n", output);

/* Now copy over the string data */

   fputs("   \"", output);
   length = 4;
   for (done = i = 0; i < count; i++)
   {
      if ((ch = fgetc(input)) == '"')
	 width = 2;
      else
	 width = 1;

/*    If we've filled up an output line start a new one */

      if (length + width + 1 > MAXLINE)
      {
         fputs("\"\n   \"", output);
         length = 4;
      }

/*    Copy over a single character */

      if (ch == '"')
      {
	 fputc('\\', output);
	 length++;
      }
      fputc(ch, output);
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
      fputs("\"\n", output);
   fputs("};\n\n", output);
}


static void read_name
(
   dynarray *name,
   FILE     *fp
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
   int   size,
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


static void write_table
(
   int  *table,
   int   size,
   int	 base,
   char *define,
   FILE *fp
)
{
   int  width;
   bool full;
   int  length;
   int  i;

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

   fprintf(fp, "static %s[%d] =\n", define, size + base);
   fputs("{\n", fp);

   full = false;
   if (base == 1)
   {
/*    If the table is base 1 write a leading 0 since C is base 0 */

      fprintf(fp, "   %*d", width, 0);
      length = 3 + width;
      if (size > 0)
      {
	 fputc(',', fp);
	 length++;
	 if (length + 1 + width + 1 <= MAXLINE)
	 {
	    fputc(' ', fp);
	    length++;
	 }
	 else
	    full = true;
      }
   }
   else
   {
      fputs("   ", fp);
      length = 3;
   }
   for (i = 0; i < size; i++)
   {
      if (i < size - 1)
      {
	 if (length + width + 1 > MAXLINE || full)
	 {
	    fputs("\n   ", fp);
	    full   = false;
	    length = 3;
	 }
      }
      else
	 if (length + width > MAXLINE || full)
	 {
	    fputs("\n   ", fp);
	    full   = false;
	    length = 3;
	 }
      fprintf(fp, "%*d", width, table[i]);
      length += width;
      if (i < size - 1)
      {
	 fputc(',', fp);
	 length++;
	 if (length + 1 + width + 1 <= MAXLINE)
	 {
	    fputc(' ', fp);
	    length++;
	 }
	 else
	    full = true;
      }
   }
   if (length)
      fputc('\n', fp);
   fputs("};\n\n", fp);
}


int main
(
   int   argc,
   char *argv[]
)
{
   FILE	   *input;
   FILE	   *output;
   int	    type;		/* Table type (1 for compressed tables */
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
   if (type != 1)
   {
      fputs("input tables were not produced by packtables\n", stderr);
      exit(1);
   }
   read_name(&name, input);

   fputs("#include \"tables_definitions.h\"\n\n", output);

/* Format end of token index table */

   length = read_table(&table, snumber + 1, input);
   write_table(table, snumber + 1, 1, "int Tokenindex", output);
   free(table);

/* Format the concatenated end of token table */

   read_table(&table, length, input);
   write_table(table, length, 0, "int Tokentable", output);
   free(table);

/* Format final state table */

   read_table(&table, snumber, input);
   write_table(table, snumber, 1, "int Final", output);
   free(table);

/* Format scanner install flag table */

   read_table(&table, snumber, input);
   write_table(table, snumber, 1, "char Install", output);
   free(table);

/* Format scanner default state table */

   read_table(&table, snumber, input);
   write_table(table, snumber, 1, "int Sdefault", output);
   free(table);

/* Format scanner base index table */

   read_table(&table, snumber, input);
   write_table(table, snumber, 1, "int Sbase", output);
   free(table);

/* Format scanner check state table */

   fscanf(input, "%d", &length);
   read_table(&table, length, input);
   write_table(table, length, 0, "int Scheck", output);
   free(table);

/* Format scanner next state table */

   read_table(&table, length, input);
   write_table(table, length, 0, "int Snext", output);
   free(table);

/* Format terminal insertion costs */

   read_table(&table, tnumber, input);
   write_table(table, tnumber, 1, "int Inscost", output);
   free(table);

/* Format terminal deletion costs */

   read_table(&table, tnumber, input);
   write_table(table, tnumber, 1, "int Delcost", output);
   free(table);

/* Format left hand side token numbers */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, 1, "int Lhstoken", output);
   free(table);

/* Format right hand side lengths */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, 1, "int Rhslength", output);
   free(table);

/* Format semantic routine numbers */

   read_table(&table, gnumber, input);
   write_table(table, gnumber, 1, "int Semantics", output);
   free(table);

/* Format error repair values */

   read_table(&table, pnumber, input);
   write_table(table, pnumber, 1, "int Repair", output);
   free(table);

/* Format symbol name table index */

   length = read_table(&table, tnumber + ntnumber + 1, input);
   write_table(table, tnumber + ntnumber + 1, 1, "int Stringindex", output);
   free(table);

/* Format concatenated symbol name string */

   format_string(length, "Stringtable", input, output);

/* Format parser base index table */

   read_table(&table, pnumber, input);
   write_table(table, pnumber, 1, "int Pbase", output);
   free(table);

/* Format parser check state table */

   fscanf(input, "%d", &length);
   read_table(&table, length, input);
   write_table(table, length, 1, "int Pcheck", output);
   free(table);

/* Format parser next state table */

   read_table(&table, length, input);
   write_table(table, length, 1, "int Pnext", output);
   free(table);

/* Finally, write the variable definition for all of the above */

   fprintf(output, "sdt_tables %s =\n", &DYNARRAY(char, name, 0));
   fputs("{\n", output);
   fprintf(output, "   %d, %d, %d, %d, %d,\n", ntokens, tnumber, ntnumber, context, defcost);
   fputs("   Tokenindex, Tokentable, Final, Install,\n", output);
   fputs("   Sdefault, Sbase, Scheck, Snext,\n", output);
   fputs("   Inscost, Delcost, Lhstoken, Rhslength, Semantics,\n", output);
   fputs("   Repair, Stringindex, Stringtable,\n", output);
   fputs("   Pbase, Pcheck, Pnext\n", output);
   fputs("};\n", output);

   dynfree(&name);
   fclose(input);
   fclose(output);
}
