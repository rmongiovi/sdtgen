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

#if !defined(_INCLUDED_PARTREE_FUNCTIONS_H)
#define	  _INCLUDED_PARTREE_FUNCTIONS_H

#include "partree_definitions.h"
#include "tables_definitions.h"


extern treenode	*append_node(treenode *, treenode *);
extern treenode *copy_tree(treenode *);
extern treenode *create_binary(int, treenode *, treenode *);
extern treenode *create_leaf(int, ...);
extern treenode *create_trinary(int, treenode *, treenode *, treenode *);
extern treenode *create_unary(int, treenode *);
extern void	 display_expression(sdt_tables *, treenode *, int, bool, FILE *);
extern void	 display_syntax(sdt_tables *, treenode *, char *, FILE *);
extern treenode *find_node(treenode *, int);
extern void	 free_tree(treenode *);
extern int	 precedence(int);
extern treenode	*prefix_node(treenode *, treenode *);

#define create_list(type, node) create_unary(type, node)
#endif /* _INCLUDED_PARTREE_FUNCTIONS_H */
