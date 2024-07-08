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

#if !defined(_INCLUDED_SYMBOLS_FUNCTIONS_H)
#define	  _INCLUDED_SYMBOLS_FUNCTIONS_H

#include <stdbool.h>
#include <stdio.h>

#include "symbols_definitions.h"
#include "tables_definitions.h"


extern symbolentry *alloc_symbol(unsigned char *);
extern void	    display_symbol(symbolentry *, FILE *);
extern void	    display_symbolset(symbolset *, FILE *);
extern void	    display_token(sdt_tables *, int, FILE *);
extern void	    free_symbols(sdt_tables *);
extern void	    init_symbols(sdt_tables *);
extern symbolentry *lookup_symbol(sdt_tables *, unsigned char *, int, int);
extern void	    symbolset_alloc(symbolset *, int);
extern void	    symbolset_copy(symbolset *, symbolset *);
extern void	    symbolset_delete(symbolset *, symbolentry *);
extern bool	    symbolset_equal(symbolset *, symbolset *);
extern int	    symbolset_find(symbolset *, symbolentry *);
extern void	    symbolset_free(symbolset *);
extern void	    symbolset_insert(symbolset *, symbolentry *);
extern void	    symbolset_intersect(symbolset *, symbolset *, symbolset *);
extern void	    symbolset_union(symbolset *, symbolset *, symbolset *);
extern char	   *unique_name(void);
#endif /* _INCLUDED_SYMBOLS_FUNCTIONS_H */
