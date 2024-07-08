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

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_definitions.h"
#include "partree_definitions.h"
#include "routine_definitions.h"
#include "sdtgen_definitions.h"
#include "symbols_definitions.h"
#include "tables_definitions.h"

#include "dynarray_functions.h"
#include "parser_functions.h"
#include "partree_functions.h"
#include "symbols_functions.h"
#include "utility_functions.h"


static int	      char_type(treenode *, char *);
static int	      decode_char(unsigned char **);
static unsigned char *decode_string(unsigned char *);
static void	      parser_tokens(sdt_tables *, treenode *);
static void	      scanner_tokens(sdt_tables *, treenode *);


static int char_type
(
   treenode *tree,
   char	    *value
)
{
/* Determine the type of a character abstract syntax tree */
/* EMPTY_CHARACTER if it results in epsilon		  */
/* SINGLE_CHARACTER if it results in a single character	  */
/* CHARACTER_CLASS if it results in a class of characters */
/* CHARACTER_STRING if it results in multiple characters  */

   int		  count;
   bool		  class;
   int		  type;
   treenode	 *node;
   unsigned char *string;

   if (tree->node.count != LEAF)
      switch (tree->node.type)
      {
	 case '.':	/* Concatenation */
	    for (count = 0, class = false, node = tree->node.entry[0]; node; node = node->node.next)
	       if ((type = char_type(node, value)) == SINGLE_CHARACTER || type == CHARACTER_CLASS)
	       {
		  if (type == CHARACTER_CLASS)
		     class = true;
		  count++;
	       }
	       else
		  if (type == CHARACTER_STRING)
		     return(type);
	    if (count == 0)
	       return(EMPTY_CHARACTER);
	    else
	       if (count == 1)
		  return((class) ? CHARACTER_CLASS : SINGLE_CHARACTER);
	       else
		  return(CHARACTER_STRING);

	 case '|':	/* Alternation */
	    for (count = 0, node = tree->node.entry[0]; node; node = node->node.next)
	    {
	       if ((type = char_type(node, value)) == CHARACTER_STRING)
		  return(type);

	       if (type != EMPTY_CHARACTER)
		  count++;
	    }
	    return((count <= 1) ? SINGLE_CHARACTER : CHARACTER_CLASS);

	 case '-':	/* Difference */
	 case '~':	/* Complement */
	 case ':':	/* Range */
	    return(CHARACTER_CLASS);

	 case '*':	/* 0 or more */
	 case '+':	/* 1 or more */
	    if (char_type(tree->node.entry[0], value) == EMPTY_CHARACTER)
	       return(EMPTY_CHARACTER);
	    else
	       return(CHARACTER_STRING);

	 default:
	    fprintf(stderr, "unexpected node type '%c' in character expression\n", tree->node.type);
	    exit(1);
      }
   else
      switch(tree->leaf.type)
      {
	 case EPSILON:
	    return(EMPTY_CHARACTER);

	 case ZEROBYTE:
	    return(SINGLE_CHARACTER);

	 case CLASS:
	    return(CHARACTER_CLASS);

	 case CHARACTER:
	    string = decode_string(tree->leaf.value.value);
	    if (string[0] && !string[1])
	    {
	       if (value)
		  *value = string[0];
	       type = SINGLE_CHARACTER;
	    }
	    else
	       type = CHARACTER_STRING;
	    free(string);
	    return(type);

	 default:
	    fprintf(stderr, "unexpected leaf type %d in character expression\n", tree->leaf.type);
	    exit(1);
      }
}


static int decode_char
(
   unsigned char **ccode
)
{
/* Convert escape character codes into a character */

   unsigned char *start;
   unsigned char *value;
   int		  chr;
   int		  digit;
   int		  i;

/* Check for escape sequence or return character */

   if (**ccode != '\\')
      return(*(*ccode)++);

   start = *ccode + 1;
   value = start;

/* Check for hexadecimal character code */

   chr = 0;
   if (*value == 'x')
   {
      value++;
      for (i = 0; i < 2; i++)
      {
              if (*value >= 'a' && *value <= 'f')
	    digit = *value++ - 'a' + 10;
	 else if (*value >= 'A' && *value <= 'F')
	    digit = *value++ - 'A' + 10;
	 else if (*value >= '0' && *value <= '9')
	    digit = *value++ - '0';
	 else
	    break;

	 chr = chr * 16 + digit;
      }
   }
   else

/*    Check for octal character code */

      for (i = 0; i < 3; i++)
      {
	 if (*value >= '0' && *value <= '7')
	    digit = *value++ - '0';
	 else
	    break;

/*	 If Adding this new digit is still a valid character, accept it */

	 if (chr * 8 + digit < 0xFF)
	    chr = chr * 8 + digit;
	 else
	 {
/*	    Return the invalid digit and accept the character so far */

	    value--;
	    break;
	 }
      }

/* If the interpreted character code is valid, accept it */

   if (chr > 0 && chr <= 0xFF)
   {
      *ccode = value;
      return(chr);
   }

/* Check for special character specification */

   switch (*start)
   {
      case 0:
	 *ccode = start;
	 return('\\');
	 break;

      case 'a':
	 chr = '\a';
	 break;

      case 'b':
	 chr = '\b';
	 break;

      case 'e':
	 chr = '\e';
	 break;

      case 'f':
	 chr = '\f';
	 break;

      case 'n':
	 chr = '\n';
	 break;

      case 'r':
	 chr = '\r';
	 break;

      case 't':
	 chr = '\t';
	 break;

      case 'v':
	 chr = '\v';
	 break;

      default:
	 chr = *start;
   }
   *ccode = start + 1;
   return(chr);
}


static unsigned char *decode_string
(
   unsigned char *src
)
{
/* Create copy of string with character codes decoded */

   unsigned char *string;
   unsigned char *dst;
   int		  c;

/* The resulting string cannot be longer than the source */

   if (!(string = malloc(strlen(src) + 1)))
      out_of_memory();

/* Decode source to destination and return it */

   dst = string;
   while (c = decode_char(&src))
      *dst++ = c;
   *dst = '\0';
   return(string);
}


