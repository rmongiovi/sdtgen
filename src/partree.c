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

#include "partree_definitions.h"
#include "symbols_definitions.h"
#include "tables_definitions.h"

#include "partree_functions.h"
#include "symbols_functions.h"
#include "utility_functions.h"


static void list_tree(sdt_tables *, treenode *, int, FILE *);


treenode *append_node
(
   treenode *list,
   treenode *entry
)
{
/* Add a new node to the end of a list */

   if (list->node.count++ == LEAF)
   {
      puts("append_node: cannot append an entry to a leaf node");
      exit(1);
   }

   if (list->node.count == BINARY)
      list->node.entry[1] = entry;
   else
      if (list->node.count == TRINARY)
	 list->node.entry[2] = entry;

   list->node.entry[3]->node.next = entry;
   list->node.entry[3]		  = entry;

   entry->node.next = NULL;
   return(list);
}


treenode *copy_tree
(
   treenode *tree
)
{
/* Create a copy of an entire tree */

   treenode	 *node;
   unsigned char *string;
   treenode	 *entry;

   switch (tree->node.count)
   {
      case LEAF:
	 switch (tree->leaf.type)
	 {
	    case EPSILON:
	    case LOOKAHEAD:
	    case ZEROBYTE:
	    case ENDOFFILE:
	       return(create_leaf(tree->leaf.type));
	       break;

	    case REFERENCE:
	       return(create_leaf(tree->leaf.type, tree->leaf.value.symbol));
	       break;

	    case CHARACTER:
	    case CLASS:
	       if (!(string = strdup(tree->leaf.value.value)))
		  out_of_memory();
	       return(create_leaf(tree->leaf.type, string));
	       break;

	    case SEMANTIC:
	       return(create_leaf(tree->leaf.type, tree->leaf.value.number));
	 }

      case UNARY:
	 return(create_unary(tree->node.type, copy_tree(tree->node.entry[0])));

      case BINARY:
	 return(create_binary(tree->node.type, copy_tree(tree->node.entry[0]), copy_tree(tree->node.entry[1])));

      case TRINARY:
	 return(create_trinary(tree->node.type, copy_tree(tree->node.entry[0]), copy_tree(tree->node.entry[1]), copy_tree(tree->node.entry[2])));

      default:
	 node = create_list(tree->node.type, copy_tree(entry = tree->node.entry[0]));
	 while (entry = entry->node.next)
	    append_node(node, copy_tree(entry));
	 return(node);
   }
}


treenode *create_binary
(
   int	     type,
   treenode *entry1,
   treenode *entry2
)
{
/* Create a new binary list with a left and right hand side */

   treenode *node;

   if (!(node = (treenode *) malloc(sizeof(*node))))
      out_of_memory();

   node->node.count    = BINARY;
   node->node.type     = type;
   node->node.next     = NULL;
   node->node.entry[0] = entry1;
   node->node.entry[1] = entry2;
   node->node.entry[2] = NULL;
   node->node.entry[3] = entry2;

   entry1->node.next = entry2;
   entry2->node.next = NULL;
   return(node);
}


treenode *create_leaf
(
   int type,
   ...
)
{
/* Create a new leaf node */

   va_list   args;
   treenode *node;

   if (!(node = (treenode *) malloc(sizeof(*node))))
      out_of_memory();
   memset(node, 0, sizeof(*node));

   va_start(args, type);

   node->leaf.count = LEAF;
   node->leaf.next  = NULL;
   switch (node->leaf.type = type)
   {
      case EPSILON:
      case LOOKAHEAD:
      case ZEROBYTE:
      case ENDOFFILE:
	 break;

      case REFERENCE:
	 node->leaf.value.symbol = va_arg(args, symbolentry *);
	 break;

      case CHARACTER:
      case CLASS:
	 node->leaf.value.value = va_arg(args, unsigned char *);
	 break;

      case SEMANTIC:
	 node->leaf.value.number = va_arg(args, int);
   }
   va_end(args);
   return(node);
}


treenode *create_trinary
(
   int	     type,
   treenode *entry1,
   treenode *entry2,
   treenode *entry3
)
{
/* Create a new trinary list with a first, second, and last node */

   treenode *node;

   if (!(node = (treenode *) malloc(sizeof(*node))))
      out_of_memory();

   node->node.count    = TRINARY;
   node->node.type     = type;
   node->node.next     = NULL;
   node->node.entry[0] = entry1;
   node->node.entry[1] = entry2;
   node->node.entry[2] = entry3;
   node->node.entry[3] = entry3;

   entry1->node.next = entry2;
   entry2->node.next = entry3;
   entry3->node.next = NULL;
   return(node);
}


