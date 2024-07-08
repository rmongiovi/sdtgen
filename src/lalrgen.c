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

#include "dynarray_definitions.h"
#include "intset_definitions.h"
#include "lalrgen_definitions.h"
#include "parser_definitions.h"
#include "partree_definitions.h"
#include "sdtgen_definitions.h"
#include "symbols_definitions.h"
#include "tables_definitions.h"

#include "dynarray_functions.h"
#include "intset_functions.h"
#include "partree_functions.h"
#include "symbols_functions.h"
#include "utility_functions.h"


static void	    apply_closure(sdt_tables *, int, int);
static void	    build_productions(sdt_tables *);
static void	    build_repair(sdt_tables *);
static void	    build_table(sdt_tables *);
static bool	    check_conflicts(sdt_tables *, dynarray *);
static void	    compute_first(sdt_tables *);
static void	    compute_sortkeys(sdt_tables *);
static void	    copy_states(sdt_tables *, dynarray *, dynarray *);
static void	    display_ancestors(sdt_tables *, FILE *);
static void	    display_collection(sdt_tables *, FILE *);
static void	    display_crossref(sdt_tables *, FILE *);
static void	    display_first(sdt_tables *, FILE *);
static void	    display_grammar(sdt_tables *, FILE *);
static void	    display_item(sdt_tables *, int, int, int, int, int, int, int, int, int, int, FILE *);
static void	    display_productions(sdt_tables *, FILE *);
static void	    display_repair(sdt_tables *, FILE *);
static void	    display_table(sdt_tables *, FILE *);
static void	    find_conflict(sdt_tables *, int, collision *);
static symbolentry *find_marker(symbolset *, int);
static int	    find_update(sdt_tables *, int, int, target *);
static void	    group_conflicts(sdt_tables *, dynarray *, dynarray *);
static void	    insert_production(sdt_tables *, symbolentry *, treenode *);
static bool	    itemset_equal(sdt_tables *, dynarray *, int, dynarray *, int);
static void	    kernel_items(sdt_tables *, collision *);
static void	    list_gotos(sdt_tables *, int, int, FILE *);
static void	    list_items(sdt_tables *, int, int, FILE *);
static int	    lookup_goto(sdt_tables *, int, int);
static int	    map_state(dynarray *, int);
static void	    previous_states(sdt_tables *, dynarray *);
static void	    propagate_lookahead(sdt_tables *);
static void	    resolve_ambiguity(sdt_tables *, int);
static int	    set_action(sdt_tables *, int, int, int);
static void	    set_ambiguity(sdt_tables *, int, int, int, int, int, int);
static void	    setup_lookahead(sdt_tables *);
static void	    sort_productions(sdt_tables *);
static bool	    split_states(sdt_tables *, int);
static bool	    spontaneous_conflict(sdt_tables *, collision *);


static void apply_closure
(
   sdt_tables *tables,
   int	       state,
   int	       index
)
{
   int prod;
   int dot;
   int token;
   int i, j, k;

/* Add closure items until no new items are added */

   for (i = index; i < ITEMCOUNT(state); i++)
   {
      prod = ITEMSET(state, i).prod;
      dot  = ITEMSET(state, i).dot;

      if (dot < PRODUCTION(prod).length && RHSIDE(prod, dot)->type == NONTERMINAL)
      {
	 token = RHSIDE(prod, dot)->value.value.token;

/*	 The dot is in front of a nonterminal, add any unmarked definitions */

	 for (j = tables->lhsindex[token - tables->termcount]; j < PRODCOUNT && PRODUCTION(j).lhside->value.value.token == token; j++)
	 {
/*	    Search closure to see if production has already been added */

	    for (k = COLLECTION(state).kernel; k < ITEMCOUNT(state) && ITEMSET(state, k).prod != j; k++)
	       ;

/*	    If the production is not already present, append it to the set */

	    if (k >= ITEMCOUNT(state))
	    {
	       dyncheck(&COLLECTION(state).itemset, ITEMSIZE(state) * 2);
	       ITEMSET(state, k).prod = j;

/*	       Skip over leading epsilon terminals */

	       for (dot = 0; dot < RHSCOUNT(j); dot++)
		  if (RHSIDE(j, dot)->type != TERMINAL || (RHSIDE(j, dot)->value.value.flags & EMPTY) != EMPTY)
		     break;
	       ITEMSET(state, k).dot               = dot;

	       ITEMSET(state, k).descendant.state  = 0;
	       ITEMSET(state, k).descendant.item   = 0;
	       ITEMSET(state, k).ancestors.element = 0;
	       ITEMSET(state, k).ancestors.count   = 0;
	       ITEMSET(state, k).ancestors.size    = 0;
	       ITEMSET(state, k).ancestors.array   = NULL;
	       symbolset_alloc(&ITEMSET(state, k).follow, INITIAL_FOLLOW_SIZE);
	       ITEMSET(state, k).update.element    = 0;
	       ITEMSET(state, k).update.count      = 0;
	       ITEMSET(state, k).update.size       = 0;
	       ITEMSET(state, k).update.array      = NULL;
	       symbolset_alloc(&ITEMSET(state, k).lookahead, INITIAL_FOLLOW_SIZE);
	       ITEMCOUNT(state)++;

/*	       For error repair, closure must be depth first */

	       if (tables->options & ERRORREPAIR)
		  apply_closure(tables, state, ITEMCOUNT(state) - 1);
	    }
	 }
      }
   }
}


static void build_productions
(
   sdt_tables *tables
)
{
   treenode *node;
   int	     i;

/* Allocate index for production left hand side symbols */

   if (tables->lhsindex = (int *) malloc((tables->nontermcount + 1) * sizeof(*tables->lhsindex)))
      memset(tables->lhsindex, 0, (tables->nontermcount + 1) * sizeof(*tables->lhsindex));
   else
      out_of_memory();

/* Create fully expanded production list with all right hand sides collected */

   if (tables->parser->node.count != LEAF && tables->parser->node.type == '_')

/* Add productions to list in nonterminal token value order */

      for (i = tables->termcount + 1; i <= tables->termcount + tables->nontermcount; i++)

/*	 Scan the abstract syntax tree for productions with this token on the left hand side */

	 for (node = tables->parser->node.entry[0]; node; node = node->node.next)
	    if (node->node.count != LEAF && node->node.type == '>' &&
		node->node.entry[0] && node->node.entry[0]->node.count == LEAF && node->node.entry[0]->leaf.type == REFERENCE &&
		node->node.entry[1] && node->node.entry[1]->node.count != LEAF && node->node.entry[1]->node.type == '|')
	       if (node->node.entry[0]->leaf.value.symbol && node->node.entry[0]->leaf.value.symbol->type == NONTERMINAL &&
		   node->node.entry[0]->leaf.value.symbol->value.value.token == i)
		  insert_production(tables, node->node.entry[0]->leaf.value.symbol, node->node.entry[1]->node.entry[0]);

/* Set lhsindex for undefined nonterminals to PRODCOUNT */

   for (i = 1; i <= tables->nontermcount; i++)
      if (tables->lhsindex[i] == 0)
	 tables->lhsindex[i] = PRODCOUNT;
}


static void build_repair
(
   sdt_tables *tables
)
{
   int i, j;

/* Select error repair tokens for each state in the LR Characteristic Finite State Machine */
/* The ordering we have applied to the grammar combined with the depth-first closure used  */
/* when generating the items ensures that the first item in each state is part of the	   */
/* continuation automaton.  If the dot in that item is all the way to the right the error  */
/* repair value for this state is a reduce by that production.  If the dot is in front	   */
/* of a terminal, that token is the error repair value for this state.  If the dot is in   */
/* front of a nonterminal, the first item in the state's closure which is either a reduce  */
/* or a shift of a terminal is the error repair value for this state			   */

   if (tables->errortoken = (int *) malloc((COLLCOUNT + 1) * sizeof(*tables->errortoken)))
      memset(tables->errortoken, 0, (COLLCOUNT + 1) * sizeof(*tables->errortoken));
   else
      out_of_memory();

   if (tables->options & ERRORREPAIR)
   {
      for (i = 1; i < COLLCOUNT; i++)
	 if (ITEMSET(i, 0).dot >= PRODUCTION(ITEMSET(i, 0).prod).length)
	 {
/*	    Reduce actions are represented by the negative of the production number */

	    tables->errortoken[i] = -ITEMSET(i, 0).prod;
	 }
	 else
	    if (RHSIDE(ITEMSET(i, 0).prod, ITEMSET(i, 0).dot)->type == TERMINAL)
	    {
/*	       Shift actions are represented by the positive terminal token number */

	       tables->errortoken[i] = RHSIDE(ITEMSET(i, 0).prod, ITEMSET(i, 0).dot)->value.value.token;
	    }
	    else
	    {
	       for (j = COLLECTION(i).kernel; j < ITEMCOUNT(i); j++)
		  if (ITEMSET(i, j).dot >= PRODUCTION(ITEMSET(i, j).prod).length)
		  {
		     tables->errortoken[i] = -ITEMSET(i, j).prod;
		     break;
		  }
		  else
		     if (RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->type == TERMINAL)
		     {
			tables->errortoken[i] = RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->value.value.token;
			break;
		     }
	       if (j >= ITEMCOUNT(i))
		  fprintf(stderr, "Warning: state %d has no valid error repair value\n", i);
	    }

/*    Display error repair values if requested */

      if (tables->debug & DEBUG_E)
	 display_repair(tables, stdout);
   }
}


static void build_table
(
   sdt_tables *tables
)
{
   int *state;
   bool changed;
   int	result;
   int	i, j, k;

   do
   {
/*    Allocate state tables with COLLCOUNT rows and tables->termcount + tables->nontermcount + 1 columns */

      if (tables->lrstates = (int **) malloc(COLLCOUNT * (sizeof(*tables->lrstates) + (tables->termcount + tables->nontermcount + 1) * sizeof(**tables->lrstates))))
	 for (state = (int *) &tables->lrstates[COLLCOUNT], i = 0; i < COLLCOUNT; i++)
	 {
	    tables->lrstates[i] = state;
	    state              += tables->termcount + tables->nontermcount + 1;
	 }
      else
	 out_of_memory();

/*    Fill in the parser actions and check for collisions */

      for (changed = false, i = 1; i < COLLCOUNT; i++)
      {
	 memset(tables->lrstates[i], 0, (tables->termcount + tables->nontermcount + 1) * sizeof(**tables->lrstates));

/*	 For state 1 we add an ACCEPT action on augmented grammar goal token */

	 if (i == 1)
	    set_action(tables, i, lookup_symbol(tables, "<Goal>", NONTERMINAL, LOOKUP)->value.value.token, ACCEPT_OFFSET);

/*	 Put all the shift and shiftreduce actions into the table */

	 for (j = 0; j < ITEMCOUNT(i); j++)
	    if (ITEMSET(i, j).descendant.state)

/*	       This item has a descendant therefore it is a shift */

	       set_action(tables, i, RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->value.value.token, SHIFT_OFFSET + ITEMSET(i, j).descendant.state);
	    else
	       if (ITEMSET(i, j).dot < PRODUCTION(ITEMSET(i, j).prod).length)

/*		  The dot is in front of a token but there is no descendant so shiftreduce */

		  set_action(tables, i, RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->value.value.token, ITEMSET(i, j).prod);

/*	 Add all the reduce actions and check for conflicts */

	 for (result = NO_ERROR, j = 0; j < ITEMCOUNT(i); j++)
	    if (ITEMSET(i, j).dot >= PRODUCTION(ITEMSET(i, j).prod).length)

/*	       The dot is at the end of the production */

	       for (k = 0; k < SYMCOUNT(ITEMSET(i, j).lookahead); k++)
		  result |= set_action(tables, i, SYMBOLSET(ITEMSET(i, j).lookahead, k)->value.value.token, -ITEMSET(i, j).prod);

/*	 If we have a reduce-reduce error which can be repaired by splitting states */
/*	 recompute lookahead sets for the altered CFSM and restart table generation */

	 if ((result & REDUCE_REDUCE_ERROR) && split_states(tables, i))
	 {
	    propagate_lookahead(tables);
	    free(tables->lrstates);
	    changed = true;
	    break;
	 }

/*	 If we have a shift-reduce error pick table entries based on precedence and associativity */

	 if (result & SHIFT_REDUCE_ERROR)
	    resolve_ambiguity(tables, i);
      }
   }
   while (changed);
}


static bool check_conflicts
(
   sdt_tables *tables,
   dynarray   *conflict
)
{
   collision *src;
   bool	      failure;
   symbolset  follow1;
   int	      length;
   int	      state;
   symbolset  merge;
   symbolset  follow2;
   symbolset  intersect;
   int	      i, j, k, l;

   for (i = 0; i < DYNCOUNT(*conflict); i++)
   {
      src = &DYNARRAY(collision, *conflict, i);
      if (!src->success)
      {
	 for (failure = false, j = 0; !failure && j < src->count; j++)
	 {
/*	    Start with the accumulated spontaneous follow for the lane */

	    symbolset_copy(&follow1, &src->lanes[j].follow);

/*	    If the lane gets propagated lookaheads to kernel items add them in */

	    if (!src->lanes[j].complete)
	    {
	       length = DYNCOUNT(src->lanes[j].lane);
	       state  = DYNARRAY(laneentry, src->lanes[j].lane, length - 1).state;
	       for (k = 0; k < INTCOUNT(DYNARRAY(laneentry, src->lanes[j].lane, length - 1).items); k++)
	       {
		  symbolset_union(&merge, &follow1, &ITEMSET(state, INTSET(DYNARRAY(laneentry, src->lanes[j].lane, length - 1).items, k)).lookahead);
		  symbolset_free(&follow1);
		  follow1 = merge;
	       }
	    }

	    for (k = j + 1; !failure && k < src->count; k++)
	    {
/*	       Start with the accumulated spontaneous follow for the lane */

	       symbolset_copy(&follow2, &src->lanes[k].follow);

/*	       If the lane gets propagated lookaheads to kernel items add them in */

	       if (!src->lanes[k].complete)
	       {
		  length = DYNCOUNT(src->lanes[k].lane);
		  state  = DYNARRAY(laneentry, src->lanes[k].lane, length - 1).state;
		  for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, src->lanes[k].lane, length - 1).items); l++)
		  {
		     symbolset_union(&merge, &follow2, &ITEMSET(state, INTSET(DYNARRAY(laneentry, src->lanes[k].lane, length - 1).items, l)).lookahead);
		     symbolset_free(&follow2);
		     follow2 = merge;
		  }
	       }

/*	       Check if lane j conflicts with lane k */

	       symbolset_intersect(&intersect, &follow1, &follow2);
	       if (SYMCOUNT(intersect))
		  failure = true;
	       symbolset_free(&intersect);
	       symbolset_free(&follow2);
	    }
	    symbolset_free(&follow1);
	 }
	 if (!failure)
	    src->success = true;
      }
   }