void free_routine
(
   sdt_tables *tables
)
{
/* Free semantic routine variables */

   dynfree(&tables->semstack);
   free(tables->termtable);
   tables->termtable    = NULL;
   tables->termcount    = 0;
   free(tables->nontermtable);
   tables->nontermtable = NULL;
   tables->nontermcount = 0;
   free(tables->name);
   tables->name         = NULL;
   free(tables->title);
   tables->title        = NULL;
   tables->startsym     = NULL;
   tables->sentinel     = NULL;
   free_tree(tables->scanner);
   tables->scanner      = NULL;
   free_tree(tables->parser);
   tables->parser       = NULL;
}


void init_routine
(
   sdt_tables *tables
)
{
/* Initialize semantic routine variables */

   tables->display             = 0;
   tables->debug               = 0;
   tables->process	       = true;
   tables->options             = 0;
   dynalloc(&tables->semstack, sizeof(treenode *), INITIAL_SEMANTIC_SIZE);
   tables->tokenval.token      = 0;
   tables->tokenval.flags      = 0;
   tables->tokenval.precedence = 0;
   tables->tokenval.insert     = 1;
   tables->tokenval.delete     = 1;
   tables->termtable           = NULL;
   tables->termcount           = 0;
   tables->nontermtable        = NULL;
   tables->nontermcount        = 0;
   tables->name                = NULL;
   tables->title               = NULL;
   tables->startsym            = NULL;
   tables->repaircost          = 0;
   tables->repaircontext       = 0;
   tables->sentinel            = NULL;
   tables->scanner             = NULL;
   tables->parser              = NULL;
}


void install_token
(
   sdt_tables *tables,
   tokenentry *token
)
{
/* Hook in between scanner and parser */
}


static void parser_tokens
(
   sdt_tables *tables,
   treenode   *tree
)
{
/* Ensure all nonterminals have token numbers */

   treenode *node;
   treenode *rhside;
   treenode *symbol;

   if (tree->node.count != LEAF && tree->node.type == '_')
   {
/*    Assign token values to all nonterminals on the left hand side of a production */

      for (node = tree->node.entry[0]; node; node = node->node.next)
	 if (node->node.count != LEAF && node->node.type == '>' &&
	     node->node.entry[0] && node->node.entry[0]->node.count == LEAF && node->node.entry[0]->leaf.type == REFERENCE)
	    if (node->node.entry[0]->leaf.value.symbol && node->node.entry[0]->leaf.value.symbol->value.value.token == 0)
	       node->node.entry[0]->leaf.value.symbol->value.value.token = tables->termcount + ++tables->nontermcount;

/*    Generate error messages for any nonterminals which still have no token number */

      for (node = tree->node.entry[0]; node; node = node->node.next)
	 if (node->node.count != LEAF && node->node.type == '>' &&
	     node->node.entry[1] && node->node.entry[1]->node.count != LEAF && node->node.entry[1]->node.type == '|')
	    for (rhside = node->node.entry[1]->node.entry[0]; rhside; rhside = rhside->node.next)
	       if (rhside->node.count != LEAF && rhside->node.type == '.')
	       {
		  for (symbol = rhside->node.entry[0]; symbol; symbol = symbol->node.next)
		     if (symbol->node.count == LEAF && symbol->leaf.type == REFERENCE)
			if (symbol->leaf.value.symbol && symbol->leaf.value.symbol->value.value.token == 0)
			{
			   record_error(tables, &tables->position, "Undefined nonterminal <%s>", symbol->leaf.value.symbol->symbol);
			   symbol->leaf.value.symbol->value.value.token = tables->termcount + ++tables->nontermcount;
			}
	       }
	       else
	          if (rhside->node.count == LEAF && rhside->leaf.type == REFERENCE)
		     if (rhside->leaf.value.symbol && rhside->leaf.value.symbol->value.value.token == 0)
		     {
			record_error(tables, &tables->position, "Undefined nonterminal <%s>", rhside->leaf.value.symbol->symbol);
			rhside->leaf.value.symbol->value.value.token = tables->termcount + ++tables->nontermcount;
		     }

/*    Now that all the token values have been assigned */
/*    Create a map from the values to the symbol entry */

      if (tables->nontermtable = (symbolentry **) malloc((tables->nontermcount + 1) * sizeof(*tables->nontermtable)))
         memset(tables->nontermtable, 0, (tables->nontermcount + 1) * sizeof(*tables->nontermtable));
      else
         out_of_memory();

      for (node = tree->node.entry[0]; node; node = node->node.next)
      {
	 if (node->node.count != LEAF && node->node.type == '>' &&
	     node->node.entry[0] && node->node.entry[0]->node.count == LEAF && node->node.entry[0]->leaf.type == REFERENCE)
	    if (node->node.entry[0]->leaf.value.symbol && !tables->nontermtable[node->node.entry[0]->leaf.value.symbol->value.value.token - tables->termcount])
	       tables->nontermtable[node->node.entry[0]->leaf.value.symbol->value.value.token - tables->termcount] = node->node.entry[0]->leaf.value.symbol;
	 if (node->node.count != LEAF && node->node.type == '>' &&
	     node->node.entry[1] && node->node.entry[1]->node.count != LEAF && node->node.entry[1]->node.type == '|')
	    for (rhside = node->node.entry[1]->node.entry[0]; rhside; rhside = rhside->node.next)
	       if (rhside->node.count != LEAF && rhside->node.type == '.')
	       {
		  for (symbol = rhside->node.entry[0]; symbol; symbol = symbol->node.next)
		     if (symbol->node.count == LEAF && symbol->leaf.type == REFERENCE)
			if (symbol->leaf.value.symbol && symbol->leaf.value.symbol->type == NONTERMINAL &&
			   !tables->nontermtable[symbol->leaf.value.symbol->value.value.token - tables->termcount])
			   tables->nontermtable[symbol->leaf.value.symbol->value.value.token - tables->termcount] = symbol->leaf.value.symbol;
	       }
	       else
	          if (rhside->node.count == LEAF && rhside->leaf.type == REFERENCE)
		     if (rhside->leaf.value.symbol && rhside->leaf.value.symbol->type == NONTERMINAL &&
			!tables->nontermtable[rhside->leaf.value.symbol->value.value.token - tables->termcount])
			tables->nontermtable[rhside->leaf.value.symbol->value.value.token - tables->termcount] = rhside->leaf.value.symbol;
      }
   }
}