treenode *create_unary
(
   int	     type,
   treenode *entry
)
{
/* Create a new unary list with only a first entry */

   treenode *node;

   if (!(node = (treenode *) malloc(sizeof(*node))))
      out_of_memory();

   node->node.count    = UNARY;
   node->node.type     = type;
   node->node.next     = NULL;
   node->node.entry[0] = entry;
   node->node.entry[1] = NULL;
   node->node.entry[2] = NULL;
   node->node.entry[3] = entry;

   entry->node.next = NULL;
   return(node);
}


void display_expression
(
   sdt_tables *tables,
   treenode   *tree,
   int	       value,
   bool	       space,
   FILE	      *fp
)
{
/* Display abstract syntax tree as regular expression */

   treenode *node;
   bool	     flag;
   int	     ch;
   int	     i;

   if (space)
      fputc(' ', fp);

   if (tree->node.count == LEAF)
      switch (tree->leaf.type)
      {
	 case EPSILON:
	    fputs("\"\"", fp);
	    break;

	 case LOOKAHEAD:
	    fputc('/', fp);
	    break;

	 case REFERENCE:
	    display_symbol(tree->leaf.value.symbol, fp);
	    break;

	 case CHARACTER:
	 case CLASS:
	    if (tree->leaf.type == CHARACTER)
	       fputc((flag = (strchr(tree->leaf.value.value, '"')) ? true : false) ? '\'' : '"', fp);
	    else
	       fputc('[', fp);
	    for (i = 0; ch = tree->leaf.value.value[i]; i++)
	       display_char(ch, (tree->leaf.type == CLASS) ? CLASS_CHAR : STRING_CHAR, fp);
	    if (tree->leaf.type == CHARACTER)
	       fputc((flag) ? '\'' : '"', fp);
	    else
	       fputc(']', fp);
	    break;

	 case SEMANTIC:
	    fprintf(fp, "@%d", tree->leaf.value.number);
	    break;

	 case ZEROBYTE:
	    fputs("NUL", fp);
	    break;

	 case ENDOFFILE:
	    fputs("EOF", fp);
      }
   else
   {
      if ((ch = tree->node.type) == '.')
	 ch = ' ';

      if ((i = precedence(ch)) < value)
	 fputc('(', fp);

      switch (ch)
      {
	 case '~':
	    fputc('~', fp);
	    display_expression(tables, tree->node.entry[0], i, false, fp);
	    break;

	 case ':':
	 case '-':
	 case '|':
	 case '_':
	 case ' ':
	    display_expression(tables, node = tree->node.entry[0], i, false, fp);
	    while (node = node->node.next)
	       if (ch == '|')
	       {
		  fputs(" |", fp);
		  display_expression(tables, node, i, true, fp);
	       }
	       else
	       {
		  fputc(ch, fp);
		  display_expression(tables, node, i, false, fp);
	       }
	    break;

	 case '*':
	 case '+':
	    display_expression(tables, tree->node.entry[0], i, false, fp);
	    fputc(tree->node.type, fp);
	    break;
      }

      if (i < value)
	 fputc(')', fp);
   }
}


void display_syntax
(
   sdt_tables *tables,
   treenode   *tree,
   char       *title,
   FILE       *fp
)
{
/* Display an abstract syntax tree */

   fprintf(fp, "%s\t%s\t%s\n", tables->name, tables->title, title);
   list_tree(tables, tree, 0, fp);
   fputc('\n', fp);
}


treenode *find_node
(
   treenode *tree,
   int	     type
)
{
/* Search a list for a matching node type */

   treenode *node;

   if (tree->node.count != LEAF)
   {
      for (node = tree->node.entry[0]; node && node->node.type != type; node = node->node.next)
	 ;
      return(node);
   }
   return(NULL);
}


void free_tree
(
   treenode *tree
)
{
/* Free every node in a tree */

   treenode *node;
   treenode *next;

   if (tree)
   {
      if (tree->node.count != LEAF)
	 for (node = tree->node.entry[0]; node; node = next)
	 {
	    next = node->node.next;
	    free_tree(node);
	 }
      else
	 if (tree->node.count == LEAF && (tree->leaf.type == CHARACTER || tree->leaf.type == CLASS))
	    free(tree->leaf.value.value);
      free(tree);
   }
}