/* Return failure if any of the conflicts are still unresolved */

   for (failure = false, i = 0; !failure && i < DYNCOUNT(*conflict); i++)
      failure = !DYNARRAY(collision, *conflict, i).success;
   return(failure);
}


static void compute_first
(
   sdt_tables *tables
)
{
   bool	     changed;
   symbolset merge;
   int	     i, j, k;

/* Each terminal token is its own firstset except for epsilon which is nullable */

   for (i = 1; i <= tables->termcount; i++)
      if (tables->termtable[i]->value.value.flags & EMPTY)
	 tables->first[i].nullable = true;
      else
      {
	 symbolset_insert(&tables->first[i].symbols, tables->termtable[i]);
	 tables->first[i].nullable = false;
      }

/* Continue adding tokens to nonterminal first sets until nothing changes */

   do
   {
      changed = false;
      for (i = 1; i <= tables->nontermcount; i++)
	 for (j = tables->lhsindex[i]; j < PRODCOUNT && PRODUCTION(j).lhside->value.value.token == tables->termcount + i; j++)
	 {
/*	    Add the first sets of the RHS until a non-nullable token is encountered */

	    for (k = 0; k < PRODUCTION(j).length; k++)
	    {
	       symbolset_union(&merge, &tables->first[tables->termcount + i].symbols, &tables->first[RHSIDE(j, k)->value.value.token].symbols);
	       if (symbolset_equal(&merge, &tables->first[tables->termcount + i].symbols))
		  symbolset_free(&merge);
	       else
	       {
		  symbolset_free(&tables->first[tables->termcount + i].symbols);
		  tables->first[tables->termcount + i].symbols = merge;
		  changed                                      = true;
	       }
	       if (!tables->first[RHSIDE(j, k)->value.value.token].nullable)
		  break;
	    }

/*	    If the entire RHS is nullable, record that the LHS is nullable */

	    if (k >= PRODUCTION(j).length)
	    {
	       if (!tables->first[tables->termcount + i].nullable)
		  changed = true;
	       tables->first[tables->termcount + i].nullable = true;
	    }
	 }
   }
   while (changed);
}


static void compute_sortkeys
(
   sdt_tables *tables
)
{
   bool changed;
   int	steps;
   int	insert;
   int	minsteps;
   int	mininsert;
   int	i, j, k;

   do
      for (changed = false, i = 1; i < PRODCOUNT; i++)
      {
	 steps  = 0;
	 insert = 0;
	 for (j = 0; j < PRODUCTION(i).length; j++)
	    if (RHSIDE(i, j)->type == NONTERMINAL)
	    {
/*	       Find the minimum number of steps and insert cost for this production */

	       minsteps  = INT_MAX;
	       mininsert = INT_MAX;
	       for (k = tables->lhsindex[RHSIDE(i, j)->value.value.token - tables->termcount]; k < PRODCOUNT; k++)
		  if (PRODUCTION(k).lhside == RHSIDE(i, j))
		  {
		     if (PRODUCTION(k).steps < minsteps)
			minsteps = PRODUCTION(k).steps;
		     if (PRODUCTION(k).insert < mininsert)
			mininsert = PRODUCTION(k).insert;
		  }
		  else
		     break;
	       if (steps < INT_MAX && minsteps < INT_MAX)
		  steps += minsteps;
	       else
		  steps = INT_MAX;
	       if (insert < INT_MAX && mininsert < INT_MAX)
		  insert += mininsert;
	       else
		  insert = INT_MAX;
	    }
	    else

/*	       Terminals have no steps but do have a defined insert cost */

	       if ((RHSIDE(i, j)->value.value.flags & EMPTY) != EMPTY && insert < INT_MAX)
		  insert += RHSIDE(i, j)->value.value.insert;

/*	 Update this production's values if necessary */

	 if (steps < INT_MAX && steps + 1 < PRODUCTION(i).steps)
	 {
	    PRODUCTION(i).steps = steps + 1;
	    changed = true;
	 }
	 if (insert < PRODUCTION(i).insert)
	 {
	    PRODUCTION(i).insert = insert;
	    changed = true;
	 }
      }
   while (changed);
}


static void copy_states
(
   sdt_tables *tables,
   dynarray   *conflict,
   dynarray   *groups
)
{
   intset     used;
   dynarray  *map;
   intset     list;
   collision *repair;
   int	      length;
   int	      state;
   statemap   next;
   int	      item;
   int	      i, j, k, l, m, n;

   intset_alloc(&used, INITIAL_MAP_SIZE);
   if (map = (dynarray *) malloc(DYNCOUNT(*conflict) * sizeof(*map)))
      for (i = 0; i < DYNCOUNT(*conflict); i++)
	 dynalloc(&map[i], sizeof(statemap), INITIAL_MAP_SIZE);
   else
      out_of_memory();

   for (i = 0; i < DYNCOUNT(*groups); i++)
   {
/*    Create a list of all the common state numbers used by the group */

      intset_alloc(&list, INITIAL_MAP_SIZE);
      for (j = 0; j < INTCOUNT(DYNARRAY(intset, *groups, i)); j++)
      {
	 repair = &DYNARRAY(collision, *conflict, INTSET(DYNARRAY(intset, *groups, i), j));
	 for (k = 0; k < repair->count; k++)
	    for (l = DYNCOUNT(repair->lanes[k].lane) - 2; l >= 0; l--)
	       intset_insert(&list, DYNARRAY(laneentry, repair->lanes[k].lane, l).state);
      }

/*    Reuse the original state or make a copy of all the states in the group */

      for (j = 0; j < INTCOUNT(list); j++)
	 if (intset_find(&used, state = INTSET(list, j)) >= 0)
	 {
/*	    Create a copy of this state */

	    dyncheck(&map[i], DYNSIZE(map[i]) * 2);
	    DYNARRAY(statemap, map[i], DYNCOUNT(map[i])).old = state;
	    DYNARRAY(statemap, map[i], DYNCOUNT(map[i])).new = COLLCOUNT;
	    DYNCOUNT(map[i])++;

/*	    Copy all the elements of each item */

	    dyncheck(&tables->collection, COLLSIZE * 2);
	    dynalloc(&COLLECTION(COLLCOUNT).itemset, ITEMELEMENT(state), ITEMCOUNT(state));
	    COLLECTION(COLLCOUNT).kernel = COLLECTION(state).kernel;
	    for (k = 0; k < ITEMCOUNT(state); k++)
	    {
	       ITEMSET(COLLCOUNT, k).prod       = ITEMSET(state, k).prod;
	       ITEMSET(COLLCOUNT, k).dot        = ITEMSET(state, k).dot;
	       ITEMSET(COLLCOUNT, k).descendant = ITEMSET(state, k).descendant;
	       symbolset_copy(&ITEMSET(COLLCOUNT, k).follow, &ITEMSET(state, k).follow);
	       if (k < COLLECTION(COLLCOUNT).kernel)
	       {
		  dynalloc(&ITEMSET(COLLCOUNT, k).ancestors, ANCELEMENT(state, k), ANCCOUNT(state, k));
		  dynalloc(&ITEMSET(COLLCOUNT, k).update, UPDELEMENT(state, k), UPDCOUNT(state, k));
		  for (l = 0; l < UPDCOUNT(state, k); l++)
		     UPDATE(COLLCOUNT, k, l) = UPDATE(state, k, l);
		  UPDCOUNT(COLLCOUNT, k) = UPDCOUNT(state, k);
	       }
	       else
	       {
		  ITEMSET(COLLCOUNT, k).ancestors.element = 0;
		  ITEMSET(COLLCOUNT, k).ancestors.count   = 0;
		  ITEMSET(COLLCOUNT, k).ancestors.size    = 0;
		  ITEMSET(COLLCOUNT, k).ancestors.array   = NULL;
		  ITEMSET(COLLCOUNT, k).update.element    = 0;
		  ITEMSET(COLLCOUNT, k).update.count      = 0;
		  ITEMSET(COLLCOUNT, k).update.size       = 0;
		  ITEMSET(COLLCOUNT, k).update.array      = NULL;
	       }
	       symbolset_alloc(&ITEMSET(COLLCOUNT, k).lookahead, INITIAL_FOLLOW_SIZE);
	    }
	    ITEMCOUNT(COLLCOUNT) = ITEMCOUNT(state);

/*	    Copy over the goto entries */

	    dynalloc(&COLLECTION(COLLCOUNT).gotos, GOTOELEMENT(state), GOTOCOUNT(state));
	    for (k = 0; k < GOTOCOUNT(state); k++)
	       GOTO(COLLCOUNT, k) = GOTO(state, k);
	    GOTOCOUNT(COLLCOUNT) = GOTOCOUNT(state);
	    COLLCOUNT++;
	 }
	 else
	    intset_insert(&used, state);
      intset_free(&list);
   }

/* Connect the new states for each group to the states at the end of each */
/* lane.  Since we map every item in a state there's no need to iterate   */
/* over the items in each lane.  They're only needed to grow the lanes    */

   for (i = 0; i < DYNCOUNT(*groups); i++)
      if (DYNCOUNT(map[i]))
      {
/*	 We have to map old to new state numbers in this group */

	 for (j = 0; j < INTCOUNT(DYNARRAY(intset, *groups, i)); j++)
	 {
/*	    Check every lane in the current conflict in this group */

	    repair = &DYNARRAY(collision, *conflict, INTSET(DYNARRAY(intset, *groups, i), j));
	    for (k = 0; k < repair->count; k++)
	    {
	       length = DYNCOUNT(repair->lanes[k].lane);
	       state  = DYNARRAY(laneentry, repair->lanes[k].lane, length - 1).state;

/*	       First map the successors of the state at the end of the lane to the new lane */

	       for (l = 0; l < ITEMCOUNT(state); l++)
	       {
/*		  If this item's decendant is a state that got copied fix decendant and ancestors */

		  next.old = ITEMSET(state, l).descendant.state;
		  next.new = map_state(&map[i], next.old);
		  if (next.new != next.old)
		  {
		     ITEMSET(state, l).descendant.state = next.new;
		     item = ITEMSET(state, l).descendant.item;

/*		     New states were generated without ancestors so all we have to do is add the new one */

		     dyncheck(&ITEMSET(next.new, item).ancestors, ANCSIZE(next.new, item) * 2);
		     ANCESTOR(next.new, item, ANCCOUNT(next.new, item)).state = state;
		     ANCESTOR(next.new, item, ANCCOUNT(next.new, item)).item  = l;
		     ANCCOUNT(next.new, item)++;

/*		     But since we've retargeted the state at the end of the lane from the */
/*		     old state to the new it is no longer an ancestor of the old state	  */

		     for (m = 0; m < ANCCOUNT(next.old, item) && (ANCESTOR(next.old, item, m).state != state || ANCESTOR(next.old, item, m).item != l); m++)
			;
		     if (m < ANCCOUNT(next.old, item))
		     {
			while (m + 1 < ANCCOUNT(next.old, item))
			{
			   ANCESTOR(next.old, item, m) = ANCESTOR(next.old, item, m + 1);
			   m++;
			}
			ANCCOUNT(next.old, item)--;
		     }
		  }

/*		  Map updates from the old to the new states */

		  if (l < COLLECTION(state).kernel)
		     for (m = 0; m < UPDCOUNT(state, l); m++)
			UPDATE(state, l, m).state = map_state(&map[i], UPDATE(state, l, m).state);
	       }

/*	       Map gotos from the old to the new states */

	       for (l = 0; l < GOTOCOUNT(state); l++)
		  GOTO(state, l).state = map_state(&map[i], GOTO(state, l).state);

/*	       Now map the remaining states in the lane */

	       for (l = length - 2; l >= 0; l--)
	       {
		  state = map_state(&map[i], DYNARRAY(laneentry, repair->lanes[k].lane, l).state);
		  for (m = 0; m < ITEMCOUNT(state); m++)
		  {
		     next.old = ITEMSET(state, m).descendant.state;
		     next.new = map_state(&map[i], next.old);
		     if (next.new != next.old)
		     {
			ITEMSET(state, m).descendant.state = next.new;
			item = ITEMSET(state, m).descendant.item;

			dyncheck(&ITEMSET(next.new, item).ancestors, ANCSIZE(next.new, item) * 2);
			ANCESTOR(next.new, item, ANCCOUNT(next.new, item)).state = state;
			ANCESTOR(next.new, item, ANCCOUNT(next.new, item)).item  = m;
			ANCCOUNT(next.new, item)++;
		     }
		     if (m < COLLECTION(state).kernel)
			for (n = 0; n < UPDCOUNT(state, m); n++)
			   UPDATE(state, m, n).state = map_state(&map[i], UPDATE(state, m, n).state);
		  }
		  for (m = 0; m < GOTOCOUNT(state); m++)
		     GOTO(state, m).state = map_state(&map[i], GOTO(state, m).state);

/*		  If the previous lane entry is the same state, skip it */

		  if (l > 0 && map_state(&map[i], DYNARRAY(laneentry, repair->lanes[k].lane, l - 1).state) == state)
		     l--;
	       }
	    }
	 }
      }
   intset_free(&used);
   for (i = 0; i < DYNCOUNT(*conflict); i++)
      dynfree(&map[i]);
   free(map);
}


