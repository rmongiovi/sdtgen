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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbols_definitions.h"
#include "tables_definitions.h"
#include "utility_definitions.h"

#include "dynarray_functions.h"
#include "partree_functions.h"
#include "symbols_functions.h"
#include "utility_functions.h"


static void define(sdt_tables *, unsigned char *, int, unsigned char *);


symbolentry *alloc_symbol
(
   unsigned char *name
)
{
/* Allocate a new node for the scanner and parser symbol table */

   symbolentry *node;

   static int order = 0;	/* Used to uniquely identify symbols */

   if ((node = (symbolentry *) malloc(sizeof(*node))) && (node->symbol = strdup(name)))
   {
      node->order = order++;
      return(node);
   }
   else
      out_of_memory();
}


static void define
(
   sdt_tables	 *tables,
   unsigned char *name,
   int		  type,
   unsigned char *value
)
{
/* Define a built-in regular expression definition */

   unsigned char *string;

   if (!value)
      string = NULL;
   else
      if (!(string = strdup(value)))
	 out_of_memory();

   lookup_symbol(tables, name, DEFINITION, INSERT)->value.tree = create_leaf(type, string);
}


void display_symbol
(
   symbolentry *symbol,
   FILE	       *fp
)
{
/* Display a symbolentry as a terminal or nonterminal */

   unsigned char *string;
   bool		  flag;

   if (symbol->type == NONTERMINAL)
   {
      fputc('<', fp);
      for (string = symbol->symbol; *string; string++)
	 display_char(*string, STRING_CHAR, fp);
      fputc('>', fp);
   }
   else
   {
      if (flag = strchr(symbol->symbol, '"'))
	 fputc('\'', fp);
      else
	 fputc('"', fp);
      for (string = symbol->symbol; *string; string++)
	 display_char(*string, STRING_CHAR, fp);
      if (flag)
	 fputc('\'', fp);
      else
	 fputc('"', fp);
   }
}


void display_symbolset
(
   symbolset  *set,
   FILE	      *fp
)
{
/* Display the symbols in a symbolset */

   int i;

   for (i = 0; i < SYMCOUNT(*set); i++)
   {
      display_symbol(SYMBOLSET(*set, i), fp);
      if (i < SYMCOUNT(*set) - 1)
	 fputc(' ', fp);
   }
}


void display_token
(
   sdt_tables *tables,
   int	       token,
   FILE	      *fp
)
{
/* Display a token number as a terminal or nonterminal */

   if (token <= tables->termcount)
      display_symbol(tables->termtable[token], fp);
   else
      display_symbol(tables->nontermtable[token - tables->termcount], fp);
}


void free_symbols
(
   sdt_tables *tables
)
{
/* Free all the entries in the scanner and parser symbol table */

   symbolentry *next;
   int		i;

   for (i = 0; i < HASH_TABLE_SIZE; i++)
      while (tables->symboltable[i])
      {
	 next = tables->symboltable[i]->next;
	 free(tables->symboltable[i]->symbol);
	 if (tables->symboltable[i]->type == DEFINITION)
	    free_tree(tables->symboltable[i]->value.tree);
	 free(tables->symboltable[i]);
	 tables->symboltable[i] = next;
      }
}


void init_symbols
(
   sdt_tables *tables
)
{
   int i;

/* Initialize scanner and parser symbol table */

   for (i = 0; i < HASH_TABLE_SIZE; i++)
      tables->symboltable[i] = NULL;

/* Predefine ASCII non-printables */

   define(tables, "NUL",  ZEROBYTE, NULL);
   define(tables, "SOH", CHARACTER, "\001");
   define(tables, "STX", CHARACTER, "\002");
   define(tables, "ETX", CHARACTER, "\003");
   define(tables, "EOT", CHARACTER, "\004");
   define(tables, "ENQ", CHARACTER, "\005");
   define(tables, "ACK", CHARACTER, "\006");
   define(tables, "BEL", CHARACTER, "\007");
   define(tables,  "BS", CHARACTER, "\010");
   define(tables,  "HT", CHARACTER, "\011");
   define(tables,  "LF", CHARACTER, "\012");
   define(tables,  "NL", CHARACTER, "\012");
   define(tables, "EOL", CHARACTER, "\012");
   define(tables,  "VT", CHARACTER, "\013");
   define(tables,  "FF", CHARACTER, "\014");
   define(tables,  "CR", CHARACTER, "\015");
   define(tables,  "SO", CHARACTER, "\016");
   define(tables,  "SI", CHARACTER, "\017");
   define(tables, "DLE", CHARACTER, "\020");
   define(tables, "DC1", CHARACTER, "\021");
   define(tables, "DC2", CHARACTER, "\022");
   define(tables, "DC3", CHARACTER, "\023");
   define(tables, "DC4", CHARACTER, "\024");
   define(tables, "NAK", CHARACTER, "\025");
   define(tables, "SYN", CHARACTER, "\026");
   define(tables, "ETB", CHARACTER, "\027");
   define(tables, "CAN", CHARACTER, "\030");
   define(tables,  "EM", CHARACTER, "\031");
   define(tables, "SUB", CHARACTER, "\032");
   define(tables, "ESC", CHARACTER, "\033");
   define(tables,  "FS", CHARACTER, "\034");
   define(tables,  "GS", CHARACTER, "\035");
   define(tables,  "RS", CHARACTER, "\036");
   define(tables,  "US", CHARACTER, "\037");
   define(tables, "DEL", CHARACTER, "\177");
   define(tables, "EOF", ENDOFFILE, NULL);
}