static void list_tree
(
   sdt_tables *tables,
   treenode   *tree,
   int	       indent,
   FILE	      *fp
)
{
/* Display an abstract syntax tree in tree format */

   treenode	 *node;
   bool		  flag;
   unsigned char *string;
   int		  i;

   for (i = indent * 3; i; i--)
      fputc(' ', fp);

   if (tree->node.count == LEAF)
      switch (tree->leaf.type)
      {
	 case EPSILON:
	    fputs("EPSILON\n", fp);
	    break;

	 case LOOKAHEAD:
	    fputs("LOOKAHEAD\n", fp);
	    break;

	 case REFERENCE:
	    if (tree->leaf.value.symbol->type == TERMINAL)
	       if (tree->leaf.value.symbol->value.value.token <= tables->termcount)
	       {
		  fputs("TERMINAL ", fp);
		  display_symbol(tree->leaf.value.symbol, fp);
		  fprintf(fp, ", token %d, flags = (", tree->leaf.value.symbol->value.value.token);
		  flag = false;
		  if (tree->leaf.value.symbol->value.value.flags & INSTALL)
		  {
		     fputs("Install", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & LEFT)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Left Associative", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & RIGHT)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Right Associative", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & NONE)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Nonassociative", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & CASE)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Ignore Case", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & ALIAS)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Alias", fp);
		     flag = true;
		  }
		  if (tree->leaf.value.symbol->value.value.flags & EMPTY)
		  {
		     if (flag)
			fputc('|', fp);
		     fputs("Empty", fp);
		  }
		  fprintf(fp, "), precedence = %d, insert = %d, delete = %d\n",
		     tree->leaf.value.symbol->value.value.precedence,
		     tree->leaf.value.symbol->value.value.insert,
		     tree->leaf.value.symbol->value.value.delete);
	       }
	       else
		  fputs("IGNORED\n", fp);
	    else
	       if (tree->leaf.value.symbol->type == NONTERMINAL)
		  fprintf(fp, "NONTERMINAL <%s>, token %d\n", tree->leaf.value.symbol->symbol, tree->leaf.value.symbol->value.value.token);
	       else
		  fputs("DEFINITION\n", fp);
	    break;

	 case CHARACTER:
	 case CLASS:
	    if (tree->leaf.type == CHARACTER)
	       if (flag = strchr(tree->leaf.value.value, '"'))
		  fputc('\'', fp);
	       else
		  fputc('"', fp);
	    else
	       fputc('[', fp);
	    for (string = tree->leaf.value.value; *string; string++)
	       display_char(*string, (tree->leaf.type == CLASS) ? CLASS_CHAR : STRING_CHAR, fp);
	    if (tree->leaf.type == CHARACTER)
	       if (flag)
		  fputs("'\n", fp);
	       else
		  fputs("\"\n", fp);
	    else
	       fputs("]\n", fp);
	    break;

	 case ZEROBYTE:
	    fputs("NUL\n", fp);
	    break;

	 case ENDOFFILE:
	    fputs("EOF\n", fp);
	    break;

	 case SEMANTIC:
	    fprintf(fp, "semantic %d\n", tree->leaf.value.number);
      }
   else
   {
      fprintf(fp, "%c\n", tree->node.type);
      for (node = tree->node.entry[0]; node; node = node->node.next)
	 list_tree(tables, node, indent + 1, fp);
   }
}


int precedence
(
   int ch
)
{
/* Return the precedence of a tree operator to determine parenthesis inclusion */

   switch (ch)
   {
      case ':':
	 return(6);
	 break;

      case '~':
	 return(5);
	 break;

      case '-':
	 return(4);
	 break;

      case '*':
      case '+':
	 return(3);
	 break;

      case '_':
      case ' ':
	 return(2);
	 break;

      default:
	 return(ch == '|');
   }
}


treenode *prefix_node
(
   treenode *list,
   treenode *entry
)
{
/* Add a new node to the beginning of a list */

   if (list->node.count++ == LEAF)
   {
      puts("prefix_node: cannot prefix an entry to a leaf node");
      exit(1);
   }

   list->node.entry[2] = list->node.entry[1];
   list->node.entry[1] = list->node.entry[0];
   list->node.entry[0] = entry;

   entry->node.next = list->node.entry[1];
   return(list);
}