static void display_ancestors
(
   sdt_tables *tables,
   FILE	      *fp
)
{
   intset *ancestors;
   int	  *token;
   int	   width1;
   int	   width2;
   int	   length;
   int	   i, j;

   if ((ancestors = (intset *) malloc(COLLCOUNT * sizeof(*ancestors))) &&
       (token     = (int *)    malloc(COLLCOUNT * sizeof(*token))))
      for (i = 1; i < COLLCOUNT; i++)
      {
	 intset_alloc(&ancestors[i], INITIAL_ANCESTOR_SIZE);
	 token[i] = 0;
      }
   else
      out_of_memory();

   for (i = 1; i < COLLCOUNT; i++)
      for (j = 0; j < GOTOCOUNT(i); j++)
      {
	 intset_insert(&ancestors[GOTO(i, j).state], i);
	 token[GOTO(i, j).state] = GOTO(i, j).token;
      }

/* Determine the maximum sizes of the various fields so they can be lined up */

   width1 = digit_count(COLLCOUNT);
   if (width1 < 5)
      width1 = 5;
   for (width2 = 0, i = 1; i < COLLCOUNT; i++)
      if (token[i])
      {
	 if (token[i] <= tables->termcount)
	    length = strlen(tables->termtable[token[i]]->symbol);
	 else
	    length = strlen(tables->nontermtable[token[i] - tables->termcount]->symbol);
	 if (length > width2)
	    width2 = length;
      }
   width2 += 2;
   if (width2 < 6)
      width2 = 6;

/* Display each state's ancestors and the GOTO symbol that led to it  */

   fprintf(fp, "%s\t%s\tAncestor States\n", tables->name, tables->title);
   fprintf(fp, "%*s.  %-*s  Ancestors\n", width1, "State", width2, "Symbol");
   for (i = 1; i < COLLCOUNT; i++)
   {
      fprintf(fp, "%*d.  ", width1, i);
      if (token[i])
      {
	 display_token(tables, token[i], fp);
	 if (token[i] <= tables->termcount)
	    length = strlen(tables->termtable[token[i]]->symbol);
	 else
	    length = strlen(tables->nontermtable[token[i] - tables->termcount]->symbol);
	 length += 2;
	 if (length < width2)
	    fprintf(fp, "%*s", width2 - length, " ");
      }
      else
	 fprintf(fp, "%*s", width2, " ");
      fputs("  ", fp);
      display_intset(&ancestors[i], fp);
      fputc('\n', fp);
   }
   fputc('\n', fp);

/* Free working ancestors */

   for (i = 1; i < COLLCOUNT; i++)
      intset_free(&ancestors[i]);
   free(ancestors);
   free(token);
}


static void display_collection
(
   sdt_tables *tables,
   FILE       *fp
)
{
   int width;
   int i;

   width = digit_count(COLLCOUNT);
   if (width < 5)
      width = 5;

   fprintf(fp, "%s\t%s\tCanonical Collection of LR Items\n", tables->name, tables->title);
   fprintf(fp, "%*s.  Items\n", width, "State");
   for (i = 1; i < COLLCOUNT; i++)
   {
      list_items(tables, i, width, fp);
      list_gotos(tables, i, width, fp);
      fputc('\n', fp);
   }
}


static void display_crossref
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display grammar terminal and nonterminal cross-reference */

   int	   width1;
   int	   width2;
   intset *lhsref;
   intset *rhsref;
   int	  *index;
   int	   save;
   int	   i, j;

/* Determine the maximum size of various fields so they can be lined up */

   width1 = digit_count(tables->termcount + tables->nontermcount + 1);
   if (width1 < 3)
      width1 = 3;
   for (width2 = 0, i = 1; i <= tables->termcount; i++)
      if ((j = strlen(tables->termtable[i]->symbol)) > width2)
	 width2 = j;
   for (i = 1; i <= tables->nontermcount; i++)
      if ((j = strlen(tables->nontermtable[i]->symbol)) > width2)
	 width2 = j;
   if (width2 < 5)
      width2 = 5;

   if ((lhsref = (intset *) malloc((tables->termcount + tables->nontermcount + 1) * sizeof(*lhsref))) &&
       (rhsref = (intset *) malloc((tables->termcount + tables->nontermcount + 1) * sizeof(*rhsref))))
      for (i = 1; i <= tables->termcount + tables->nontermcount; i++)
      {
	 intset_alloc(&lhsref[i], INITIAL_REFERENCE_SIZE);
	 intset_alloc(&rhsref[i], INITIAL_REFERENCE_SIZE);
      }
   else
      out_of_memory();

/* Scan grammar productions and record where tokens are used */

   for (i = 1; i < PRODCOUNT; i++)
   {
      intset_insert(&lhsref[PRODUCTION(i).lhside->value.value.token], i);
      for (j = 0; j < RHSCOUNT(i); j++)
         intset_insert(&rhsref[RHSIDE(i, j)->value.value.token], i);
   }

/* Create a token index and sort terminals and nonterminals independently */

   if (index = (int *) malloc((tables->termcount + tables->nontermcount + 1) * sizeof(*index)))
      for (i = 1; i <= tables->termcount + tables->nontermcount; i++)
	 index[i] = i;
   else
      out_of_memory();
   for (i = 2; i <= tables->termcount; i++)
   {
      save = index[i];
      for (j = i - 1; j >= 1 && strcmp(tables->termtable[index[j]]->symbol, tables->termtable[save]->symbol) > 0; j--)
	 index[j + 1] = index[j];
      index[j + 1] = save;
   }
   for (i = tables->termcount + 2; i <= tables->termcount + tables->nontermcount; i++)
   {
      save = index[i];
      for (j = i - 1; j >= tables->termcount + 1 && strcmp(tables->nontermtable[index[j] - tables->termcount]->symbol, tables->nontermtable[save - tables->termcount]->symbol) > 0; j--)
	 index[j + 1] = index[j];
      index[j + 1] = save;
   }

/* Display left and right hand side locations */

   fprintf(fp, "%s\t%s\tToken Cross-Reference\n", tables->name, tables->title);
   fprintf(fp, "%*s.  %-*s  References\n", width1, "Num", width2 + 2, "Token");
   for (i = 1; i <= tables->termcount; i++)
   {
/*    Right hand side references only for terminals */

      fprintf(fp, "%*d.  ", width1, index[i]);
      display_token(tables, index[i], fp);
      if ((j = strlen(tables->termtable[index[i]]->symbol)) < width2)
	 fprintf(fp, "%*s", width2 - j, " ");
      if (INTCOUNT(rhsref[index[i]]))
         fputs("  RHS", fp);
      else
         fputs("  Unused", fp);
      for (j = 0; j < INTCOUNT(rhsref[index[i]]); j++)
         fprintf(fp, " %d", INTSET(rhsref[index[i]], j));
      fputc('\n', fp);
   }
   for (i = tables->termcount + 1; i <= tables->termcount + tables->nontermcount; i++)
   {
/*    Left and right hand side references for nonterminals */

      fprintf(fp, "%*d.  ", width1, index[i]);
      display_token(tables, index[i], fp);
      if ((j = strlen(tables->nontermtable[index[i] - tables->termcount]->symbol)) < width2)
	 fprintf(fp, "%*s", width2 - j, " ");
      if (INTCOUNT(lhsref[index[i]]))
         fputs("  LHS", fp);
      else
         fputs("  Undefined", fp);
      for (j = 0; j < INTCOUNT(lhsref[index[i]]); j++)
         fprintf(fp, " %d", INTSET(lhsref[index[i]], j));
      fputc('\n', fp);
      fprintf(fp, "%*s   %*s", width1, " ", width2 + 2, " ");
      if (INTCOUNT(rhsref[index[i]]))
         fputs("  RHS", fp);
      else
         fputs("  Unused", fp);
      for (j = 0; j < INTCOUNT(rhsref[index[i]]); j++)
         fprintf(fp, " %d", INTSET(rhsref[index[i]], j));
      fputc('\n', fp);
   }
   fputc('\n', fp);

/* Free working cross-reference */

   for (i = 1; i <= tables->termcount + tables->nontermcount; i++)
   {
      intset_free(&lhsref[i]);
      intset_free(&rhsref[i]);
   }
   free(lhsref);
   free(rhsref);
   free(index);
}


static void display_first
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display nonterminal first sets and whether or not they are nullable */

   int width1;
   int width2;
   int i, j;

/* Determine the maximum size of various fields so they can be lined up */

   width1 = digit_count(tables->termcount + tables->nontermcount + 1);
   if (width1 < 3)
      width1 = 3;
   for (width2 = 0, i = 1; i <= tables->nontermcount; i++)
      if ((j = strlen(tables->nontermtable[i]->symbol)) > width2)
	 width2 = j;
   if (width2 < 5)
      width2 = 5;

   fprintf(fp, "%s\t%s\tNonterminal First Sets\n", tables->name, tables->title);
   fprintf(fp, "%*s.  Null  %-*s First Set\n", width1, "Num", width2 + 2, "Token");
   for (i = 1; i <= tables->nontermcount; i++)
   {
      fprintf(fp, "%*d.    %c   ", width1, tables->termcount + i, (tables->first[tables->termcount + i].nullable) ? 'N' : ' ');
      display_token(tables, tables->termcount + i, fp);
      if ((j = strlen(tables->nontermtable[i]->symbol)) < width2)
	 fprintf(fp, "%*s", width2 - j, " ");
      fputs(" [", fp);
      display_symbolset(&tables->first[tables->termcount + i].symbols, fp);
      fputs("]\n", fp);
   }
   fputc('\n', fp);
}


static void display_grammar
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display the grammar abstract syntax tree in readable form */

   treenode *tree;
   int	     width1;
   int	     width2;
   int	     size;
   bool	     skip;
   treenode *node;
   int	     i, j;

   fprintf(fp, "%s\t%s\tInput Grammar Productions\n", tables->name, tables->title);
   if (tables->parser->node.count != LEAF)
   {
/*    Find the largest left hand side and count the number of productions */

      for (width2 = 0, i = 0, tree = tables->parser->node.entry[0]; tree; tree = tree->node.next, i++)
	 if (tree->node.count == LEAF || tree->node.type != '>')
	 {
	    fprintf(stderr, "Production chain is corrupt\n");
	    exit(1);
	 }
	 else
	    if ((j = strlen(tree->node.entry[0]->leaf.value.symbol->symbol)) > width2)
	       width2 = j;
      width1 = digit_count(i);
      if (width1 < 3)
	 width1 = 3;

/*    Display the productions and their alternate right hand sides */

      fprintf(fp, "%*s.  Production\n", width1, "Num");
      for (i = 1, tree = tables->parser->node.entry[0]; tree; tree = tree->node.next, i++)
      {
	 fprintf(fp, "%*d.  ", width1, i);
	 display_symbol(tree->node.entry[0]->leaf.value.symbol, fp);
	 if ((size = strlen(tree->node.entry[0]->leaf.value.symbol->symbol)) < width2)
	    fprintf(fp, "%*s", width2 - size, " ");
	 fputs(" --> ", fp);
	 for (skip = true, node = tree->node.entry[1]->node.entry[0]; node; node = node->node.next)
	 {
	    if (!skip)
	       fprintf(fp, "%*s | ", width1 + 3 + width2 + 2 + 5 - 3, " ");
	    else
	       skip = false;
	    display_expression(tables, node, (node->node.count > BINARY) ? precedence('_') : 0, false, fp);
	    fputc('\n', fp);
	 }
      }
   }
   fputc('\n', fp);
}


static void display_item
(
   sdt_tables *tables,
   int	       prod,
   int	       dot,
   int	       index,
   int	       indexwidth,
   int	       itemno,
   int	       itemwidth,
   int	       stepswidth,
   int	       insertwidth,
   int	       semnowidth,
   int	       symbolwidth,
   FILE	      *fp
)
{
/* Display one item in the canonical collection of LR items */

   int size;
   int i;

   if (indexwidth > 0)
      fprintf(fp, "%*d.  ", indexwidth, index);
   else
      if (indexwidth < 0)
	 fprintf(fp, "%*s   ", -indexwidth, " ");
   if (itemwidth > 0)
      fprintf(fp, "%*d.  ", itemwidth, itemno);
   if (stepswidth > 0 && insertwidth > 0)
      fprintf(fp, "%*d %*d ", stepswidth, PRODUCTION(prod).steps, insertwidth, PRODUCTION(prod).insert);
   if (semnowidth > 0)
      fprintf(fp, "%*d  ", semnowidth, PRODUCTION(prod).semantic);
   display_symbol(PRODUCTION(prod).lhside, fp);
   if (symbolwidth > 0 && (size = strlen(PRODUCTION(prod).lhside->symbol)) < symbolwidth)
      fprintf(fp, "%*s", symbolwidth - size, " ");
   fputs(" -->", fp);
   for (i = 0; i < RHSCOUNT(prod); i++)
   {
      if (i == dot)
	 fputs(" .", fp);
      fputc(' ', fp);
      display_symbol(RHSIDE(prod, i), fp);
   }
   if (dot >= RHSCOUNT(prod))
      fputs(" .", fp);
}


static void display_productions
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display the grammar productions after they have been converted to standard format */

   int width1;
   int width2;
   int width3;
   int width4;
   int width5;
   int i, j;

/* Determine the maximum size of various fields so they can be lined up */

   width1 = digit_count(PRODCOUNT - 1);
   if (width1 < 3)
      width1 = 3;

/* If error repair has been selected find maximum steps and insertion cost */

   if (tables->options & ERRORREPAIR)
   {
      for (width2 = width3 = 0, i = 1; i < PRODCOUNT; i++)
      {
         if (PRODUCTION(i).steps > width2)
	    width2 = PRODUCTION(i).steps;
	 if (PRODUCTION(i).insert > width3)
	    width3 = PRODUCTION(i).insert;
      }
      width2 = digit_count(width2);
      if (width2 < 5)
         width2 = 5;
      width3 = digit_count(width3);
      if (width3 < 6)
         width3 = 6;
   }
   else
   {
      width2 = 0;
      width3 = 0;
   }

/* Find largest semantic routine number and longest LHS symbol */

   for (width4 = width5 = 0, i = 1; i < PRODCOUNT; i++)
   {
      if (PRODUCTION(i).semantic > width4)
	 width4 = PRODUCTION(i).semantic;
      if ((j = strlen(PRODUCTION(i).lhside->symbol)) > width5)
         width5 = j;
   }
   width4 = digit_count(width4);
   if (width4 < 5)
      width4 = 5;

   fprintf(fp, "%s\t%s\tStandardized Grammar Productions\n", tables->name, tables->title);
   if (tables->options & ERRORREPAIR)
      fprintf(fp, "%*s.  %*s %*s %*s  Production\n", width1, "Num", width2, "Steps", width3, "Insert", width4, "Semno");
   else
      fprintf(fp, "%*s.  %*s  Production\n", width1, "Num", width4, "Semno");
   for (i = 1; i < PRODCOUNT; i++)
   {
      display_item(tables, i, -1, i, width1, 0, 0, width2, width3, width4, width5, fp);
      fputc('\n', fp);
   }
   fputc('\n', fp);
}


static void display_repair
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display the error repair values for each parser state */

   int width;
   int i;

   width = digit_count(COLLCOUNT);
   if (width < 5)
      width = 5;

   fprintf(fp, "%s\t%s\tError Repair Values\n", tables->name, tables->title);
   fprintf(fp, "%*s.  Action\n", width, "State");
   for (i = 1; i < COLLCOUNT; i++)
   {
      fprintf(fp, "%*d.  ", width, i);
      if (tables->errortoken[i] > 0)
      {
         fputs("Shift or shiftreduce ", fp);
	 display_token(tables, tables->errortoken[i], fp);
      }
      else
	 if (tables->errortoken[i] < 0)
	    fprintf(fp, "Reduce by production %d", -tables->errortoken[i]);
	 else
	    fputs("Error", fp);
      fputc('\n', fp);
   }
   fputc('\n', fp);
}