void perform_action
(
   sdt_tables *tables,
   int	       semno
)
{
   int		length1;
   int		length2;
   symbolentry *symbol1;
   symbolentry *symbol2;
   long		value;
   int		range[2];
   treenode    *node;
   treenode    *tree;
   int		type1;
   int		type2;
   char		lower;
   char		upper;
   int		i;

   switch (semno)
   {
      case  1:		/* Set name of syntax directed translator */
	 tables->name                  = PARSTACK(PARCOUNT - 2).symbol;
	 PARSTACK(PARCOUNT - 2).symbol = NULL;
	 break;

      case  2:		/* Set title string for listing */
	 if (length1 = (PARSTACK(PARCOUNT - 2).symbol) ? strlen(PARSTACK(PARCOUNT - 2).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 2).symbol[length1 - 1] != PARSTACK(PARCOUNT - 2).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 2).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 2).symbol[length1 - 1] = '\0';
	 if (length1 < 3)
	    tables->title = NULL;
	 else
	    if (!(tables->title = strdup(&PARSTACK(PARCOUNT - 2).symbol[1])))
	       out_of_memory();
	 break;

      case  3:		/* Finish creation of scanner regular expressions */
	 if (SEMCOUNT)
	 {
/*	    Add special token definition for end of file.  The single quotes */
/*	    in the name prevent it from being defined or used in the input   */

	    tables->termcount++;
	    tables->tokenval.token++;

	    (tables->sentinel = lookup_symbol(tables, "\"'$'\"", TERMINAL, INSERT))->value.value = tables->tokenval;
	    tables->sentinel->value.value.insert = (MAXCOST + 1) / 2 - 1;
	    tables->sentinel->value.value.delete = MAXCOST;

	    dyncheck(&tables->semstack, SEMSIZE * 2);
	    SEMSTACK(SEMCOUNT) = copy_tree(lookup_symbol(tables, "EOF", DEFINITION, LOOKUP)->value.tree);
	    if (SEMSTACK(SEMCOUNT)->node.count == LEAF || SEMSTACK(SEMCOUNT)->node.type != '.')
	       SEMSTACK(SEMCOUNT) = create_binary('.', SEMSTACK(SEMCOUNT), create_leaf(REFERENCE, tables->sentinel));
	    else
	       append_node(SEMSTACK(SEMCOUNT), create_leaf(REFERENCE, tables->sentinel));

	    if (SEMSTACK(SEMCOUNT - 1))
	       if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '|')
		  SEMSTACK(SEMCOUNT - 1) = create_binary('|', SEMSTACK(SEMCOUNT - 1), SEMSTACK(SEMCOUNT));
	       else
		  append_node(SEMSTACK(SEMCOUNT - 1), SEMSTACK(SEMCOUNT));
	    else
	       SEMSTACK(SEMCOUNT - 1) = SEMSTACK(SEMCOUNT);

/*	    Assign token number to ignored regular expression and build tables of all terminals */

	    if (SEMSTACK(SEMCOUNT - 1))
	       scanner_tokens(tables, SEMSTACK(SEMCOUNT - 1));

/*	    Return the scanner syntax tree */

	    tables->scanner = SEMSTACK(--SEMCOUNT);
	 }
	 else
	    tables->scanner = NULL;
	 break;

      case  4:		/* Finish creation of parser productions */
	 if (SEMCOUNT && SEMSTACK(SEMCOUNT - 1))
	 {
/*	    If the start symbol hasn't been specified select the LHS symbol of the first production */

	    if (!tables->startsym && SEMSTACK(SEMCOUNT - 1)->node.type != LEAF)
	       if (SEMSTACK(SEMCOUNT - 1)->node.type == '_')
		  tables->startsym = SEMSTACK(SEMCOUNT - 1)->node.entry[0]->node.entry[0]->leaf.value.symbol;
	       else
		  tables->startsym = SEMSTACK(SEMCOUNT - 1)->node.entry[0]->leaf.value.symbol;

/*	    Add a production for the distinguished symbol */

	    dyncheck(&tables->semstack, SEMSIZE * 2);
	    SEMSTACK(SEMCOUNT) = create_binary('>',
	       create_leaf(REFERENCE, lookup_symbol(tables, "<Goal>", NONTERMINAL, INSERT)),
	       create_unary('|', create_binary('.', create_leaf(REFERENCE, tables->startsym), create_leaf(REFERENCE, tables->sentinel))));
	    if (!SEMSTACK(SEMCOUNT - 1))
	       SEMSTACK(SEMCOUNT - 1) = SEMSTACK(SEMCOUNT);
	    else
	       if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '_')
		  SEMSTACK(SEMCOUNT - 1) = create_binary('_', SEMSTACK(SEMCOUNT), SEMSTACK(SEMCOUNT - 1));
	       else
		  prefix_node(SEMSTACK(SEMCOUNT - 1), SEMSTACK(SEMCOUNT));

/*	    Assign token numbers to all nonterminals */

	    if (SEMSTACK(SEMCOUNT - 1))
	       parser_tokens(tables, SEMSTACK(SEMCOUNT - 1));

