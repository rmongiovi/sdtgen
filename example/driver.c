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

#define LANGUAGE_IDENTIFIER	ptables


#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "parser_definitions.h"
#include "tables_definitions.h"

#include "parser_functions.h"


extern sdt_tables LANGUAGE_IDENTIFIER;


/* Function prototypes */

       void install_token(sdt_tables *, tokenentry *);
       void perform_action(sdt_tables *, int);
static void usage(char *);


int main
(
   int   argc,
   char *argv[]
)
{
   bool listing;
   int	c;
   int	fd;

   listing = false;
   while ((c = getopt(argc, argv, "l")) != -1)
      switch (c)
      {
	 case 'l':
	    listing = true;
	    break;

	 case '?':
	    if (isprint(optopt))
	       fprintf(stderr, "unknown option '-%c'\n", optopt);
	    else
	       fprintf(stderr, "unknown option character '\\x%x'\n", optopt);
	    usage(argv[0]);
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
      init_parser(&LANGUAGE_IDENTIFIER, fd, &perform_action, &install_token);
   }
   else
      init_parser(&LANGUAGE_IDENTIFIER, fileno(stdin), &perform_action, &install_token);
   LANGUAGE_IDENTIFIER.listing = listing;

   parse_input(&LANGUAGE_IDENTIFIER);
   free_parser(&LANGUAGE_IDENTIFIER);
   exit(0);
}


void install_token
(
   sdt_tables *tables,
   tokenentry *token
)
{
}


void perform_action
(
   sdt_tables *tables,
   int	       semno
)
{
   switch (semno)
   {
   }
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

   fprintf(stderr, "usage: %s [ -l ] [ <input file> ]\n", program);
   exit(1);
}