static void display_table
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Display the parsing tables as a matrix */

   int width1;
   int half1;
   int width2;
   int size;
   int maxline;
   int i, j, k;

/* Determine the maximum size of various fields so they can be lined up */

   for (width1 = 0, i = 1; i <= tables->termcount; i++)
      if ((size = strlen(tables->termtable[i]->symbol)) > width1)
	 width1 = size;
   for (i = 1; i <= tables->nontermcount; i++)
      if ((size = strlen(tables->nontermtable[i]->symbol)) > width1)
	 width1 = size;
   if (width1 < 5 + 1 + 5)
      width1 = 5 + 1 + 5;
   half1 = width1 / 2;
   for (width2 = digit_count(COLLCOUNT), i = 1; i < COLLCOUNT; i++)
      for (j = 1; j <= tables->termcount + tables->nontermcount; j++)
      {
         if (tables->lrstates[i][j] > SHIFT_OFFSET)
	    size = 1 + digit_count(tables->lrstates[i][j] - SHIFT_OFFSET);
	 else
	    if (tables->lrstates[i][j] > 0)
	       size = 2 + digit_count(tables->lrstates[i][j]);
	    else
	       if (tables->lrstates[i][j] < 0)
		  size = 1 + digit_count(-tables->lrstates[i][j]);
	       else
		  size = 1;
         if (size > width2)
	    width2 = size;
      }

/* Calculate the maximum number of states per line */

   maxline = (PARSE_TABLE_WIDTH - width1 - 2 - 1) / (width2 + 1);

   fprintf(fp, "%s\t%s\tLR Parsing Tables\n", tables->name, tables->title);
   i = 1;
   while (i < COLLCOUNT)
   {
      fprintf(fp, "%-*s/%*s ", width1 - half1, "Token", half1 + 1, "State");
      for (k = i; k < COLLCOUNT; k++)
      {
	 fprintf(fp, " %-*d", width2, k);
	 if (k - i + 1 >= maxline)
	    break;
      }
      fputc('\n', fp);
      for (j = 1; j <= tables->termcount + tables->nontermcount; j++)
      {
	 display_token(tables, j, fp);
	 if (j <= tables->termcount)
	    size = strlen(tables->termtable[j]->symbol);
	 else
	    size = strlen(tables->nontermtable[j - tables->termcount]->symbol);
	 if (size < width1)
	    fprintf(fp, "%*s", width1 - size, " ");
	 fputc(' ', fp);
	 for (k = i; k < COLLCOUNT; k++)
	 {
	    if (tables->lrstates[k][j] > SHIFT_OFFSET)
	       fprintf(fp, " S%-*d", width2 - 1, tables->lrstates[k][j] - SHIFT_OFFSET);
	    else
	       if (tables->lrstates[k][j] > 0)
		  fprintf(fp, " SR%-*d", width2 - 2, tables->lrstates[k][j]);
	       else
		  if (tables->lrstates[k][j] <= ACCEPT_OFFSET)
		     fprintf(fp, " A%*s", width2 - 1, " ");
		  else
		     if (tables->lrstates[k][j] < 0)
			fprintf(fp, " R%-*d", width2 - 1, -tables->lrstates[k][j]);
		     else
			fprintf(fp, " %-*s", width2, ".");
	    if (k - i + 1 >= maxline)
	       break;
	 }
	 fputc('\n', fp);
      }
      fputc('\n', fp);

      i = k + 1;
   }
}


static void find_conflict
(
   sdt_tables *tables,
   int	       state,
   collision  *conflict
)
{
   intset    matches;
   symbolset intersect;
   int	     i, j;

/* Find all reduces whose lookaheads intersect */

   intset_alloc(&matches, INITIAL_ITEMSET_SIZE);
   for (i = 0; i < ITEMCOUNT(state); i++)
      if (ITEMSET(state, i).dot >= PRODUCTION(ITEMSET(state, i).prod).length)
	 for (j = i + 1; j < ITEMCOUNT(state); j++)
	    if (ITEMSET(state, j).dot >= PRODUCTION(ITEMSET(state, j).prod).length)
	    {
	       symbolset_intersect(&intersect, &ITEMSET(state, i).lookahead, &ITEMSET(state, j).lookahead);
	       if (SYMCOUNT(intersect))
	       {
		  fprintf(stderr, "Reduce-Reduce conflict in state %d on [", state);
		  display_symbolset(&intersect, stderr);
		  fputs("]\n   ", stderr);
		  display_item(tables, ITEMSET(state, i).prod, ITEMSET(state, i).dot, state, 0, i, 0, 0, 0, 0, 0, stderr);
		  fputs(", [", stderr);
		  display_symbolset(&ITEMSET(state, i).lookahead, stderr);
		  fputs("]\n   ", stderr);
		  display_item(tables, ITEMSET(state, j).prod, ITEMSET(state, i).dot, state, 0, i, 0, 0, 0, 0, 0, stderr);
		  fputs(", [", stderr);
		  display_symbolset(&ITEMSET(state, j).lookahead, stderr);
		  fputs("]\n", stderr);

		  intset_insert(&matches, i);
		  intset_insert(&matches, j);
	       }
	       symbolset_free(&intersect);
	    }

/* Create the initial lanes for the original conflict state */

   if (conflict->lanes = (traceentry *) malloc(INTCOUNT(matches) * sizeof(*conflict->lanes)))
   {
      for (i = 0; i < INTCOUNT(matches); i++)
      {
	 conflict->lanes[i].complete = false;
	 dynalloc(&conflict->lanes[i].lane, sizeof(laneentry), INITIAL_LANE_SIZE);
	 DYNARRAY(laneentry, conflict->lanes[i].lane, 0).state = state;
	 intset_alloc(&DYNARRAY(laneentry, conflict->lanes[i].lane, 0).items, INITIAL_ITEMSET_SIZE);
	 intset_insert(&DYNARRAY(laneentry, conflict->lanes[i].lane, 0).items, INTSET(matches, i));
	 DYNCOUNT(conflict->lanes[i].lane) = 1;
	 symbolset_copy(&conflict->lanes[i].follow, &ITEMSET(state, INTSET(matches, i)).follow);
      }
      conflict->count   = INTCOUNT(matches);
      conflict->success = false;
   }
   else
      out_of_memory();
   intset_free(&matches);
}


static symbolentry *find_marker
(
   symbolset *set,
   int	      token
)
{
   int i;

   for (i = 0; i < SYMCOUNT(*set); i++)
      if (SYMBOLSET(*set, i)->value.value.token == token)
	 return(SYMBOLSET(*set, i));
   return(NULL);
}


static int find_update
(
   sdt_tables *tables,
   int	       state,
   int	       index,
   target     *match
)
{
   int i;

   for (i = 0; i < UPDCOUNT(state, index); i++)
      if (UPDATE(state, index, i).state == match->state && UPDATE(state, index, i).item == match->item)
	 return(i);
   return(-1);
}


void free_lalrgen
(
   sdt_tables *tables
)
{
   int i, j;

/* Free the grammar productions */

   for (i = 1; i < PRODCOUNT; i++)
      dynfree(&PRODUCTION(i).rhside);
   dynfree(&tables->productions);
   free(tables->lhsindex);

/* Free the canonical collection of LR(0) items */

   for (i = 1; i < COLLCOUNT; i++)
   {
      for (j = 0; j < ITEMCOUNT(i); j++)
      {
	 symbolset_free(&ITEMSET(i, j).follow);
	 symbolset_free(&ITEMSET(i, j).lookahead);
	 if (j < COLLECTION(i).kernel)
	 {
	    dynfree(&ITEMSET(i, j).ancestors);
	    dynfree(&ITEMSET(i, j).update);
	 }
      }
      dynfree(&COLLECTION(i).itemset);
      dynfree(&COLLECTION(i).gotos);
   }
   dynfree(&tables->collection);

/* Free first sets */

   if (tables->first)
      for (i = 1; i <= tables->termcount + tables->nontermcount; i++)
	 symbolset_free(&tables->first[i].symbols);
   free(tables->first);

/* Free error repair table */

   free(tables->errortoken);

/* Free parsing table */

   free(tables->lrstates);
}


void generate_parser
(
   sdt_tables *tables
)
{
   int token;
   int count;
   int found;
   int i, j;

/* Display abstract syntax tree as tree and as productions if desired */

   if (tables->debug & DEBUG_P)
      display_syntax(tables, tables->parser, "Parser Syntax Tree", stdout);
   if (tables->debug & DEBUG_G)
      display_grammar(tables, stdout);

/* Convert parser tree into a standardized list of productions */

   build_productions(tables);
   free_tree(tables->parser);
   tables->parser = NULL;

/* If error repair has been requested, reorder the productions */

   if (tables->options & ERRORREPAIR)
   {
      compute_sortkeys(tables);
      sort_productions(tables);
   }

/* Now that the productions have been finalized display them and their cross-reference */

   if (tables->display & DISPLAY_G)
      display_productions(tables, stdout);
   if (tables->display & DISPLAY_X)
      display_crossref(tables, stdout);

/* Create state 1 of the canonical collection */

   dyncheck(&tables->collection, COLLSIZE * 2);
   dynalloc(&COLLECTION(COLLCOUNT).itemset, sizeof(itementry), INITIAL_ITEMSET_SIZE);
   dynalloc(&COLLECTION(COLLCOUNT).gotos, sizeof(gotoentry), INITIAL_GOTO_SIZE);

/* State 1 has 1 kernel item - the augmented production with the dot on the left */

   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).prod              = 1;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).dot               = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).descendant.state  = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).descendant.item   = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).ancestors.element = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).ancestors.count   = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).ancestors.size    = 0;
   ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).ancestors.array   = NULL;
   symbolset_alloc(&ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).follow, INITIAL_FOLLOW_SIZE);
   dynalloc(&ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).update, sizeof(target), INITIAL_UPDATE_SIZE);
   symbolset_alloc(&ITEMSET(COLLCOUNT, ITEMCOUNT(COLLCOUNT)).lookahead, INITIAL_FOLLOW_SIZE);
   ITEMCOUNT(COLLCOUNT)++;
   COLLECTION(COLLCOUNT).kernel = 1;
   apply_closure(tables, COLLCOUNT, 0);
   COLLCOUNT++;

/* Create the goto states for every itemset in the collection */

   for (i = 1; i < COLLCOUNT; i++)
      for (token = 1; token <= tables->termcount + tables->nontermcount; token++)
      {
/*	 Count how many times this token appears after a dot */

	 for (count = j = 0; j < ITEMCOUNT(i); j++)
	    if (ITEMSET(i, j).dot < PRODUCTION(ITEMSET(i, j).prod).length && RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->value.value.token == token)
	    {
	       found = j;
	       count++;
	    }

/*	 If the dot is in front of the final token in the production and */
/*	 this is the only dot in front of that token, then we can reduce */
/*	 the size of the collection by using a default shiftreduce	 */

	 if (tables->options & DEFAULTREDUCE && count == 1 && ITEMSET(i, found).dot == PRODUCTION(ITEMSET(i, found).prod).length - 1)
	    continue;
	 else
	    if (count)
	    {
	       dyncheck(&COLLECTION(i).gotos, GOTOSIZE(i) * 2);
	       GOTO(i, GOTOCOUNT(i)).token = RHSIDE(ITEMSET(i, found).prod, ITEMSET(i, found).dot)->value.value.token;
	       GOTO(i, GOTOCOUNT(i)).state = lookup_goto(tables, i, token);
	       GOTOCOUNT(i)++;
	    }
      }

/* Compute first sets for every token */

   compute_first(tables);

/* Display the nonterminal first sets if requested */

   if (tables->debug & DEBUG_F)
      display_first(tables, stdout);

/* Initialize spontanenous follow set and determine where lookaheads propagate */

   setup_lookahead(tables);
   propagate_lookahead(tables);

/* Generate encoded parsing tables */

   build_table(tables);

/* We've built the canonical collection of LR(0) items, added the */
/* LALR(1) lookahead sets to it, and potentially split states to  */
/* repair reduce-reduce conflicts so we can display the CFSM and  */
/* ancestor states now						  */

   if (tables->debug & DEBUG_I)
      display_collection(tables, stdout);
   if (tables->debug & DEBUG_A)
      display_ancestors(tables, stdout);

/* Generate error repair tokens if requested */

   build_repair(tables);

/* And last but not least display the parsing tables if requested */

   if (tables->display & DISPLAY_T)
      display_table(tables, stdout);
}