/*	    Return the parser syntax tree */

	    tables->parser = SEMSTACK(--SEMCOUNT);
	 }
	 else
	    tables->parser = NULL;
	 break;

      case  5:		/* Process SDTGEN option value */
	 if (PARSTACK(PARCOUNT - 1).symbol)
		 if (!strcasecmp(PARSTACK(PARCOUNT - 1).symbol, "AMBIGUOUS"))
	       tables->options |= AMBIGUOUS;
	    else if (!strcasecmp(PARSTACK(PARCOUNT - 1).symbol, "ERRORREPAIR"))
	       tables->options |= ERRORREPAIR;
	    else if (!strcasecmp(PARSTACK(PARCOUNT - 1).symbol, "SHIFTREDUCE"))
	       tables->options |= DEFAULTREDUCE;
	    else if (!strcasecmp(PARSTACK(PARCOUNT - 1).symbol, "SPLITSTATES"))
	       tables->options |= SPLITSTATES;
	    else
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Unknown parser option ignored");
	 break;

      case  6:		/* Define a regular expression */
	 if (PARSTACK(PARCOUNT - 4).symbol)
	    if (lookup_symbol(tables, PARSTACK(PARCOUNT - 4).symbol, DEFINITION, LOOKUP))
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Duplicate symbol definition ignored");
	       free_tree(SEMSTACK(--SEMCOUNT));
	    }
	    else
	       if (!SEMSTACK(SEMCOUNT - 1))
	       {
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid symbol definition");
		  free_tree(SEMSTACK(--SEMCOUNT));
	       }
	       else
		  lookup_symbol(tables, PARSTACK(PARCOUNT - 4).symbol, DEFINITION, INSERT)->value.tree = SEMSTACK(--SEMCOUNT);
	 else
	    free_tree(SEMSTACK(--SEMCOUNT));
	 break;

      case  7:		/* Alternate regular expressions */
	 if (!SEMSTACK(SEMCOUNT - 2))
	    SEMSTACK(SEMCOUNT - 2) = SEMSTACK(SEMCOUNT - 1);
	 else
	    if (SEMSTACK(SEMCOUNT - 1))
	       if (SEMSTACK(SEMCOUNT - 2)->node.count == LEAF || SEMSTACK(SEMCOUNT - 2)->node.type != '|')
		  SEMSTACK(SEMCOUNT - 2) = create_binary('|', SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	       else
		  append_node(SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	 SEMCOUNT--;
	 break;

      case  8:		/* Create token from regular expression */
	 if (length1 = (PARSTACK(PARCOUNT - 5).symbol) ? strlen(PARSTACK(PARCOUNT - 5).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 5).symbol[length1 - 1] != PARSTACK(PARCOUNT - 5).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 5).symbol[length1 - 1] = '\0';
	 if (length1 >= 3)
	    if (!lookup_symbol(tables, &PARSTACK(PARCOUNT - 5).symbol[1], TERMINAL, LOOKUP))
	    {
/*	       If the regular expression does not evaluate to epsilon it needs a token value */

	       if (char_type(SEMSTACK(SEMCOUNT - 1), NULL) != EMPTY_CHARACTER)
	       {
		  tables->termcount++;
		  tables->tokenval.token++;

/*		  If the token hasn't picked an associativity force NONE */

		  if ((tables->tokenval.flags & ASSOCIATIVITY) == 0)
		     tables->tokenval.flags |= NONE;
	       }
	       else
		  tables->tokenval.flags = EMPTY;

	       (symbol1 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 5).symbol[1], TERMINAL, INSERT))->value.value = tables->tokenval;
	       if (symbol1->value.value.flags & EMPTY)
	       {
		  symbol1->value.value.token  = 0;
		  symbol1->value.value.insert = 0;
		  symbol1->value.value.delete = 0;
	       }

	       if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '.')
		  SEMSTACK(SEMCOUNT - 1) = create_binary('.', SEMSTACK(SEMCOUNT - 1), create_leaf(REFERENCE, symbol1));
	       else
		  append_node(SEMSTACK(SEMCOUNT - 1), create_leaf(REFERENCE, symbol1));
	    }
	    else
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Duplicate token definition ignored");

	       free_tree(SEMSTACK(SEMCOUNT - 1));
	       SEMSTACK(SEMCOUNT - 1) = NULL;
	    }
	 else
	 {
	    if (length1 == 2)
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid token definition");
	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = NULL;
	 }

/*	 Reset token values for next token */

	 tables->tokenval.flags      = 0;
	 tables->tokenval.precedence = 0;
	 tables->tokenval.insert     = 1;
	 tables->tokenval.delete     = 1;
	 break;

      case  9:		/* Create alias for token */
	 if (length1 = (PARSTACK(PARCOUNT - 5).symbol) ? strlen(PARSTACK(PARCOUNT - 5).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 5).symbol[length1 - 1] != PARSTACK(PARCOUNT - 5).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 5).symbol[length1 - 1] = '\0';
	 if (length2 = (PARSTACK(PARCOUNT - 3).symbol) ? strlen(PARSTACK(PARCOUNT - 3).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 3).symbol[length2 - 1] != PARSTACK(PARCOUNT - 3).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 3).symbol[length2 - 1] = '\0';
	 if (length1 >= 3)
	    if (!lookup_symbol(tables, &PARSTACK(PARCOUNT - 5).symbol[1], TERMINAL, LOOKUP))
	    {
	       if (length2 >= 3)
		  if (symbol2 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 3).symbol[1], TERMINAL, LOOKUP))
		     if (symbol2->value.value.flags & ALIAS)
			record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Cannot define an alias for an alias");
		     else
		     {
/*			Create a new token with separate attributes but the same token number */

/*			Copy over the flags associated with the definition token's regular expression */

			tables->tokenval.flags  = tables->tokenval.flags & ~(INSTALL | CASE | EMPTY) | symbol2->value.value.flags & (INSTALL | CASE | EMPTY);

/*			If the alias token hasn't picked an associativity force NONE */

			if ((tables->tokenval.flags & ASSOCIATIVITY) == 0)
			   tables->tokenval.flags |= NONE;

/*			And flag this token as an alias */

			tables->tokenval.flags |= ALIAS;

/*			Create the alias and set its token number to that of the definition */

			(symbol1 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 5).symbol[1], TERMINAL, INSERT))->value.value = tables->tokenval;
			symbol1->value.value.token = symbol2->value.value.token;

/*			Add the new alias to the end of the alias list in the original token */

			while (symbol2->alias)
			   symbol2 = symbol2->alias;
			symbol2->alias = symbol1;
		     }
		  else
		     record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Undefined alias definition");
	       else
		  if (length2 == 2)
		     record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid alias definition");
	    }
	    else
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Duplicate token alias ignored");
	 else
	    if (length1 == 2)
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid token alias");
	 SEMSTACK(SEMCOUNT++) = NULL;

