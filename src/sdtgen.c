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

/* SDTGEN - Syntax Directed Translator Generator - Version 1.0		*/
/* Written by Roy J. Mongiovi						*/
/*									*/
/* Usage: sdtgen {<arguments>} <input file>				*/
/*									*/
/*	  Valid <arguments> are:					*/
/*									*/
/*		-g		list the standardized grammar		*/
/*		-l		list the input file as it is parsed	*/
/*		-q		perform input syntax check only		*/
/*		-r		list token regular expressions		*/
/*		-t		list the LR parsing tables		*/
/*		-v		list conflict resolutions		*/
/*		-w tables.dat	scanner and parser output file		*/
/*		-x		list a cross-reference of tokens	*/
/*									*/
/*		-da		list the ancestors for each state	*/
/*		-dd		list DFA before minimization		*/
/*		-de		list error repair values		*/
/*		-df		list nonterminal first sets		*/
/*		-dg		list augmented input grammar		*/
/*		-di		list canonical collection of LR items	*/
/*		-dm		list DFA after minimization		*/
/*		-dn		list nondeterministic autotomaton	*/
/*		-dp		list parser syntax tree			*/
/*		-ds		list scanner syntax tree		*/
/*									*/
/* Options which may be selected within the <input file>:		*/
/*									*/
/*	  AMBIGUOUS	Use precedence and associtivity to resolve	*/
/*			shift-reduce conflicts				*/
/*	  ERRORREPAIR	Generate automatic error repair tables		*/
/*	  SHIFTREDUCE	Generate shiftreduce parsing actions to		*/
/*			decrease the size of the parsing tables		*/
/*	  SPLITSTATES	Attempt to resolve reduce-reduce conflicts by   */
/*			splitting states to undo lookahead set merges   */


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sdtgen_definitions.h"
#include "tables_definitions.h"

#include "lalrgen_functions.h"
#include "parser_functions.h"
#include "routine_functions.h"
#include "scangen_functions.h"
#include "symbols_functions.h"


extern sdt_tables sdtgen;		/* Sdtgen scanning, parsing, and semantic tables */


static void usage(char *);
static void write_tables(sdt_tables *, char *);