static void group_conflicts
(
   sdt_tables *tables,
   dynarray   *conflict,
   dynarray   *groups
)
{
   int	       count;
   symbolset **lookahead;
   symbolset  *index;
   collision  *repair;
   int	       length;
   int	       state;
   intset      merge;
   symbolset  *combine;
   bool	       changed;
   symbolset   intersect;
   bool	       failure;
   int	       i, j, k, l;

/* Put each conflict into its own group */

   dynalloc(groups, sizeof(intset), DYNCOUNT(*conflict));
   for (i = 0; i < DYNCOUNT(*conflict); i++)
   {
      intset_alloc(&DYNARRAY(intset, *groups, i), INITIAL_CONFLICT_SIZE);
      intset_insert(&DYNARRAY(intset, *groups, i), i);
   }
   DYNCOUNT(*groups) = DYNCOUNT(*conflict);

/* All conflicts have the same number of lanes */

   count = DYNARRAY(collision, *conflict, 0).count;

/* Create the lookahead sets for each lane in each group */

   if (lookahead = (symbolset **) malloc(DYNCOUNT(*conflict) * (sizeof(*lookahead) + count * sizeof(**lookahead))))
   {
      for (index = (symbolset *) &lookahead[DYNCOUNT(*conflict)], i = 0; i < DYNCOUNT(*conflict); i++)
      {
	 lookahead[i] = index;
	 index       += count;

	 repair = &DYNARRAY(collision, *conflict, i);
	 for (j = 0; j < count; j++)
	 {
	    symbolset_copy(&lookahead[i][j], &repair->lanes[j].follow);
	    if (!repair->lanes[j].complete)
	    {
	       length = DYNCOUNT(repair->lanes[j].lane);
	       state  = DYNARRAY(laneentry, repair->lanes[j].lane, length - 1).state;
	       for (k = 0; k < INTCOUNT(DYNARRAY(laneentry, repair->lanes[j].lane, length - 1).items); k++)
	       {
		  symbolset_union(&merge, &lookahead[i][j], &ITEMSET(state, INTSET(DYNARRAY(laneentry, repair->lanes[j].lane, length - 1).items, k)).lookahead);
		  symbolset_free(&lookahead[i][j]);
		  lookahead[i][j] = merge;
	       }
	    }
	 }
      }
   }
   else
      out_of_memory();

/* Merge any groups whose lookaheads are compatible */

   if (combine = (symbolset *) malloc(count * sizeof(*combine)))
      do
	 for (changed = false, i = 0; i < DYNCOUNT(*groups); i++)
	    for (j = i + 1; j < DYNCOUNT(*groups); j++)
	    {
/*	       Simulate merging the group i and group j lookaheads for each lane */

	       for (k = 0; k < count; k++)
		  symbolset_union(&combine[k], &lookahead[i][k], &lookahead[j][k]);

/*	       Check if that merge created a conflict */

	       for (failure = false, k = 0; !failure && k < count; k++)
		  for (l = k + 1; !failure && l < count; l++)
		  {
		     symbolset_intersect(&intersect, &combine[k], &combine[l]);
		     if (SYMCOUNT(intersect))
			failure = true;
		     symbolset_free(&intersect);
		  }

	       if (!failure)
	       {
/*		  Two conflicts with compatible lookaheads can be merged */

		  intset_union(&merge, &DYNARRAY(intset, *groups, i), &DYNARRAY(intset, *groups, j));
		  intset_free(&DYNARRAY(intset, *groups, i));
		  DYNARRAY(intset, *groups, i) = merge;

/*		  Save the merged lookaheads for the new group */

		  for (k = 0; k < count; k++)
		  {
		     symbolset_free(&lookahead[i][k]);
		     lookahead[i][k] = combine[k];
		  }

/*		  Remove group j and its lookaheads from the list */

		  intset_free(&DYNARRAY(intset, *groups, j));
		  for (k = 0; k < count; k++)
		     symbolset_free(&lookahead[j][k]);
		  for (k = j + 1; k < DYNCOUNT(*groups); k++)
		  {
		     DYNARRAY(intset, *groups, k - 1) = DYNARRAY(intset, *groups, k);
		     for (l = 0; l < count; l++)
			lookahead[k - 1][l] = lookahead[k][l];
		  }
		  DYNCOUNT(*groups)--;

		  changed = true;
	       }
	       else

/*		  Free the failed lookahead simulation */

		  for (k = 0; k < count; k++)
		     symbolset_free(&combine[k]);
	    }
      while (changed);
   else
      out_of_memory();

   free(combine);
   for (i = 0; i < DYNCOUNT(*groups); i++)
      for (j = 0; j < count; j++)
	 symbolset_free(&lookahead[i][j]);
   free(lookahead);
}


void init_lalrgen
(
   sdt_tables *tables
)
{
   int i;

/* Allocate production list and start with production 1 */

   dynalloc(&tables->productions, sizeof(production), INITIAL_PRODUCTION_SIZE);
   PRODCOUNT = 1;

   tables->lhsindex = NULL;

/* Allocate characteristic finite state machine and start with state 1 */

   dynalloc(&tables->collection, sizeof(configuration), INITIAL_COLLECTION_SIZE);
   COLLCOUNT = 1;

/* Initialize terminal and nonterminal first sets */

   if (tables->first = (firstset *) malloc((tables->termcount + tables->nontermcount + 1) * sizeof(*tables->first)))
      for (i = 1; i <= tables->termcount + tables->nontermcount; i++)
      {
	 symbolset_alloc(&tables->first[i].symbols, INITIAL_FOLLOW_SIZE);
	 tables->first[i].nullable = false;
      }
   else
      out_of_memory();

   tables->errortoken = NULL;
   tables->lrstates   = NULL;
}


static void insert_production
(
   sdt_tables  *tables,
   symbolentry *lhside,
   treenode    *rhslist
)
{
   treenode *tree;
   treenode *node;

/* If this is the first time we've seen this LHS add it to the index */

   if (tables->lhsindex[lhside->value.value.token - tables->termcount] == 0)
      tables->lhsindex[lhside->value.value.token - tables->termcount] = PRODCOUNT;

/* Append a production for each alternative right hand side */

   for (tree = rhslist; tree; tree = tree->node.next)
   {
      dyncheck(&tables->productions, PRODSIZE * 2);
      PRODUCTION(PRODCOUNT).lhside   = lhside;
      dynalloc(&PRODUCTION(PRODCOUNT).rhside, sizeof(symbolentry *), INITIAL_RHS_LENGTH);
      PRODUCTION(PRODCOUNT).length   = 0;
      PRODUCTION(PRODCOUNT).semantic = 0;
      PRODUCTION(PRODCOUNT).steps    = INT_MAX;
      PRODUCTION(PRODCOUNT).insert   = INT_MAX;

      if (tree->node.count != LEAF && tree->node.type == '.')
      {
/*	 This right hand side consists of more than one symbol */

	 for (node = tree->node.entry[0]; node; node = node->node.next)
	    if (node->node.count == LEAF)
	       switch (node->leaf.type)
	       {
		  case REFERENCE:

/*		     Append the new symbol to the right hand side */

		     dyncheck(&PRODUCTION(PRODCOUNT).rhside, RHSSIZE(PRODCOUNT) * 2);
		     RHSIDE(PRODCOUNT, RHSCOUNT(PRODCOUNT)++) = node->leaf.value.symbol;

/*		     Save the length of the RHS excluding trailing epsilon terminals */

		     if (node->leaf.value.symbol->type != TERMINAL || (node->leaf.value.symbol->value.value.flags & EMPTY) != EMPTY)
			PRODUCTION(PRODCOUNT).length = RHSCOUNT(PRODCOUNT);
		     break;

		  case SEMANTIC:

/*		     Set the semantic routine number for this production */

		     PRODUCTION(PRODCOUNT).semantic = node->leaf.value.number;
	       }
      }
      else
	 if (tree->node.count == LEAF && tree->leaf.type == REFERENCE)
	 {
/*	    This right hand side consists of only a single symbol */

	    dyncheck(&PRODUCTION(PRODCOUNT).rhside, RHSSIZE(PRODCOUNT) * 2);
	    RHSIDE(PRODCOUNT, RHSCOUNT(PRODCOUNT)++) = tree->leaf.value.symbol;
	    if (tree->leaf.value.symbol->type != TERMINAL || (tree->leaf.value.symbol->value.value.flags & EMPTY) != EMPTY)
	       PRODUCTION(PRODCOUNT).length = RHSCOUNT(PRODCOUNT);
	 }
      PRODCOUNT++;
   }
}


static bool itemset_equal
(
   sdt_tables *tables,
   dynarray   *itemset1,
   int	       kernel1,
   dynarray   *itemset2,
   int	       kernel2
)
{
/* Check if two item sets in CFSM are identical */

   int i, j;

   if (kernel1 != kernel2)
      return(false);

   if (tables->options & ERRORREPAIR)

/*    For error repair, all the items must be equal and in the same order */

      for (i = 0; i < kernel1; i++)
      {
	 if (DYNARRAY(itementry, *itemset1, i).prod != DYNARRAY(itementry, *itemset2, i).prod ||
	     DYNARRAY(itementry, *itemset1, i).dot  != DYNARRAY(itementry, *itemset2, i).dot)
	    return(false);
      }
   else

/*    Check that all the items in one set are also in the other */

      for (i = 0; i < kernel1; i++)
      {
	 for (j = 0; j < kernel2; j++)
	    if (DYNARRAY(itementry, *itemset1, j).prod == DYNARRAY(itementry, *itemset2, i).prod &&
		DYNARRAY(itementry, *itemset1, j).dot  == DYNARRAY(itementry, *itemset2, i).dot)
	       break;
	 if (j >= kernel2)

   /*    kernel2 is missing an item which is in kernel1 */

	    return(false);
      }
   return(true);
}


static void kernel_items
(
   sdt_tables *tables,
   collision  *conflict
)
{
/* Ensure than all the lanes in the conflict end with the kernel items in their state with derived them */

   int	  length;
   intset kernel;
   int	  state;
   int	  item;
   int	  i, j, k, l;

   for (i = 0; i < conflict->count; i++)
      if (!conflict->lanes[i].complete)
      {
/*	 Get the state at the end of lane i in this conflict */

	 length = DYNCOUNT(conflict->lanes[i].lane);
	 state  = DYNARRAY(laneentry, conflict->lanes[i].lane, length - 1).state;

	 intset_alloc(&kernel, INITIAL_UPDATE_SIZE);
	 for (j = 0; j < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[i].lane, length - 1).items); j++)
	    if ((item = INTSET(DYNARRAY(laneentry, conflict->lanes[i].lane, length - 1).items, j)) < COLLECTION(state).kernel)

/*	       If the item is already a kernel item just insert it into the set */

	       intset_insert(&kernel, item);
	    else

/*	       Otherwise insert all the kernel items which propagate to this item */

	       for (k = 0; k < COLLECTION(state).kernel; k++)
		  for (l = 0; l < UPDCOUNT(state, k); l++)
		     if (UPDATE(state, k, l).state == state && UPDATE(state, k, l).item == item)
		     {
		        intset_insert(&kernel, k);
			break;
		     }

	 if (!INTCOUNT(kernel))
	 {
/*	    If no kernel items propagate to this lane it is complete */

	    conflict->lanes[i].complete = true;
	    intset_free(&kernel);
	 }
	 else
	    if (!intset_equal(&kernel, &DYNARRAY(laneentry, conflict->lanes[i].lane, length - 1).items))
	    {
/*	       If there are new kernel entries to search back from add them to the lane */

	       dyncheck(&conflict->lanes[i].lane, DYNSIZE(conflict->lanes[i].lane) * 2);
	       DYNARRAY(laneentry, conflict->lanes[i].lane, length).state = state;
	       DYNARRAY(laneentry, conflict->lanes[i].lane, length).items = kernel;
	       DYNCOUNT(conflict->lanes[i].lane)++;
	    }
	    else
	       intset_free(&kernel);
      }
}


static void list_gotos
(
   sdt_tables *tables,
   int         index,
   int         width1,
   FILE       *fp
)
{
/* Display all the GOTO entries for a state */

   int i;

   for (i = 0; i < GOTOCOUNT(index); i++)
   {
      fprintf(fp, "%*s   Goto state %d on ", width1, " ", GOTO(index, i).state);
      display_token(tables, GOTO(index, i).token, fp);
      fputc('\n', fp);
   }
}


static void list_items
(
   sdt_tables *tables,
   int	       index,
   int	       width1,
   FILE	      *fp
)
{
/* Display the items in a parser state with their lookahead sets */

   int width2;
   int width3;
   int size;
   int i;
#ifdef DEBUG
   int j;
#endif

/* Determine length of longest left hand side in itemset */

   width2 = digit_count(ITEMCOUNT(index) - 1);
   for (width3 = i = 0; i < ITEMCOUNT(index); i++)
      if ((size = strlen(PRODUCTION(ITEMSET(index, i).prod).lhside->symbol)) > width3)
	 width3 = size;

   for (i = 0; i < ITEMCOUNT(index); i++)
   {
      if (i == 0)
	 display_item(tables, ITEMSET(index, i).prod, ITEMSET(index, i).dot, index, width1, i, width2, 0, 0, 0, width3, fp);
      else
	 display_item(tables, ITEMSET(index, i).prod, ITEMSET(index, i).dot, index, -width1, i, width2, 0, 0, 0, width3, fp);
      if (SYMCOUNT(ITEMSET(index, i).lookahead))
      {
	 fputs(", [", fp);
	 display_symbolset(&ITEMSET(index, i).lookahead, fp);
	 fputc(']', fp);
      }
      fputc('\n', fp);
#ifdef DEBUG
      if (ITEMSET(index, i).descendant.state)
	 fprintf(fp, "%*s       Descendant %d,%d\n", width1, " ", ITEMSET(index, i).descendant.state, ITEMSET(index, i).descendant.item);
      if (i < COLLECTION(index).kernel && ANCCOUNT(index, i))
      {
	 fprintf(fp, "%*s       Ancestors ", width1, " ");
	 for (j = 0; j < ANCCOUNT(index, i); j++)
	 {
	    fprintf(fp, "%d,%d", ANCESTOR(index, i, j).state, ANCESTOR(index, i, j).item);
	    if (j < ANCCOUNT(index, i) - 1)
	       fputc(' ', fp);
	 }
	 fputc('\n', fp);
      }
      if (SYMCOUNT(ITEMSET(index, i).follow))
      {
	 fprintf(fp, "%*s       Follow [", width1, " ");
	 display_symbolset(&ITEMSET(index, i).follow, fp);
	 fputs("]\n", fp);
      }
      if (i < COLLECTION(index).kernel && UPDCOUNT(index, i))
      {
	 fprintf(fp, "%*s       Update ", width1, " ");
	 for (j = 0; j < UPDCOUNT(index, i); j++)
	 {
	    fprintf(fp, "%d,%d", UPDATE(index, i, j).state, UPDATE(index, i, j).item);
	    if (j < UPDCOUNT(index, i) - 1)
	       fputc(' ', fp);
	 }
	 fputc('\n', fp);
      }
#endif
      if (i == COLLECTION(index).kernel - 1 && ITEMCOUNT(index) > COLLECTION(index).kernel)
	 fprintf(fp, "%*s   ---\n", width1, " ");
   }
}