/*	 Reset token values for next token */

	 tables->tokenval.flags      = 0;
	 tables->tokenval.precedence = 0;
	 tables->tokenval.insert     = 1;
	 tables->tokenval.delete     = 1;
	 break;

      case 10:		/* Create token from name string */
	 if (length1 = (PARSTACK(PARCOUNT - 3).symbol) ? strlen(PARSTACK(PARCOUNT - 3).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 3).symbol[length1 - 1] != PARSTACK(PARCOUNT - 3).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 3).symbol[length1 - 1] = '\0';
	 if (length1 >= 3)
	    if (!lookup_symbol(tables, &PARSTACK(PARCOUNT - 3).symbol[1], TERMINAL, LOOKUP))
	    {
/*	       If the token hasn't picked an associativity force NONE */

	       if ((tables->tokenval.flags & ASSOCIATIVITY) == 0)
		  tables->tokenval.flags |= NONE;

	       tables->termcount++;
	       tables->tokenval.token++;

	       (symbol1 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 3).symbol[1], TERMINAL, INSERT))->value.value = tables->tokenval;
	       dyncheck(&tables->semstack, SEMSIZE * 2);
	       SEMSTACK(SEMCOUNT++) = create_binary('.', create_leaf(CHARACTER, decode_string(&PARSTACK(PARCOUNT - 3).symbol[1])), create_leaf(REFERENCE, symbol1));
	    }
	    else
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Duplicate token definition ignored");
	 else
	    if (length1 == 2)
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid token definition");

/*	 Reset token values for next token */

	 tables->tokenval.flags      = 0;
	 tables->tokenval.precedence = 0;
	 tables->tokenval.insert     = 1;
	 tables->tokenval.delete     = 1;
	 break;

      case 11:		/* Set regular expression should be ignored */