symbolentry *lookup_symbol
(
   sdt_tables	 *tables,
   unsigned char *symbol,
   int		  type,
   int		  action
)
{
/* Look up symbols defined by the current input file */

   int		hash;		/* Hashed value of symbol to look up */
   symbolentry *chain;		/* Current entry in symboltable bucket */
   symbolentry *entry;		/* Previous entry in symboltable chain */

/* Search symbol table chain for existing symbol */

   chain = tables->symboltable[hash = hash_string(symbol)];
   while (chain && (chain->type != type || strcmp(chain->symbol, symbol)))
      chain = chain->next;

   if (!chain && action == INSERT)
   {
/*    Allocate and initialize a new symboltable entry */

      chain = alloc_symbol(symbol);
      if ((chain->type = type) == DEFINITION)
	 chain->value.tree = NULL;
      else
	 memset(&chain->value, 0, sizeof(chain->value));
      chain->alias = NULL;

/*    Add it to the front of the chain */

      chain->next	        = tables->symboltable[hash];
      tables->symboltable[hash] = chain;
   }
   else
      if (chain && action == DELETE)
      {
/*	 Remove symbol table entry from hash table chain */

	 if ((entry = tables->symboltable[hash]) != chain)
	 {
	    while (entry->next != chain)
	       entry = entry->next;
	    entry->next = chain->next;
	 }
	 else
	    tables->symboltable[hash] = chain->next;

	 free(chain->symbol);
	 free(chain);
      }

   return(chain);
}


void symbolset_alloc
(
   symbolset *set,
   int	      size
)
{
/* Allocate a set of symbolentries */

   dynalloc(set, sizeof(symbolentry *), size);
}


void symbolset_copy
(
   symbolset *dst,
   symbolset *src
)
{
/* Create a copy of a set of symbolentries */

   dyncopy(dst, src);
}


void symbolset_delete
(
   symbolset   *set,
   symbolentry *symbol
)
{
/* Delete a symbolentry from a set */

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Find symbol in set */

      lower = 0;
      upper = SYMCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (symbol->order < SYMBOLSET(*set, i)->order)
	    upper = i;
	 else
	    if (symbol->order > SYMBOLSET(*set, i)->order)
	       lower = i + 1;
	    else
	       break;
      }

/*    Remove the symbol from the set if it was found */

      if (lower < upper)
      {
         while (i < SYMCOUNT(*set) - 1)
	 {
	    SYMBOLSET(*set, i) = SYMBOLSET(*set, i + 1);
	    i++;
	 }
	 SYMCOUNT(*set)--;
      }
   }
}


bool symbolset_equal
(
   symbolset *set1,
   symbolset *set2
)
{
/* Compare two symbolentry sets for equality */

   int i;

   if (SYMCOUNT(*set1) != SYMCOUNT(*set2) || SYMELEMENT(*set1) != SYMELEMENT(*set2))
      return(false);
   for (i = 0; i < SYMCOUNT(*set1); i++)
      if (SYMBOLSET(*set1, i)->order != SYMBOLSET(*set2, i)->order)
	 return(false);
   return(true);
}


int symbolset_find
(
   symbolset   *set,
   symbolentry *symbol
)
{
/* Find a symbolentry in a set */

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Look for symbol in set */

      lower = 0;
      upper = SYMCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (symbol->order < SYMBOLSET(*set, i)->order)
	    upper = i;
	 else
	    if (symbol->order > SYMBOLSET(*set, i)->order)
	       lower = i + 1;
	    else
	       return(i);
      }
   }
   return(-1);
}


void symbolset_free
(
   symbolset *set
)
{
/* Free a symbolentry set */

   dynfree(set);
}


void symbolset_insert
(
   symbolset   *set,
   symbolentry *symbol
)
{
/* Insert a new symbolentry into a set */

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Check if symbol is already in the set */

      lower = 0;
      upper = SYMCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (symbol->order < SYMBOLSET(*set, i)->order)
	    upper = i;
	 else
	    if (symbol->order > SYMBOLSET(*set, i)->order)
	       lower = i + 1;
	    else
	       return;
      }