static int lookup_goto
(
   sdt_tables *tables,
   int	       state,
   int	       token
)
{
   dynarray kernel;
   int	    prod;
   int	    dot;
   int	    i, j, k;

/* Create local copy of the goto kernel items */

   dynalloc(&kernel, sizeof(itementry), INITIAL_ITEMSET_SIZE);
   for (i = 0; i < ITEMCOUNT(state); i++)
   {
      prod = ITEMSET(state, i).prod;
      dot  = ITEMSET(state, i).dot;

      if (dot < PRODUCTION(prod).length && RHSIDE(prod, dot)->value.value.token == token)
      {
	 dyncheck(&kernel, DYNSIZE(kernel) * 2);
	 DYNARRAY(itementry, kernel, DYNCOUNT(kernel)).prod = prod;

/*	 Skip over epsilon terminals */

	 for (dot = dot + 1; dot < RHSCOUNT(prod); dot++)
	    if (RHSIDE(prod, dot)->type != TERMINAL || (RHSIDE(prod, dot)->value.value.flags & EMPTY) != EMPTY)
	       break;
	 DYNARRAY(itementry, kernel, DYNCOUNT(kernel)).dot  = dot;
	 DYNCOUNT(kernel)++;
      }
   }

/* Check to see if this goto state has already been defined */

   for (i = 2; i < COLLCOUNT && !itemset_equal(tables, &COLLECTION(i).itemset, COLLECTION(i).kernel, &kernel, DYNCOUNT(kernel)); i++)
      ;
   if (i >= COLLCOUNT)
   {
/*    Allocate a new GOTO state */

      for (i = 0; i < DYNCOUNT(kernel); i++)
      {
	 DYNARRAY(itementry, kernel, i).descendant.state = 0;
	 DYNARRAY(itementry, kernel, i).descendant.item  = 0;
	 dynalloc(&DYNARRAY(itementry, kernel, i).ancestors, sizeof(target), INITIAL_ANCESTOR_SIZE);
	 symbolset_alloc(&DYNARRAY(itementry, kernel, i).follow, INITIAL_FOLLOW_SIZE);
	 dynalloc(&DYNARRAY(itementry, kernel, i).update, sizeof(target), INITIAL_UPDATE_SIZE);
	 symbolset_alloc(&DYNARRAY(itementry, kernel, i).lookahead, INITIAL_FOLLOW_SIZE);
      }

/*    Save the local GOTO kernel in the new GOTO state and close it */

      dyncheck(&tables->collection, COLLSIZE * 2);
      COLLECTION(COLLCOUNT).itemset = kernel;
      COLLECTION(COLLCOUNT).kernel  = ITEMCOUNT(COLLCOUNT);
      dynalloc(&COLLECTION(COLLCOUNT).gotos, sizeof(gotoentry), INITIAL_GOTO_SIZE);
      apply_closure(tables, COLLCOUNT, 0);

/*    Return the newly allocated state */

      i = COLLCOUNT++;
   }
   else

/*    Free the unneeded kernel and return the matching state */

      dynfree(&kernel);

/* Set up descendant and ancestor values */

   for (k = j = 0; j < ITEMCOUNT(state); j++)
   {
      prod = ITEMSET(state, j).prod;
      dot  = ITEMSET(state, j).dot;

      if (dot < PRODUCTION(prod).length && RHSIDE(prod, dot)->value.value.token == token)
      {
         ITEMSET(state, j).descendant.state = i;
	 ITEMSET(state, j).descendant.item  = k;

	 dyncheck(&ITEMSET(i, k).ancestors, ANCSIZE(i, k) * 2);
	 ANCESTOR(i, k, ANCCOUNT(i, k)).state = state;
	 ANCESTOR(i, k, ANCCOUNT(i, k)).item  = j;
	 ANCCOUNT(i, k)++;
	 k++;
      }
   }
   return(i);
}


static int map_state
(
   dynarray *map,
   int	     state
)
{
/* Find state in the old to new state map and return its new state number */

   int i;

   for (i = 0; i < DYNCOUNT(*map); i++)
      if (DYNARRAY(statemap, *map, i).old == state)
	 return(DYNARRAY(statemap, *map, i).new);
   return(state);
}


static void previous_states
(
   sdt_tables *tables,
   dynarray   *conflict
)
{
/* Duplicate conflicts for each ancestor state of the state at the top of its lanes */

   collision *src;
   int	      count;
   int	      lane;
   int	      length;
   int	      state;
   int	      item;
   collision *dst;
   symbolset  merge;
   int	      i, j, k, l;

   for (i = 0; i < DYNCOUNT(*conflict); i++)
      if (!DYNARRAY(collision, *conflict, i).success)
      {
	 src = &DYNARRAY(collision, *conflict, i);

/*	 Find how many ancestor states this conflict has.  Since every */
/*	 kernel item has the same number of ancestors any one will do  */

	 for (count = j = 0; j < src->count; j++)
	    if (!src->lanes[j].complete)
	    {
	       lane   = j;
	       length = DYNCOUNT(src->lanes[j].lane);
	       state  = DYNARRAY(laneentry, src->lanes[j].lane, length - 1).state;
	       item   = INTSET(DYNARRAY(laneentry, src->lanes[j].lane, length - 1).items, 0);
	       count  = ANCCOUNT(state, item);
	       break;
	    }

/*	 If this conflict has no ancestors ensure all the lanes are complete */

	 if (!count)
	 {
	    for (j = 0; j < src->count; j++)
	       src->lanes[j].complete = true;
	    continue;
	 }

/*	 If there is more than one ancestor, create a duplicate conflict for each */

	 if (count > 1)
	 {
/*	    Make room to insert the new conflicts after the current */

	    while (DYNCOUNT(*conflict) + count - 1 > DYNSIZE(*conflict))
	       dynresize(conflict, DYNSIZE(*conflict) * 2);
	    if (DYNCOUNT(*conflict) - i - 1 > 0)
	       memmove(&DYNARRAY(collision, *conflict, i + count), &DYNARRAY(collision, *conflict, i + 1), (DYNCOUNT(*conflict) - i - 1) * DYNELEMENT(*conflict));
	    DYNCOUNT(*conflict) += count - 1;

/*	    Since dynresize may have moved *conflict we have to restore the pointer */

	    src = &DYNARRAY(collision, *conflict, i);

/*	    Copy the current conflict into each of the newly created entries */

	    for (j = 1; j < count; j++)
	    {
	       dst = &DYNARRAY(collision, *conflict, i + j);
	       if (dst->lanes = (traceentry *) malloc(src->count * sizeof(*src->lanes)))
	       {
		  for (k = 0; k < src->count; k++)
		  {
		     dst->lanes[k].complete = src->lanes[k].complete;
		     dynalloc(&dst->lanes[k].lane, sizeof(laneentry), DYNSIZE(src->lanes[k].lane));
		     for (l = 0; l < DYNCOUNT(src->lanes[k].lane); l++)
		     {
			DYNARRAY(laneentry, dst->lanes[k].lane, l).state = DYNARRAY(laneentry, src->lanes[k].lane, l).state;
			intset_copy(&DYNARRAY(laneentry, dst->lanes[k].lane, l).items, &DYNARRAY(laneentry, src->lanes[k].lane, l).items);
		     }
		     DYNCOUNT(dst->lanes[k].lane) = DYNCOUNT(src->lanes[k].lane);
		     symbolset_copy(&dst->lanes[k].follow, &src->lanes[k].follow);
		  }
		  dst->count   = src->count;
		  dst->success = src->success;
	       }
	       else
		  out_of_memory();
	    }
	 }

/*	 Append the corresponding ancestor to each lane of each conflict */

	 for (j = 0; j < src->count; j++)
	    if (!src->lanes[j].complete)
	    {
	       length = DYNCOUNT(src->lanes[j].lane);
	       state  = DYNARRAY(laneentry, src->lanes[j].lane, length - 1).state;

	       for (k = 0; k < count; k++)
	       {
		  dst = &DYNARRAY(collision, *conflict, i + k);

		  dyncheck(&dst->lanes[j].lane, DYNSIZE(dst->lanes[j].lane) * 2);
		  intset_alloc(&DYNARRAY(laneentry, dst->lanes[j].lane, length).items, INITIAL_ITEMSET_SIZE);

		  for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, src->lanes[j].lane, length - 1).items); l++)
		  {
		     item = INTSET(DYNARRAY(laneentry, src->lanes[j].lane, length - 1).items, l);

/*		     Add the k'th ancestor to the i + k'th conflict */

		     DYNARRAY(laneentry, dst->lanes[j].lane, length).state = ANCESTOR(state, item, k).state;
		     intset_insert(&DYNARRAY(laneentry, dst->lanes[j].lane, length).items, ANCESTOR(state, item, k).item);

/*		     Merge the k'th ancestor's spontaneous lookahead with the i + k'th conflict */

		     symbolset_union(&merge, &dst->lanes[j].follow, &ITEMSET(ANCESTOR(state, item, k).state, ANCESTOR(state, item, k).item).follow);
		     symbolset_free(&dst->lanes[j].follow);
		     dst->lanes[j].follow = merge;
		  }
		  DYNCOUNT(dst->lanes[j].lane)++;

/*		  If the state we just added occurs earlier in the lane we created a loop */

		  for (l = length - 1; l >= 0 && DYNARRAY(laneentry, dst->lanes[j].lane, l).state != ANCESTOR(state, item, k).state; l--)
		     ;
		  if (l >= 0)
		     dst->lanes[j].complete = true;
	       }
	    }
	 i += count - 1;
      }
}


static void propagate_lookahead
(
   sdt_tables *tables
)
{
   bool	     changed;
   symbolset merge;
   int	     i, j, k;

/* Clear all propagated lookahead sets */

   for (i = 1; i < COLLCOUNT; i++)
      for (j = 0; j < ITEMCOUNT(i); j++)
	 SYMCOUNT(ITEMSET(i, j).lookahead) = 0;

/* Initialize every item's lookahead set to its spontaneous follow set */

   for (i = 1; i < COLLCOUNT; i++)
      for (j = 0; j < ITEMCOUNT(i); j++)
	 if (SYMCOUNT(ITEMSET(i, j).follow))
	 {
	    symbolset_union(&merge, &ITEMSET(i, j).lookahead, &ITEMSET(i, j).follow);
	    symbolset_free(&ITEMSET(i, j).lookahead);
	    ITEMSET(i, j).lookahead = merge;

	    if (ITEMSET(i, j).descendant.state)
	    {
/*	       Propagate spontaneous follow sets to the goto state */

	       symbolset_union(&merge, &ITEMSET(ITEMSET(i, j).descendant.state, ITEMSET(i, j).descendant.item).lookahead, &ITEMSET(i, j).follow);
	       symbolset_free(&ITEMSET(ITEMSET(i, j).descendant.state, ITEMSET(i, j).descendant.item).lookahead);
	       ITEMSET(ITEMSET(i, j).descendant.state, ITEMSET(i, j).descendant.item).lookahead = merge;
	    }
	 }

/* Give state 1 an end of file lookahead */

   symbolset_insert(&ITEMSET(1, 0).lookahead, tables->sentinel);

/* Propagate lookahead sets until there are no more changes */

   do
   {
      changed = false;
      for (i = 1; i < COLLCOUNT; i++)
         for (j = 0; j < COLLECTION(i).kernel; j++)

/*	    Propagate lookaheads to all items that this item updates */

	    for (k = 0; k < UPDCOUNT(i, j); k++)
	    {
	       symbolset_union(&merge, &ITEMSET(UPDATE(i, j, k).state, UPDATE(i, j, k).item).lookahead, &ITEMSET(i, j).lookahead);
	       if (symbolset_equal(&merge, &ITEMSET(UPDATE(i, j, k).state, UPDATE(i, j, k).item).lookahead))
		  symbolset_free(&merge);
	       else
	       {
/*		  If the lookahead has changed update it and record that something has changed */

		  symbolset_free(&ITEMSET(UPDATE(i, j, k).state, UPDATE(i, j, k).item).lookahead);
		  ITEMSET(UPDATE(i, j, k).state, UPDATE(i, j, k).item).lookahead = merge;
		  changed                                                        = true;
	       }
	    }
   }
   while (changed);
}


static void resolve_ambiguity
(
   sdt_tables *tables,
   int	       state
)
{
   symbolset matches;
   bool	     failure;
   int	     reduceprec;
   int	     shiftprec;
   int	     associativity;
   int	     nextprec;
   int	     nextassoc;
   int	     i, j, k;

/* Display the tokens that are in collision in this state */

   for (i = 0; i < ITEMCOUNT(state); i++)
      if (ITEMSET(state, i).dot >= PRODUCTION(ITEMSET(state, i).prod).length)
      {
/*	 Find all the symbols in this reduce lookahead that are in conflict */

	 symbolset_alloc(&matches, SYMCOUNT(ITEMSET(state, i).lookahead));
	 for (j = 0; j < SYMCOUNT(ITEMSET(state, i).lookahead); j++)
	    if (tables->lrstates[state][SYMBOLSET(ITEMSET(state, i).lookahead, j)->value.value.token] > 0)
	       symbolset_insert(&matches, SYMBOLSET(ITEMSET(state, i).lookahead, j));

	 if (SYMCOUNT(matches))
	 {
	    fprintf(stderr, "Shift-Reduce conflict in state %d on [", state);
	    display_symbolset(&matches, stderr);
	    fputs("]\n", stderr);
	    fputs("   Reduce      by ", stderr);
	    display_item(tables, ITEMSET(state, i).prod, ITEMSET(state, i).dot, state, 0, i, 0, 0, 0, 0, 0, stderr);
	    fputs(", [", stderr);
	    display_symbolset(&ITEMSET(state, i).lookahead, stderr);
	    fputs("]\n", stderr);

	    for (j = 0; j < SYMCOUNT(matches); j++)
	       for (k = 0; k < ITEMCOUNT(state); k++)
		  if (ITEMSET(state, k).dot < PRODUCTION(ITEMSET(state, k).prod).length &&
		     RHSIDE(ITEMSET(state, k).prod, ITEMSET(state, k).dot)->value.value.token ==
		     SYMBOLSET(matches, j)->value.value.token)
		  {
		     fprintf(stderr, "   %s by ", (ITEMSET(state, k).descendant.state) ? "Shift      " : "Shiftreduce");
		     display_item(tables, ITEMSET(state, k).prod, ITEMSET(state, k).dot, state, 0, k, 0, 0, 0, 0, 0, stderr);
		     fputc('\n', stderr);
		  }
	 }
	 symbolset_free(&matches);
      }

   if (tables->options & AMBIGUOUS)
   {
/*    All the shift and shiftreduce actions have already been saved in the state */
/*    Use precedence and associativity to decide how to handle the reduces	 */

      for (failure = false, i = 0; i < ITEMCOUNT(state); i++)
	 if (ITEMSET(state, i).dot >= PRODUCTION(ITEMSET(state, i).prod).length)
	 {
/*	    Check if this reduce production collides with any shift */

	    for (j = 0; j < SYMCOUNT(ITEMSET(state, i).lookahead); j++)
	       if (tables->lrstates[state][SYMBOLSET(ITEMSET(state, i).lookahead, j)->value.value.token] > 0)
		  break;
	    if (j < SYMCOUNT(ITEMSET(state, i).lookahead))
	    {
/*	       This reduce has at least one collision so determine its precedence */

	       for (reduceprec = -1, j = 0; j < RHSCOUNT(ITEMSET(state, i).prod); j++)
		  if (RHSIDE(ITEMSET(state, i).prod, j)->type == TERMINAL)
		     reduceprec = RHSIDE(ITEMSET(state, i).prod, j)->value.value.precedence;

	       if (tables->display & DISPLAY_V || reduceprec < 0)
	       {
		  fputs("The reduce by production ", stderr);
		  display_item(tables, ITEMSET(state, i).prod, ITEMSET(state, i).dot, state, 0, i, 0, 0, 0, 0, 0, stderr);
		  if (reduceprec >= 0)
		     fprintf(stderr, " has precedence %d\n", reduceprec);
		  else
		     fputs(" has no precedence\n", stderr);
	       }

	       if (reduceprec >= 0)
	       {
/*		  Check each lookahead symbol for matching shifts */

		  for (j = 0; j < SYMCOUNT(ITEMSET(state, i).lookahead); j++)
		  {
/*		     Check all items that shift on this token to determine precedence and associativity */

		     for (shiftprec = -1, associativity = k = 0; k < ITEMCOUNT(state); k++)
			if (ITEMSET(state, k).dot < PRODUCTION(ITEMSET(state, k).prod).length &&
			   RHSIDE(ITEMSET(state, k).prod, ITEMSET(state, k).dot)->value.value.token ==
			   SYMBOLSET(ITEMSET(state, i).lookahead, j)->value.value.token)
			{
			   nextprec  = RHSIDE(ITEMSET(state, k).prod, ITEMSET(state, k).dot)->value.value.precedence;
			   nextassoc = RHSIDE(ITEMSET(state, k).prod, ITEMSET(state, k).dot)->value.value.flags & ASSOCIATIVITY;

			   if ((tables->display & DISPLAY_V) || shiftprec >= 0 && nextprec != shiftprec || associativity && nextassoc != associativity)
			   {
			      fprintf(stderr, "The %s by production ", (ITEMSET(state, k).descendant.state) ? "shift" : "shiftreduce");
			      display_item(tables, ITEMSET(state, k).prod, ITEMSET(state, k).dot, state, 0, k, 0, 0, 0, 0, 0, stderr);
			      fprintf(stderr, " has precedence %d and associativity = %s\n", nextprec,
				 (nextassoc & LEFT) ? "LEFT" : (nextassoc & RIGHT) ? "RIGHT" : "NONE");
			   }
			   if (shiftprec >= 0 && nextprec != shiftprec)
			      fprintf(stderr, "   Warning: shift precedance %d is not equal to the earlier precedence %d\n", nextprec, shiftprec);
			   if (associativity && nextassoc != associativity)
			      fprintf(stderr, "   Warning: shift associativity = %s is not equal to the earlier associativity = %s\n",
				 (nextassoc & LEFT) ? "LEFT" : (nextassoc & RIGHT) ? "RIGHT" : "NONE",
				 (associativity & LEFT) ? "LEFT" : (associativity & RIGHT) ? "RIGHT" : "NONE");

			   if (shiftprec < 0)
			      shiftprec = nextprec;

			   if (!associativity)
			      associativity = nextassoc;
			}
		     if (reduceprec == shiftprec && associativity == NONE)
			failure = true;

/*		     Find a representative shift and resolve the conflict */

		     set_ambiguity(tables, state, i, SYMBOLSET(ITEMSET(state, i).lookahead, j)->value.value.token, reduceprec, shiftprec, associativity);
		  }
	       }
	       else
		  failure = true;
	    }
	 }
      if (failure)
      {
	 fputs("Shift-Reduce conflict cannot be resolved\n", stderr);
	 tables->process = false;
      }
      else
	 fputs("Shift-Reduce conflict has been resolved\n", stderr);
   }
   else

/*    Prevent tables from being generated */

      tables->process = false;
   fputc('\n', stderr);
}