/*	 Create unique symbol with token number 0 to indicate an ignored regular expression */

	 symbol1 = lookup_symbol(tables, unique_name(), TERMINAL, INSERT);
	 symbol1->value.value.token      = 0;
	 symbol1->value.value.flags      = 0;
	 symbol1->value.value.precedence = 0;
	 symbol1->value.value.insert     = 0;
	 symbol1->value.value.delete     = 0;
	 if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '.')
	    SEMSTACK(SEMCOUNT - 1) = create_binary('.', SEMSTACK(SEMCOUNT - 1), create_leaf(REFERENCE, symbol1));
	 else
	    append_node(SEMSTACK(SEMCOUNT - 1), create_leaf(REFERENCE, symbol1));
	 break;

      case 12:		/* Set starting left hand side symbol */
	 if (length1 = (PARSTACK(PARCOUNT - 2).symbol) ? strlen(PARSTACK(PARCOUNT - 2).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 2).symbol[length1 - 1] != '>')
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close angle bracket");
	    else
	       PARSTACK(PARCOUNT - 2).symbol[length1 - 1] = '\0';
	 if (length1 >= 3)
	    tables->startsym = lookup_symbol(tables, &PARSTACK(PARCOUNT - 2).symbol[1], NONTERMINAL, INSERT);
	 break;

      case 13:		/* Set default repair cost */
	 if (PARSTACK(PARCOUNT - 2).symbol)
	 {
	    i = ((value = atol(PARSTACK(PARCOUNT - 2).symbol)) <= INT_MAX) ? value : INT_MAX;
	    if (i == 0)
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Default error repair cost is invalid");
	       tables->repaircost = MAXCOST;
	    }
	    else
	       tables->repaircost = i;
	 }
	 break;

      case 14:		/* Set error correction context */
	 if (PARSTACK(PARCOUNT - 2).symbol)
	 {
	    i = ((value = atol(PARSTACK(PARCOUNT - 2).symbol)) <= INT_MAX) ? value : INT_MAX;
	    if (i == 0)
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Error repair context is invalid");
	       tables->repaircontext = 1;
	    }
	    else
	       tables->repaircontext = i;
	 }
	 break;

      case 15:		/* Add production to grammar */
	 if (!SEMSTACK(SEMCOUNT - 2))
	    SEMSTACK(SEMCOUNT - 2) = SEMSTACK(SEMCOUNT - 1);
	 else
	    if (SEMSTACK(SEMCOUNT - 1))
	       if (SEMSTACK(SEMCOUNT - 2)->node.count == LEAF || SEMSTACK(SEMCOUNT - 2)->node.type != '_')
		  SEMSTACK(SEMCOUNT - 2) = create_binary('_', SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	       else
		  append_node(SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	 SEMCOUNT--;
	 break;

      case 16:		/* Join LHS and RHS into production */
	 if (length1 = (PARSTACK(PARCOUNT - 4).symbol) ? strlen(PARSTACK(PARCOUNT - 4).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 4).symbol[length1 - 1] != '>')
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close angle bracket");
	    else
	       PARSTACK(PARCOUNT - 4).symbol[length1 - 1] = '\0';
	 if (length1 >= 3 && SEMSTACK(SEMCOUNT - 1))
	 {
/*	    The RHS is always a list of alternatives even if there is only one */

	    if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '|')
	       SEMSTACK(SEMCOUNT - 1) = create_unary('|', SEMSTACK(SEMCOUNT - 1));

	    SEMSTACK(SEMCOUNT - 1) = create_binary('>',
	       create_leaf(REFERENCE, lookup_symbol(tables, &PARSTACK(PARCOUNT - 4).symbol[1], NONTERMINAL, INSERT)),
	       SEMSTACK(SEMCOUNT - 1));
	 }
	 else
	 {
	    if (length1 == 2 || !SEMSTACK(SEMCOUNT - 1))
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Invalid grammar production");
	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = NULL;
	 }
	 break;

      case 17:		/* Concatenate two regular expressions */
	 if (!SEMSTACK(SEMCOUNT - 2))
	    SEMSTACK(SEMCOUNT - 2) = SEMSTACK(SEMCOUNT - 1);
	 else
	    if (SEMSTACK(SEMCOUNT - 1))
	       if (SEMSTACK(SEMCOUNT - 2)->node.count == LEAF || SEMSTACK(SEMCOUNT - 2)->node.type != '.')
		  SEMSTACK(SEMCOUNT - 2) = create_binary('.', SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	       else
		  append_node(SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	 SEMCOUNT--;
	 break;

      case 18:		/* Create a range of occurrences of a regular expression */
	 if (PARSTACK(PARCOUNT - 3).symbol && PARSTACK(PARCOUNT - 1).symbol)
	 {
	    range[0] = ((value = atol(PARSTACK(PARCOUNT - 3).symbol)) <= INT_MAX) ? value : INT_MAX;
	    range[1] = ((value = atol(PARSTACK(PARCOUNT - 1).symbol)) <= INT_MAX) ? value : INT_MAX;
	    if (range[0] > range[1])
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Lower bound of range is greater than upper bound");

	       free_tree(SEMSTACK(SEMCOUNT - 1));
	       SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	    }
	    else
	       if (SEMSTACK(SEMCOUNT - 1))
		  if (range[1] > 0)
		  {
		     node = SEMSTACK(SEMCOUNT - 1);
		     for (tree = copy_tree(node), i = range[0]; i > 1; i--)
			if (tree->node.count == LEAF || tree->node.type != '.')
			   tree = create_binary('.', tree, copy_tree(node));
			else
			   append_node(tree, copy_tree(node));
		     if (range[0] == 0)
			SEMSTACK(SEMCOUNT - 1) = create_binary('|', create_leaf(EPSILON), copy_tree(tree));
		     else
			SEMSTACK(SEMCOUNT - 1) = copy_tree(tree);
		     for (i = (range[0] > 0) ? range[0] : 1; i < range[1]; i++)
		     {
			if (tree->node.count == LEAF || tree->node.type != '.')
			   tree = create_binary('.', tree, copy_tree(node));
			else
			   tree = append_node(tree, copy_tree(node));
			if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '|')
			   SEMSTACK(SEMCOUNT - 1) = create_binary('|', SEMSTACK(SEMCOUNT - 1), copy_tree(tree));
			else
			   append_node(SEMSTACK(SEMCOUNT - 1), copy_tree(tree));
		     }
		     free_tree(node);
		     free_tree(tree);
		  }
		  else
		  {
		     free_tree(SEMSTACK(SEMCOUNT - 1));
		     SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
		  }
	 }
	 else
	 {
	    record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Lower and/or upper bound of range is invalid");

	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	 }
	 break;

      case 19:		/* Create multiple occurrences of a regular expression */
	 if (!PARSTACK(PARCOUNT - 1).symbol)
	 {
	    record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Number of occurrences is invalid");

	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	 }
	 else
	    if (SEMSTACK(SEMCOUNT - 1))
	    {
	       i = ((value = atol(PARSTACK(PARCOUNT - 1).symbol)) <= INT_MAX) ? value : INT_MAX;
	       if (i > 1)
	       {
		  node = copy_tree(SEMSTACK(SEMCOUNT - 1));
		  while (--i)
		     if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '.')
			SEMSTACK(SEMCOUNT - 1) = create_binary('.', SEMSTACK(SEMCOUNT - 1), copy_tree(node));
		     else
			append_node(SEMSTACK(SEMCOUNT - 1), copy_tree(node));
		  free_tree(node);
	       }
	       else
		  if (i == 0)
		  {
		     free_tree(SEMSTACK(SEMCOUNT - 1));
		     SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
		  }
	    }
	 break;

      case 20:		/* Create 0 or more occurrences of a regular expression */
	 if (SEMSTACK(SEMCOUNT - 1))
	    SEMSTACK(SEMCOUNT - 1) = create_unary('*', SEMSTACK(SEMCOUNT - 1));
	 break;

      case 21:		/* Create 1 or more occurrences of a regular expression */
	 if (SEMSTACK(SEMCOUNT - 1))
	    SEMSTACK(SEMCOUNT - 1) = create_unary('+', SEMSTACK(SEMCOUNT - 1));
	 break;

      case 22:		/* Create an optional regular expression */
	 if (SEMSTACK(SEMCOUNT - 1))
	    if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '|')
	       SEMSTACK(SEMCOUNT - 1) = create_binary('|', SEMSTACK(SEMCOUNT - 1), create_leaf(EPSILON));
	    else
	       append_node(SEMSTACK(SEMCOUNT - 1), create_leaf(EPSILON));
	 break;

      case 23:		/* Create a character difference computation */
	 if (SEMSTACK(SEMCOUNT - 2) && SEMSTACK(SEMCOUNT - 1))
	 {
	    type1 = char_type(SEMSTACK(SEMCOUNT - 2), NULL);
	    type2 = char_type(SEMSTACK(SEMCOUNT - 1), NULL);
	    if (type1 != CHARACTER_STRING && type2 != CHARACTER_STRING)
	    {
	       SEMSTACK(SEMCOUNT - 2) = create_binary('-', SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
	       SEMCOUNT--;
	    }
	    else
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Difference of complex expressions replaced with epsilon");

	       free_tree(SEMSTACK(--SEMCOUNT));
	       free_tree(SEMSTACK(SEMCOUNT - 1));
	       SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	    }
	 }
	 else
	 {
	    free_tree(SEMSTACK(--SEMCOUNT));
	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = NULL;
	 }
	 break;

      case 24:		/* Create the complement of a character */
	 if (SEMSTACK(SEMCOUNT - 1))
	    if (char_type(SEMSTACK(SEMCOUNT - 1), NULL) != CHARACTER_STRING)
	       SEMSTACK(SEMCOUNT - 1) = create_unary('~', SEMSTACK(SEMCOUNT - 1));
	    else
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Complement of complex expression replaced with complement of epsilon");

	       free_tree(SEMSTACK(SEMCOUNT - 1));
	       SEMSTACK(SEMCOUNT - 1) = create_unary('~', create_leaf(EPSILON));
	    }
	 break;

      case 25:		/* Create a range of characters */
	 if (SEMSTACK(SEMCOUNT - 2) && SEMSTACK(SEMCOUNT - 1))
	 {
	    type1 = char_type(SEMSTACK(SEMCOUNT - 2), &lower);
	    type2 = char_type(SEMSTACK(SEMCOUNT - 1), &upper);
	    if (type1 == SINGLE_CHARACTER && type2 == SINGLE_CHARACTER)
	       if (lower > upper)
	       {
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Lower bound of range greater than upper bound");

		  free_tree(SEMSTACK(--SEMCOUNT));
		  free_tree(SEMSTACK(SEMCOUNT - 1));
		  SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	       }
	       else
	       {
		  SEMSTACK(SEMCOUNT - 2) = create_binary(':', SEMSTACK(SEMCOUNT - 2), SEMSTACK(SEMCOUNT - 1));
		  SEMCOUNT--;
	       }
	    else
	    {
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Range of non-characters replaced with epsilon");

	       free_tree(SEMSTACK(--SEMCOUNT));
	       free_tree(SEMSTACK(SEMCOUNT - 1));
	       SEMSTACK(SEMCOUNT - 1) = create_leaf(EPSILON);
	    }
	 }
	 else
	 {
	    free_tree(SEMSTACK(--SEMCOUNT));
	    free_tree(SEMSTACK(SEMCOUNT - 1));
	    SEMSTACK(SEMCOUNT - 1) = NULL;
	 }
	 break;

      case 26:		/* Copy a predefined regular expression */
	 dyncheck(&tables->semstack, SEMSIZE * 2);
	 if (!PARSTACK(PARCOUNT - 1).symbol || !(symbol1 = lookup_symbol(tables, PARSTACK(PARCOUNT - 1).symbol, DEFINITION, LOOKUP)))
	 {
	    if (PARSTACK(PARCOUNT - 1).symbol)
	    {
	       if (strchr(PARSTACK(PARCOUNT - 1).symbol, '"'))
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "'%s' has not been previously defined", PARSTACK(PARCOUNT - 1).symbol);
	       else
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "\"%s\" has not been previously defined", PARSTACK(PARCOUNT - 1).symbol);
	       lookup_symbol(tables, PARSTACK(PARCOUNT - 1).symbol, DEFINITION, INSERT)->value.tree = create_leaf(EPSILON);
	    }

	    SEMSTACK(SEMCOUNT++) = create_leaf(EPSILON);
	 }
	 else
	    SEMSTACK(SEMCOUNT++) = copy_tree(symbol1->value.tree);
	 break;

      case 27:		/* Create transitions for a string */
	 if (length1 = (PARSTACK(PARCOUNT - 1).symbol) ? strlen(PARSTACK(PARCOUNT - 1).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 1).symbol[length1 - 1] != PARSTACK(PARCOUNT - 1).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 1).symbol[length1 - 1] = '\0';

	 dyncheck(&tables->semstack, SEMSIZE * 2);
	 SEMSTACK(SEMCOUNT++) = (length1 >= 3) ? create_leaf(CHARACTER, decode_string(&PARSTACK(PARCOUNT - 1).symbol[1])) : create_leaf(EPSILON);
	 break;

      case 28:		/* Create transition for a character class */
	 if (length1 = (PARSTACK(PARCOUNT - 1).symbol) ? strlen(PARSTACK(PARCOUNT - 1).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 1).symbol[length1 - 1] != ']')
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close square bracket");
	    else
	       PARSTACK(PARCOUNT - 1).symbol[length1 - 1] = '\0';

	 dyncheck(&tables->semstack, SEMSIZE * 2);
	 SEMSTACK(SEMCOUNT++) = (length1 >= 3) ? create_leaf(CLASS, decode_string(&PARSTACK(PARCOUNT - 1).symbol[1])) : create_leaf(EPSILON);
	 break;

      case 29:		/* Create a lookahead expression */