/*    Insert the new symbol into the correct place in the set */

      dyncheck(set, SYMSIZE(*set) * 2);
      for (i = SYMCOUNT(*set); i > 0 && SYMBOLSET(*set, i - 1)->order > symbol->order; i--)
	 SYMBOLSET(*set, i) = SYMBOLSET(*set, i - 1);
      SYMBOLSET(*set, i) = symbol;
      SYMCOUNT(*set)++;
   }
   else
   {
/*    Allocate the initial entry in the set */

      symbolset_alloc(set, INITIAL_SYMBOLSET_SIZE);
      SYMBOLSET(*set, SYMCOUNT(*set)++) = symbol;
   }
}


void symbolset_intersect
(
   symbolset *dst,
   symbolset *src1,
   symbolset *src2
)
{
/* Compute the intersection of two symbolentry sets */

   int i1;
   int i2;

   symbolset_alloc(dst, INITIAL_SYMBOLSET_SIZE);
   if (src1->array && src2->array)
   {
      i1 = 0;
      i2 = 0;
      while (i1 < SYMCOUNT(*src1) && i2 < SYMCOUNT(*src2))
      {
/*	 Skip the elements in src1 which are less than the current element in src2 */

	 while (i1 < SYMCOUNT(*src1) && SYMBOLSET(*src1, i1)->order < SYMBOLSET(*src2, i2)->order)
	    i1++;

/*	 If the current elements in src1 and src2 are equal copy 1 and skip the other */

	 if (i1 < SYMCOUNT(*src1) && SYMBOLSET(*src1, i1)->order == SYMBOLSET(*src2, i2)->order)
	 {
	    dyncheck(dst, SYMSIZE(*dst) * 2);
	    SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src1, i1++);
	    i2++;
	 }

/*	 Skip the elements in src2 which are less than the current element in src1 */

	 while (i1 < SYMCOUNT(*src1) && i2 < SYMCOUNT(*src2) && SYMBOLSET(*src2, i2)->order < SYMBOLSET(*src1, i1)->order)
	    i2++;
      }
   }
}


void symbolset_union
(
   symbolset *dst,
   symbolset *src1,
   symbolset *src2
)
{
/* Compute the union of two symbolentry sets */

   int i1;
   int i2;

   if (src1->array && src2->array)
   {
      symbolset_alloc(dst, INITIAL_SYMBOLSET_SIZE);
      i1 = 0;
      i2 = 0;
      while (i1 < SYMCOUNT(*src1) && i2 < SYMCOUNT(*src2))
      {
/*	 Copy over the elements in src1 which are less than the current element in src2 */

	 while (i1 < SYMCOUNT(*src1) && SYMBOLSET(*src1, i1)->order < SYMBOLSET(*src2, i2)->order)
	 {
	    dyncheck(dst, SYMSIZE(*dst) * 2);
	    SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src1, i1++);
	 }

/*	 If the current elements in src1 and src2 are equal copy 1 and skip the other */

	 if (i1 < SYMCOUNT(*src1) && SYMBOLSET(*src1, i1)->order == SYMBOLSET(*src2, i2)->order)
	 {
	    dyncheck(dst, SYMSIZE(*dst) * 2);
	    SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src1, i1++);
	    i2++;
	 }

/*	 Copy over the elements in src2 which are less than the current element in src1 */

	 while (i1 < SYMCOUNT(*src1) && i2 < SYMCOUNT(*src2) && SYMBOLSET(*src2, i2)->order < SYMBOLSET(*src1, i1)->order)
	 {
	    dyncheck(dst, SYMSIZE(*dst) * 2);
	    SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src2, i2++);
	 }
      }

/*    Copy any remaining elements in src1 or src2 whichever is non-empty */

      while (i1 < SYMCOUNT(*src1))
      {
	 dyncheck(dst, SYMSIZE(*dst) * 2);
	 SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src1, i1++);
      }
      while (i2 < SYMCOUNT(*src2))
      {
	 dyncheck(dst, SYMSIZE(*dst) * 2);
	 SYMBOLSET(*dst, SYMCOUNT(*dst)++) = SYMBOLSET(*src2, i2++);
      }
   }
   else
      if (src1->array)
	 dyncopy(dst, src1);
      else
         if (src2->array)
	    dyncopy(dst, src2);
	 else
	    symbolset_alloc(dst, INITIAL_SYMBOLSET_SIZE);
}


char *unique_name(void)
{
/* Generate a unique symbol name for the symbol table This is used */
/* for ignored regular expressions which don't have token names	   */

   static int  unique;
   static char name[8 + 1];

   sprintf(name, "<%06d>", ++unique);
   return(name);
}