static int set_action
(
   sdt_tables *tables,
   int	       state,
   int	       token,
   int	       action
)
{
/* Detect action conflict or store encoded action for this state and token */

   if (tables->lrstates[state][token] && tables->lrstates[state][token] != action)
      if (tables->lrstates[state][token] > 0 || action > 0)
	 return(SHIFT_REDUCE_ERROR);
      else
	 return(REDUCE_REDUCE_ERROR);
   else
      tables->lrstates[state][token] = action;
   return(NO_ERROR);
}


static void set_ambiguity
(
   sdt_tables *tables,
   int	       state,
   int	       item,
   int	       token,
   int	       reduceprec,
   int	       shiftprec,
   int	       associativity
)
{
/* Update shift/shiftreduce action to reduce if indicated by precedence or associativity */

   int i;

/* Find a shift/shiftreduce item whose token conflicts with reduce token */

   for (i = 0; i < ITEMCOUNT(state); i++)
      if (ITEMSET(state, i).dot < PRODUCTION(ITEMSET(state, i).prod).length &&
	 RHSIDE(ITEMSET(state, i).prod, ITEMSET(state, i).dot)->value.value.token == token)
      break;

/* Attempt to use precedence and associativity to choose an action */

   if (i < ITEMCOUNT(state))
      if (shiftprec < reduceprec)
	 if (ITEMSET(state, i).descendant.state)
	 {
	    if (tables->display & DISPLAY_V)
	       fprintf(stderr, "Shift precedence %d is higher than reduce precedence %d; action will be shift\n", shiftprec, reduceprec);
	 }
	 else
	 {
	    if (tables->display & DISPLAY_V)
	       fprintf(stderr, "Shiftreduce precedence %d is higher than reduce precedence %d; action will be shiftreduce\n", shiftprec, reduceprec);
	 }
      else
	 if (reduceprec < shiftprec)
	 {
	    if (tables->display & DISPLAY_V)
	       fprintf(stderr, "Reduce precedence %d is higher than %s precedence %d; action will be reduce\n", reduceprec,
		  (ITEMSET(state, i).descendant.state) ? "shift" : "shiftreduce", shiftprec);
	    tables->lrstates[state][RHSIDE(ITEMSET(state, i).prod, ITEMSET(state, i).dot)->value.value.token] = -ITEMSET(state, item).prod;
	 }
	 else
	    if (associativity == LEFT)
	    {
	       if (tables->display & DISPLAY_V)
		  fprintf(stderr, "%s precedence %d equals reduce precedence %d and associativity = LEFT; action will be reduce\n",
		     (ITEMSET(state, i).descendant.state) ? "Shift" : "Shiftreduce", shiftprec, reduceprec);
	       tables->lrstates[state][RHSIDE(ITEMSET(state, i).prod, ITEMSET(state, i).dot)->value.value.token] = -ITEMSET(state, item).prod;
	    }
	    else
	       if (associativity == RIGHT)
		  if (ITEMSET(state, i).descendant.state)
		  {
		     if (tables->display & DISPLAY_V)
			fprintf(stderr, "Shift precedence %d equals reduce precedence %d and associativity = RIGHT; action will be shift\n",
			   shiftprec, reduceprec);
		  }
		  else
		  {
		     if (tables->display & DISPLAY_V)
			fprintf(stderr, "Shiftreduce precedence %d equals reduce precedence %d and associativity = RIGHT; action will be shiftreduce\n",
			   shiftprec, reduceprec);
		  }
	       else
		  fprintf(stderr, "%s precedence %d equals reduce precedence %d and associativity = NONE\n",
		     (ITEMSET(state, i).descendant.state) ? "Shift" : "Shiftreduce", shiftprec, reduceprec);
}


static void setup_lookahead
(
   sdt_tables *tables
)
{
   symbolentry *symbol;
   bool		changed;
   symbolset	follow;
   symbolset	merge;
   int		i, j, k;

   for (i = 1; i < COLLCOUNT; i++)
   {
/*    Give each kernel item a unique follow set symbol to identify it */

      for (j = 0; j < COLLECTION(i).kernel; j++)
      {
	 symbol = alloc_symbol("marker");
	 symbol->value.value.token = tables->termcount + 1 + j;
	 symbolset_insert(&ITEMSET(i, j).follow, symbol);
      }

/*    Propagate spontaneous follow sets throughout this item set */

      symbolset_alloc(&follow, INITIAL_FOLLOW_SIZE);
      do
	 for (changed = false, j = 0; j < ITEMCOUNT(i); j++)
	    if (ITEMSET(i, j).dot < PRODUCTION(ITEMSET(i, j).prod).length && RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->type == NONTERMINAL)
	    {
/*	       Compute the follow set for this item */

	       SYMCOUNT(follow) = 0;
	       for (k = ITEMSET(i, j).dot + 1; k < PRODUCTION(ITEMSET(i, j).prod).length; k++)
	       {
		  symbolset_union(&merge, &follow, &tables->first[RHSIDE(ITEMSET(i, j).prod, k)->value.value.token].symbols);
		  symbolset_free(&follow);
		  follow = merge;
		  if (!tables->first[RHSIDE(ITEMSET(i, j).prod, k)->value.value.token].nullable)
		     break;
	       }
	       if (k >= PRODUCTION(ITEMSET(i, j).prod).length)
	       {
		  symbolset_union(&merge, &follow, &ITEMSET(i, j).follow);
		  symbolset_free(&follow);
		  follow = merge;
	       }

/*	       Compute follow sets generated spontaneously within the closure  */
/*	       Also determine the source and targets of propagated follow sets */

	       for (k = COLLECTION(i).kernel; k < ITEMCOUNT(i); k++)
		  if (PRODUCTION(ITEMSET(i, k).prod).lhside->value.value.token == RHSIDE(ITEMSET(i, j).prod, ITEMSET(i, j).dot)->value.value.token)
		  {
		     symbolset_union(&merge, &ITEMSET(i, k).follow, &follow);
		     if (symbolset_equal(&merge, &ITEMSET(i, k).follow))
			symbolset_free(&merge);
		     else
		     {
			symbolset_free(&ITEMSET(i, k).follow);
			ITEMSET(i, k).follow = merge;
			changed              = true;
		     }
		  }
	    }
      while (changed);
      symbolset_free(&follow);

/*    Every item containing a marker receives propagated lookaheads from the kernel */

      for (j = 0; j < COLLECTION(i).kernel; j++)
      {
/*	 If this kernel item has a goto it must update it */

	 if (ITEMSET(i, j).descendant.state)
	 {
	    dyncheck(&ITEMSET(i, j).update, UPDSIZE(i, j) * 2);
	    UPDATE(i, j, UPDCOUNT(i, j)).state = ITEMSET(i, j).descendant.state;
	    UPDATE(i, j, UPDCOUNT(i, j)).item  = ITEMSET(i, j).descendant.item;
	    UPDCOUNT(i, j)++;
	 }

	 for (k = COLLECTION(i).kernel; k < ITEMCOUNT(i); k++)
	    if (symbol = find_marker(&ITEMSET(i, k).follow, tables->termcount + 1 + j))
	    {
/*	       If kernel item j updated this item, record it */

	       dyncheck(&ITEMSET(i, j).update, UPDSIZE(i, j) * 2);
	       UPDATE(i, j, UPDCOUNT(i, j)).state = i;
	       UPDATE(i, j, UPDCOUNT(i, j)).item  = k;
	       UPDCOUNT(i, j)++;

	       symbolset_delete(&ITEMSET(i, k).follow, symbol);

/*	       If this item has a goto the kernel item must update it as well */

	       if (ITEMSET(i, k).descendant.state && (ITEMSET(i, k).descendant.state != i || ITEMSET(i, k).descendant.item != j) &&
		  find_update(tables, i, j, &ITEMSET(i, k).descendant) < 0)
	       {
		  dyncheck(&ITEMSET(i, j).update, UPDSIZE(i, j) * 2);
		  UPDATE(i, j, UPDCOUNT(i, j)).state = ITEMSET(i, k).descendant.state;
		  UPDATE(i, j, UPDCOUNT(i, j)).item  = ITEMSET(i, k).descendant.item;
		  UPDCOUNT(i, j)++;
	       }
	    }

/*	 Clean up the kernel marker symbol */

	 symbol = find_marker(&ITEMSET(i, j).follow, tables->termcount + 1 + j);
	 symbolset_delete(&ITEMSET(i, j).follow, symbol);
	 free(symbol->symbol);
	 free(symbol);
      }
   }
}


static void sort_productions
(
   sdt_tables *tables
)
{
/* Order productions for error repair table generation */

   int	      min;
   production temp;
   int	      i, j, k;

/* Sort the alternatives for each left hand side symbol */

   for (i = 1; i <= tables->nontermcount; i++)
      for (j = tables->lhsindex[i]; j < PRODCOUNT && PRODUCTION(j).lhside->value.value.token == tables->termcount + i; j++)
      {
/*	 Find the smallest of the remaining right hand sides */

	 for (min = j, k = j + 1; k < PRODCOUNT && PRODUCTION(k).lhside->value.value.token == PRODUCTION(j).lhside->value.value.token; k++)
	    if (PRODUCTION(k).steps < PRODUCTION(min).steps)
	       min = k;
	    else
	       if (PRODUCTION(k).steps == PRODUCTION(min).steps && PRODUCTION(k).insert < PRODUCTION(min).insert)
		  min = k;

/*	 Swap PRODUCTION(j) and PRODUCTION(min) if necessary */

	 if (j != min)
	 {
	    temp            = PRODUCTION(j);
	    PRODUCTION(j)   = PRODUCTION(min);
	    PRODUCTION(min) = temp;
	 }
      }
}