/*	 If there is a non-empty regular expression indicate it has a lookahead */

	 if (SEMSTACK(SEMCOUNT - 1) && char_type(SEMSTACK(SEMCOUNT - 1), NULL) != EMPTY_CHARACTER)
	    if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '.')
	       SEMSTACK(SEMCOUNT - 1) = create_binary('.', SEMSTACK(SEMCOUNT - 1), create_leaf(LOOKAHEAD));
	    else
	       append_node(SEMSTACK(SEMCOUNT - 1), create_leaf(LOOKAHEAD));
	 break;

      case 30:		/* Set token precedence */
	 if (!PARSTACK(PARCOUNT - 1).symbol)
	    value = 0L;
	 else
	    if ((value = atol(PARSTACK(PARCOUNT - 1).symbol)) > INT_MAX)
	       value = INT_MAX;

	 tables->tokenval.precedence = value;
	 break;

      case 31:		/* Set left associativity flag for token */
	 if (tables->tokenval.flags & ASSOCIATIVITY)
	    record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Token associativity has already be selected");
	 else
	    tables->tokenval.flags |= LEFT;
	 break;

      case 32:		/* Set right associativity flag for token */
	 if (tables->tokenval.flags & ASSOCIATIVITY)
	    record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Token associativity has already be selected");
	 else
	    tables->tokenval.flags |= RIGHT;
	 break;

      case 33:		/* Set no associativity flag for token */
	 if (tables->tokenval.flags & ASSOCIATIVITY)
	    record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Token associativity has already be selected");
	 else
	    tables->tokenval.flags |= NONE;
	 break;

      case 34:		/* Set token insertion cost */
	 if (!PARSTACK(PARCOUNT - 1).symbol)
	    value = 1L;
	 else
	    if ((value = atol(PARSTACK(PARCOUNT - 1).symbol)) > INT_MAX)
	       value = INT_MAX;

	 tables->tokenval.insert = value;
	 break;

      case 35:		/* Set token deletion cost */
	 if (!PARSTACK(PARCOUNT - 1).symbol)
	    value = 1L;
	 else
	    if ((value = atol(PARSTACK(PARCOUNT - 1).symbol)) > INT_MAX)
	       value = INT_MAX;

	 tables->tokenval.delete = value;
	 break;

      case 36:		/* Set install flag for token */
	 tables->tokenval.flags |= INSTALL;
	 break;

      case 37:		/* Set ignore case flag for token */
	 tables->tokenval.flags |= CASE;
	 break;

      case 38:		/* Append semantic routine to right hand side */
	 if (SEMSTACK(SEMCOUNT - 1))
	 {
	    node = create_leaf(SEMANTIC, (PARSTACK(PARCOUNT - 1).symbol) ? atol(&PARSTACK(PARCOUNT - 1).symbol[1]) : 0);
	    if (SEMSTACK(SEMCOUNT - 1)->node.count == LEAF || SEMSTACK(SEMCOUNT - 1)->node.type != '.')
	       SEMSTACK(SEMCOUNT - 1) = create_binary('.', SEMSTACK(SEMCOUNT - 1), node);
	    else
	       append_node(SEMSTACK(SEMCOUNT - 1), node);
	 }
	 break;

      case 39:		/* Create right hand side symbol from symbol */
	 if (length1 = (PARSTACK(PARCOUNT - 1).symbol) ? strlen(PARSTACK(PARCOUNT - 1).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 1).symbol[length1 - 1] != '>')
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close angle bracket");
	    else
	       PARSTACK(PARCOUNT - 1).symbol[length1 - 1] = '\0';
	 dyncheck(&tables->semstack, SEMSIZE * 2);
	 if (length1)
	    SEMSTACK(SEMCOUNT++) = create_leaf(REFERENCE, lookup_symbol(tables, &PARSTACK(PARCOUNT - 1).symbol[1], NONTERMINAL, INSERT));
	 else
	    SEMSTACK(SEMCOUNT++) = create_leaf(EPSILON);
	 break;

      case 40:		/* Create right hand side symbol from string */
	 if (length1 = (PARSTACK(PARCOUNT - 1).symbol) ? strlen(PARSTACK(PARCOUNT - 1).symbol) : 0)
	    if (PARSTACK(PARCOUNT - 1).symbol[length1 - 1] != PARSTACK(PARCOUNT - 1).symbol[0])
	       record_error(tables, &PARSTACK(PARCOUNT - 1).where, "%s", "Missing close quote");
	    else
	       PARSTACK(PARCOUNT - 1).symbol[length1 - 1] = '\0';

	 dyncheck(&tables->semstack, SEMSIZE * 2);
	 if (length1 < 3 || !(symbol1 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 1).symbol[1], TERMINAL, LOOKUP)))
	 {
	    if (length1 >= 3)
	    {
	       if (strchr(&PARSTACK(PARCOUNT - 1).symbol[1], '"'))
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "'%s' has not been previously defined", &PARSTACK(PARCOUNT - 1).symbol[1]);
	       else
		  record_error(tables, &PARSTACK(PARCOUNT - 1).where, "\"%s\" has not been previously defined", &PARSTACK(PARCOUNT - 1).symbol[1]);

	       symbol1 = lookup_symbol(tables, &PARSTACK(PARCOUNT - 1).symbol[1], TERMINAL, INSERT);
	       symbol1->value.value.token      = 0;
	       symbol1->value.value.flags      = 0;
	       symbol1->value.value.precedence = 0;
	       symbol1->value.value.insert     = 0;
	       symbol1->value.value.delete     = 0;
	    }

	    SEMSTACK(SEMCOUNT++) = create_leaf(EPSILON);
	 }
	 else
	    SEMSTACK(SEMCOUNT++) = (symbol1->value.value.token) ? create_leaf(REFERENCE, symbol1) : create_leaf(EPSILON);
	 break;
   }
}


