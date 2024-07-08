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

#if !defined(_INCLUDED_PARSER_FUNCTIONS_H)
#define	  _INCLUDED_PARSER_FUNCTIONS_H

#include "parser_definitions.h"
#include "tables_definitions.h"


extern void	  free_parser(sdt_tables *);
extern void	  init_parser(sdt_tables *, int, void (*)(sdt_tables *, int), void (*)(sdt_tables *, tokenentry *));
extern nameentry *lookup_token(sdt_tables *, unsigned char *, int, int);
extern void	  parse_input(sdt_tables *);
extern void	  record_error(sdt_tables *, location *, char *, ...);
#endif /* _INCLUDED_PARSER_FUNCTIONS_H */