static bool split_states
(
   sdt_tables  *tables,
   int		state
)
{
/* Attempt to repair a reduce-reduce conflict by splitting LR(0) states */

   dynarray   conflict;
   bool	      failure;
   int	      width1;
   int	      width2;
   collision *src;
   int	      curr;
   int	      item;
   dynarray   groups;
   int	      i, j, k, l;

/* Initialize conflict to reduces with conflicting lookaheads */

   dynalloc(&conflict, sizeof(collision), INITIAL_CONFLICT_SIZE);
   find_conflict(tables, state, &DYNARRAY(collision, conflict, 0));
   DYNCOUNT(conflict) = 1;

   if (tables->options & SPLITSTATES)
   {
      do
      {
/*	 Check for a conflict in spontaneous lookahead which cannot be resolved */

	 for (failure = false, i = 0; !failure && i < DYNCOUNT(conflict); i++)
	    if (!DYNARRAY(collision, conflict, i).success)
	       failure = spontaneous_conflict(tables, &DYNARRAY(collision, conflict, i));
	 if (failure)
	    break;

/*	 Move back to the kernel items that included the conflict */

	 for (i = 0; i < DYNCOUNT(conflict); i++)
	    if (!DYNARRAY(collision, conflict, i).success)
	       kernel_items(tables, &DYNARRAY(collision, conflict, i));

/*	 Move to the previous states for every conflict that hasn't */
/*	 been successfully resolved or reached the end of the lane  */

	 previous_states(tables, &conflict);

/*	 Continue until we find a resolution or fail */
      }
      while (check_conflicts(tables, &conflict));

      if (!failure)
      {
	 if (tables->display & DISPLAY_V)
	 {
	    for (width1 = width2 = i = 0; i < DYNCOUNT(conflict); i++)
	    {
	       src = &DYNARRAY(collision, conflict, i);
	       for (j = 0; j < src->count; j++)
		  for (k = 0; k < DYNCOUNT(src->lanes[j].lane); k++)
		  {
		     curr = DYNARRAY(laneentry, src->lanes[j].lane, k).state;
		     if (curr > width1)
			width1 = curr;
		     for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, src->lanes[j].lane, k).items); l++)
		     {
			item = INTSET(DYNARRAY(laneentry, src->lanes[j].lane, k).items, l);
			if (item > width2)
			   width2 = item;
		     }
		  }
	    }
	    width1 = digit_count(width1);
	    width2 = digit_count(width2);

	    for (i = 0; i < DYNCOUNT(conflict); i++)
	    {
	       fprintf(stderr, "Conflict Resolution %d:\n", i + 1);
	       src = &DYNARRAY(collision, conflict, i);
	       for (j = 0; j < src->count; j++)
	       {
		  fprintf(stderr, "   Lane %d: follow [", j + 1);
		  display_symbolset(&src->lanes[j].follow, stderr);
		  fputs("]\n", stderr);
		  for (k = 0; k < DYNCOUNT(src->lanes[j].lane); k++)
		  {
		     fputs("      ", stderr);
		     curr = DYNARRAY(laneentry, src->lanes[j].lane, k).state;
		     for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, src->lanes[j].lane, k).items); l++)
		     {
			item = INTSET(DYNARRAY(laneentry, src->lanes[j].lane, k).items, l);
			display_item(tables, ITEMSET(curr, item).prod, ITEMSET(curr, item).dot, curr, width1, item, width2, 0, 0, 0, 0, stderr);
			fputs(", [", stderr);
			display_symbolset(&ITEMSET(curr, item).lookahead, stderr);
			fputc(']', stderr);
			if (l < INTCOUNT(DYNARRAY(laneentry, src->lanes[j].lane, k).items) - 1)
			   fputs("; ", stderr);
		     }
		     fputc('\n', stderr);
		  }
	       }
	    }
	 }

/*	 Group conflict resolutions with compatible lookaheads */

	 group_conflicts(tables, &conflict, &groups);
	 if (tables->display & DISPLAY_V)
	    for (i = 0; i < DYNCOUNT(groups); i++)
	       if (INTCOUNT(DYNARRAY(intset, groups, i)) > 1)
	       {
		  fputs("The lookaheads for conflict resolutions ", stderr);
		  for (j = 0; j < INTCOUNT(DYNARRAY(intset, groups, i)); j++)
		  {
		     fprintf(stderr, "%d", INTSET(DYNARRAY(intset, groups, i), j) + 1);
		     if (j < INTCOUNT(DYNARRAY(intset, groups, i)) - 1)
		     {
			if (INTCOUNT(DYNARRAY(intset, groups, i)) > 2)
			   fputs(", ", stderr);
			if (j == INTCOUNT(DYNARRAY(intset, groups, i)) - 2)
			   fputs(" and ", stderr);
		     }
		  }
		  fputs(" are compatible\n", stderr);
	       }

/*	 Split the conflict resolution states in one copy for each resolution */

	 copy_states(tables, &conflict, &groups);

	 for (i = 0; i < DYNCOUNT(groups); i++)
	    intset_free(&DYNARRAY(intset, groups, i));
	 dynfree(&groups);

	 fputs("Reduce-Reduce conflict has been resolved\n\n", stderr);
      }
      else
      {
	 fputs("Reduce-Reduce conflict cannot be resolved\n\n", stderr);
	 tables->process = false;
      }
   }
   else
   {
      tables->process = false;
      failure         = true;
   }

/* Free state splitting bookkeeping */

   for (i = 0; i < DYNCOUNT(conflict); i++)
   {
      src = &DYNARRAY(collision, conflict, i);
      for (j = 0; j < src->count; j++)
      {
	 for (k = 0; k < DYNCOUNT(src->lanes[j].lane); k++)
	    intset_free(&DYNARRAY(laneentry, src->lanes[j].lane, k).items);
	 dynfree(&src->lanes[j].lane);
	 symbolset_free(&src->lanes[j].follow);
      }
      free(src->lanes);
   }
   dynfree(&conflict);
   return(!failure);
}


static bool spontaneous_conflict
(
   sdt_tables *tables,
   collision  *conflict
)
{
/* Check if there is a reduce-reduce conflict in spontaneous lookaheads */

   symbolset intersect;
   bool	     failure;
   int	     width1;
   int	     width2;
   int	     state;
   int	     item;
   int	     i, j, k, l;

   for (failure = false, i = 0; !failure && i < conflict->count; i++)
      for (j = i + 1; !failure && j < conflict->count; j++)
      {
	 symbolset_intersect(&intersect, &conflict->lanes[i].follow, &conflict->lanes[j].follow);
	 if (SYMCOUNT(intersect))
	    failure = true;
	 symbolset_free(&intersect);
      }

/* Report failure */

   if (failure)
   {
      fputs("Spontaneous lookahead conflict\n", stderr);
      for (i = 0; i < conflict->count; i++)
	 for (j = i + 1; j < conflict->count; j++)
	 {
	    symbolset_intersect(&intersect, &conflict->lanes[i].follow, &conflict->lanes[j].follow);
	    if (SYMCOUNT(intersect))
	    {
	       for (width1 = width2 = k = 0; k < DYNCOUNT(conflict->lanes[i].lane); k++)
	       {
		  if ((state = DYNARRAY(laneentry, conflict->lanes[i].lane, k).state) > width1)
		     width1 = state;
		  for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[i].lane, k).items); l++)
		  {
		     if ((item = INTSET(DYNARRAY(laneentry, conflict->lanes[i].lane, k).items, l)) > width2)
			width2 = item;
		  }
	       }
	       width1 = digit_count(width1);
	       width2 = digit_count(width2);
	       fprintf(stderr, "   Lane %d: follow [", i + 1);
	       display_symbolset(&conflict->lanes[i].follow, stderr);
	       fputs("]\n", stderr);
	       for (k = 0; k < DYNCOUNT(conflict->lanes[i].lane); k++)
	       {
		  state = DYNARRAY(laneentry, conflict->lanes[i].lane, k).state;
		  fputs("      ", stderr);
	          for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[i].lane, k).items); l++)
		  {
		     item = INTSET(DYNARRAY(laneentry, conflict->lanes[i].lane, k).items, l);
		     display_item(tables, ITEMSET(state, item).prod, ITEMSET(state, item).dot, state, width1, item, width2, 0, 0, 0, 0, stderr);
		     fputs(", [", stderr);
		     display_symbolset(&ITEMSET(state, item).lookahead, stderr);
		     fputc(']', stderr);
		     if (l < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[i].lane, k).items) - 1)
			fputs("; ", stderr);
		  }
		  fputc('\n', stderr);
	       }
	       fputs("   Conflicts with\n", stderr);
	       fprintf(stderr, "   Lane %d: follow [", j + 1);
	       display_symbolset(&conflict->lanes[j].follow, stderr);
	       fputs("]\n", stderr);
	       for (k = 0; k < DYNCOUNT(conflict->lanes[j].lane); k++)
	       {
		  state = DYNARRAY(laneentry, conflict->lanes[j].lane, k).state;
		  fputs("      ", stderr);
	          for (l = 0; l < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[j].lane, k).items); l++)
		  {
		     item = INTSET(DYNARRAY(laneentry, conflict->lanes[j].lane, k).items, l);
		     display_item(tables, ITEMSET(state, item).prod, ITEMSET(state, item).dot, state, width1, item, width2, 0, 0, 0, 0, stderr);
		     fputs(", [", stderr);
		     display_symbolset(&ITEMSET(state, item).lookahead, stderr);
		     fputc(']', stderr);
		     if (l < INTCOUNT(DYNARRAY(laneentry, conflict->lanes[j].lane, k).items) - 1)
			fputs("; ", stderr);
		  }
		  fputc('\n', stderr);
	       }
	       fputs("   On [", stderr);
	       display_symbolset(&intersect, stderr);
	       fputs("]\n", stderr);
	    }
	    symbolset_free(&intersect);
	 }
   }
   return(failure);
}


void write_parser
(
   sdt_tables *tables,
   FILE	      *fp
)
{
/* Write the generated parser tables */

   int	 width;
   bool	 full;
   int	 length;
   int	 count;
   int	*index;
   int	 size;
   char	*string;
   int	 limit;
   int	 i, j;

/* Calculate the size of the largest insertion or deletion cost */

   for (width = 0, i = 1; i <= tables->termcount; i++)
      if (tables->termtable[i]->value.value.insert > width)
	 width = tables->termtable[i]->value.value.insert;
   for (i = 1; i <= tables->termcount; i++)
      if (tables->termtable[i]->value.value.delete > width)
	 width = tables->termtable[i]->value.value.delete;
   width = digit_count(width);

/* Write insertion and deletion cost tables */

   for (full = false, length = 0, i = 1; i <= tables->termcount; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, tables->termtable[i]->value.value.insert);
      length += width;
      if (i < tables->termcount && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);
   for (full = false, length = 0, i = 1; i <= tables->termcount; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, tables->termtable[i]->value.value.delete);
      length += width;
      if (i < tables->termcount && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Write production left hand side token numbers */

   for (width = 0, i = 1; i < PRODCOUNT; i++)
      if (PRODUCTION(i).lhside->value.value.token > width)
	 width = PRODUCTION(i).lhside->value.value.token;
   width = digit_count(width);

   for (full = false, length = 0, i = 1; i < PRODCOUNT; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, PRODUCTION(i).lhside->value.value.token);
      length += width;
      if (i < PRODCOUNT - 1 && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Write production right hand side lengths */

   for (width = 0, i = 1; i < PRODCOUNT; i++)
   {
/*    Count the number of non-epsilon tokens on the RHS */

      for (count = j = 0; j < PRODUCTION(i).length; j++)
         if (RHSIDE(i, j)->type != TERMINAL || (RHSIDE(i, j)->value.value.flags & EMPTY) != EMPTY)
	    count++;

      if (count > width)
	 width = count;
   }
   width = digit_count(width);

   for (full = false, length = 0, i = 1; i < PRODCOUNT; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      for (count = j = 0; j < PRODUCTION(i).length; j++)
         if (RHSIDE(i, j)->type != TERMINAL || (RHSIDE(i, j)->value.value.flags & EMPTY) != EMPTY)
	    count++;
      fprintf(fp, "%*d", width, count);
      length += width;
      if (i < PRODCOUNT - 1 && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Write semantic routine numbers */

   for (width = 0, i = 1; i < PRODCOUNT; i++)
      if (PRODUCTION(i).semantic > width)
	 width = PRODUCTION(i).semantic;
   width = digit_count(width);

   for (full = false, length = 0, i = 1; i < PRODCOUNT; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, PRODUCTION(i).semantic);
      length += width;
      if (i < PRODCOUNT - 1 && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Write error repair values */
   for (width = 0, i = 1; i < COLLCOUNT; i++)
      if (tables->errortoken[i] < 0)
      {
         if (-tables->errortoken[i] * 10 > width)
	    width = -tables->errortoken[i] * 10;
      }
      else
	 if (tables->errortoken[i] > width)
	    width = tables->errortoken[i];
   width = digit_count(width);

   for (full = false, length = 0, i = 1; i < COLLCOUNT; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, tables->errortoken[i]);
      length += width;
      if (i < COLLCOUNT - 1 && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Build concatenated symbol name string and index */

   if (!(index = (int *) malloc((tables->termcount + tables->nontermcount + 1) * sizeof(*index))))
      out_of_memory();
   for (size = 0, i = 1; i <= tables->termcount; i++)
   {
      index[i - 1] = size;
      size += strlen(tables->termtable[i]->symbol);
   }
   for (i = 1; i <= tables->nontermcount; i++)
   {
      index[tables->termcount + i - 1] = size;
      size += strlen(tables->nontermtable[i]->symbol);
   }
   index[tables->termcount + tables->nontermcount] = size;

   if (!(string = (char *) malloc(size + 1)))
      out_of_memory();
   for (i = 1; i <= tables->termcount; i++)
      strcpy(&string[index[i - 1]], tables->termtable[i]->symbol);
   for (i = 1; i <= tables->nontermcount; i++)
      strcpy(&string[index[tables->termcount + i - 1]], tables->nontermtable[i]->symbol);

   width = digit_count(size);

   for (full = false, length = i = 0; i <= tables->termcount + tables->nontermcount; i++)
   {
      if (length + width > MAXLINE || full)
      {
	 fputc('\n', fp);
	 full   = false;
	 length = 0;
      }
      fprintf(fp, "%*d", width, index[i]);
      length += width;
      if (i < tables->termcount + tables->nontermcount && length + 1 + width <= MAXLINE)
      {
	 fputc(' ', fp);
	 length++;
      }
      else
	 full = true;
   }
   if (length)
      fputc('\n', fp);

/* Write concatenated symbol string */

   fprintf(fp, "%d\n", MAXLINE);
   i = 0;
   while (i < size)
      if (size - i > MAXLINE)
      {
	 fprintf(fp, "%.*s\n", MAXLINE, &string[i]);
	 i += MAXLINE;
      }
      else
      {
	 fprintf(fp, "%.*s\n", size - i, &string[i]);
	 i = size;
      }

   free(index);
   free(string);

/* Write parsing tables */

   for (width = 0, i = 1; i < COLLCOUNT; i++)
      for (j = 1; j <= tables->termcount + tables->nontermcount; j++)
      {
	 if (width < j)
	    width = j;
         if (tables->lrstates[i][j] < 0)
	 {
	    if (-tables->lrstates[i][j] * 10 > width)
	       width = -tables->lrstates[i][j] * 10;
	 }
	 else
	    if (tables->lrstates[i][j] > width)
	       width = tables->lrstates[i][j];
      }
   width = digit_count(width);

   for (i = 1; i < COLLCOUNT; i++)
   {
/*    Count the number of actions and the index of the last one */

      for (length = 0, j = 1; j <= tables->termcount + tables->nontermcount; j++)
	 if (tables->lrstates[i][j])
	 {
	    limit = j;
	    length++;
	 }
      limit++;

      fprintf(fp, "%d\n", length);
      for (full = false, length = 0, j = 1; j < limit; j++)
	 if (tables->lrstates[i][j])
	 {
	    if (length + width + 1 + width > MAXLINE || full)
	    {
	       fputc('\n', fp);
	       full   = false;
	       length = 0;
	    }
	    fprintf(fp, "%*d %*d", width, j, width, tables->lrstates[i][j]);
	    length += width + 1 + width;
	    if (j < limit - 1 && length + 1 + width + 1 + width <= MAXLINE)
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
}