static void scanner_tokens
(
   sdt_tables *tables,
   treenode   *tree
)
{
   treenode    *last;
   symbolentry *symbol;
   treenode    *node;

/* Each token defined in the scanner has a token number starting with 1.     */
/* All ignored regular expressions are given token numbers larger than the   */
/* numbers of actual terminals.  The scanner is given the number of actual   */
/* terminals and will never return a token number larger than that number.   */
/* This automatically causes the ignored regular expressions to be ignored.  */
/* When token numbers are assigned to the nonterminals in the grammar they   */
/* are given values starting at the number of terminals + 1.  Although there */
/* is overlap in the token numbers this cannot cause a collision.	     */

   if (tree->node.count != LEAF)
   {
      if (tree->node.type == '.')
      {
/*	 There is only a single token defined */

	 last = tree->node.entry[3];
	 if (last->node.count == LEAF && last->leaf.type == REFERENCE)
	    if ((symbol = last->leaf.value.symbol)->value.value.token == 0)
	       symbol->value.value.token = ++tables->tokenval.token;
      }
      else
	 if (tree->node.type == '|')

/*	    Check each alternative token defined */

	    for (node = tree->node.entry[0]; node; node = node->node.next)
	       if (node->node.count != LEAF && node->node.type == '.')
	       {
		  last = node->node.entry[3];
		  if (last->node.count == LEAF && last->leaf.type == REFERENCE)
		  {
		     symbol = last->leaf.value.symbol;
		     if (symbol->value.value.token == 0)
			symbol->value.value.token = ++tables->tokenval.token;
		  }
	       }

/*    Create a map from the values to the symbol entry */

      if (tables->termtable = (symbolentry **) malloc((tables->termcount + 1) * sizeof(*tables->termtable)))
         memset(tables->termtable, 0, (tables->termcount + 1) * sizeof(*tables->termtable));
      else
         out_of_memory();

      if (tree->node.type == '.')
      {
	 last = tree->node.entry[3];
	 if (last->node.count == LEAF && last->leaf.type == REFERENCE &&
	    !(last->leaf.value.symbol->value.value.flags & ALIAS) &&
	    last->leaf.value.symbol->value.value.token <= tables->termcount)
	    tables->termtable[last->leaf.value.symbol->value.value.token] = last->leaf.value.symbol;
      }
      else
	 if (tree->node.type == '|')
	    for (node = tree->node.entry[0]; node; node = node->node.next)
	       if (node->node.count != LEAF && node->node.type == '.')
	       {
		  last = node->node.entry[3];
		  if (last->node.count == LEAF && last->leaf.type == REFERENCE &&
		     !(last->leaf.value.symbol->value.value.flags & ALIAS) &&
		     last->leaf.value.symbol->value.value.token <= tables->termcount)
		     tables->termtable[last->leaf.value.symbol->value.value.token] = last->leaf.value.symbol;
	       }
   }
}