int main
(
   int	 argc,
   char *argv[]
)
{
   bool	 listing;
   int	 display;
   int	 debug;
   bool	 process;
   char *output;
   int	 c;
   char *p;
   int	 fd;

   listing = false;
   display = 0;
   debug   = 0;
   process = true;
   output  = "tables.dat";
   while ((c = getopt(argc, argv, "d:ghlqrtvw:x")) != -1)
      switch (c)
      {
	 case 'd':	/* List debugging output */
	    for (p = optarg; *p; p++)
	       switch (*p)
	       {
		  case 'a':	/* Display ancestor states */
		     debug |= DEBUG_A;
		     break;

		  case 'd':	/* Display deterministic automaton before minimization */
		     debug |= DEBUG_D;
		     break;

		  case 'e':	/* Display error repair values */
		     debug |= DEBUG_E;
		     break;

		  case 'f':	/* Display nonterminal follow sets */
		     debug |= DEBUG_F;
		     break;

		  case 'g':	/* Display grammar productions */
		     debug |= DEBUG_G;
		     break;

		  case 'i':	/* Display canonical collection of LR items */
		     debug |= DEBUG_I;
		     break;

		  case 'm':	/* Display deterministic automaton after minimization */
		     debug |= DEBUG_M;
		     break;

		  case 'n':	/* Display nondeterministic automaton */
		     debug |= DEBUG_N;
		     break;

		  case 'p':	/* Display parser syntax tree */
		     debug |= DEBUG_P;
		     break;

		  case 's':	/* Display scanner syntax tree */
		     debug |= DEBUG_S;
		     break;

		  default:
		     usage(argv[0]);
	       }
	    break;

	 case 'g':	/* List the augmented grammar */
	    display |= DISPLAY_G;
	    break;

	 case 'h':	/* Display usage message */
	    usage(argv[0]);

	 case 'l':	/* List the input file as it is parsed */
	    listing = true;
	    break;

	 case 'q':	/* Select syntax check of input only */
	    process = false;
	    break;

	 case 'r':	/* List expanded regular expressions */
	    display |= DISPLAY_R;
	    break;

	 case 't':	/* List the LR parsing tables */
	    display |= DISPLAY_T;
	    break;

	 case 'v':	/* List shift-reduce and reduce-reduce resolutions */
	    display |= DISPLAY_V;
	    break;

	 case 'w':	/* Set machine readable scanner file name */
	    output = optarg;
	    break;

	 case 'x':	/* List a cross-reference of symbols */
	    display |= DISPLAY_X;
	    break;

	 case '?':	/* Process argument error */
	    if (optopt == 'd' || optopt == 's')
	       fprintf(stderr, "option '-%c' requires an argument\n", optopt);
	    else
	       if (isprint(optopt))
		  fprintf(stderr, "unknown option '-%c'\n", optopt);
	       else
		  fprintf(stderr, "unknown option character '\\x%x'\n", optopt);
	    usage(argv[0]);

	 default:
	    abort();
      }
   if (argc > optind + 1)
      usage(argv[0]);

   if (optind < argc)
   {
      if ((fd = open(argv[optind], O_RDONLY)) < 0)
      {
         fprintf(stderr, "%s: can't open: %s\n", argv[optind], strerror(errno));
	 exit(1);
      }
      init_parser(&sdtgen, fd, &perform_action, &install_token);
   }
   else
      init_parser(&sdtgen, fileno(stdin), &perform_action, &install_token);
   sdtgen.listing = listing;

/* Perform syntax directed translation of input file */

   init_routine(&sdtgen);
   sdtgen.display = display;
   sdtgen.debug   = debug;
   sdtgen.process = process;

   init_symbols(&sdtgen);

   parse_input(&sdtgen);
   free_parser(&sdtgen);

/* Generate the scanner and parser if requested */

   if (sdtgen.process)
   {
      if (sdtgen.scanner)
	 if (init_scangen(&sdtgen))
	    generate_scanner(&sdtgen);
	 else
	    sdtgen.process = false;

      if (sdtgen.parser)
      {
	 init_lalrgen(&sdtgen);
	 generate_parser(&sdtgen);
      }

/*    Write the output tables if no error occurred */

      if (sdtgen.process)
	 write_tables(&sdtgen, output);

      free_scangen(&sdtgen);
      free_lalrgen(&sdtgen);
   }
   free_routine(&sdtgen);
   free_symbols(&sdtgen);
   exit(0);
}


static void usage
(
   char *argv0
)
{
   char *program;

   if (!(program = strrchr(argv0, '/')))
      program = argv0;
   else
      program++;

   fprintf(stderr, "usage: %s { -[ghlqrtvx] | -d[adefgimnps] | -w tables.dat } [<input file>]\n", program);
   exit(1);
}


static void write_tables
(
   sdt_tables *tables,
   char	      *output
)
{
   FILE *fp;

   if (!strcmp(output, "-"))
      fp = stdout;
   else
      if (!(fp = fopen(output, "w")))
      {
	 fprintf(stderr, "can't create %s: %s\n", output, strerror(errno));
	 exit(1);
      }

/* Write the header line followed by the scanner and parser tables */

   fprintf(fp, "0 %d %d %d %d %d %d %d %d %s\n", tables->termcount, tables->tokenval.token, tables->dfacount,
      tables->nontermcount, (PRODCOUNT > 1) ? PRODCOUNT - 1 : 0, (COLLCOUNT > 1) ? COLLCOUNT - 1 : 0,
      tables->repaircontext, tables->repaircost, tables->name);
   write_scanner(tables, fp);
   write_parser(tables, fp);
   fclose(fp);
}
