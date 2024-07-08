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

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "intset_definitions.h"

#include "dynarray_functions.h"
#include "utility_functions.h"


int digit_count
(
   int i
)
{
/* Return the number of digits in the base 10 representation of an integer */

   if (i == 0)
      return(1);
   else
      return(trunc(log10(i) + 1.0));
}


void display_intset
(
   intset *set,
   FILE   *fp
)
{
/* Display the integers in an intset */

   int i;

   for (i = 0; i < INTCOUNT(*set); i++)
   {
      fprintf(fp, "%d", INTSET(*set, i));
      if (i + 1 < INTCOUNT(*set))
         fputc(' ', fp);
   }
}


void intset_alloc
(
   intset *set,
   int	   size
)
{
/* Allocate set of integers */

   dynalloc(set, sizeof(int), size);
}


void intset_copy
(
   intset *dst,
   intset *src
)
{
/* Create a copy of a set of integers */

   dyncopy(dst, src);
}


void intset_delete
(
   intset *set,
   int	   value
)
{
/* Delete value from the set of integers if present */

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Find value in set */

      lower = 0;
      upper = INTCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (value < INTSET(*set, i))
	    upper = i;
	 else
	    if (value > INTSET(*set, i))
	       lower = i + 1;
	    else
	       break;
      }

/*    Remove the value from the set if it was found */

      if (lower < upper)
      {
         while (i < INTCOUNT(*set) - 1)
	 {
	    INTSET(*set, i) = INTSET(*set, i + 1);
	    i++;
	 }
	 INTCOUNT(*set)--;
      }
   }
}


bool intset_equal
(
   intset *set1,
   intset *set2
)
{
/* Return true if the sets contain the same values */

   if (INTCOUNT(*set1) != INTCOUNT(*set2) || INTELEMENT(*set1) != INTELEMENT(*set2))
      return(false);
   return(memcmp(&INTSET(*set1, 0), &INTSET(*set2, 0), INTCOUNT(*set1) * INTELEMENT(*set1)) == 0);
}


int intset_find
(
   intset *set,
   int	   value
)
{
/* Find the index of value in a set of integers */
/* Return -1 if not found			*/

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Look for value in set */

      lower = 0;
      upper = INTCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (value < INTSET(*set, i))
	    upper = i;
	 else
	    if (value > INTSET(*set, i))
	       lower = i + 1;
	    else
	       return(i);
      }
   }
   return(-1);
}


void intset_free
(
   intset *set
)
{
/* Free a set of integers */

   dynfree(set);
}


void intset_insert
(
   intset *set,
   int	   value
)
{
/* Insert value into a set of integers if not already present */

   int lower;
   int upper;
   int i;

   if (set->array)
   {
/*    Check if value is already in the set */

      lower = 0;
      upper = INTCOUNT(*set);
      while (lower < upper)
      {
	 i = lower + (upper - lower) / 2;
	 if (value < INTSET(*set, i))
	    upper = i;
	 else
	    if (value > INTSET(*set, i))
	       lower = i + 1;
	    else
	       return;
      }

/*    Insert the new value into the correct place in the set */

      dyncheck(set, INTSIZE(*set) * 2);
      for (i = INTCOUNT(*set); i > 0 && INTSET(*set, i - 1) > value; i--)
	 INTSET(*set, i) = INTSET(*set, i - 1);
      INTSET(*set, i) = value;
      INTCOUNT(*set)++;
   }
   else
   {
/*    Allocate the initial entry in the set */

      intset_alloc(set, INITIAL_INTSET_SIZE);
      INTSET(*set, INTCOUNT(*set)++) = value;
   }
}


void intset_intersect
(
   intset *dst,
   intset *src1,
   intset *src2
)
{
/* Create the intersection of two sets of integers */

   int i1;
   int i2;

   intset_alloc(dst, INITIAL_INTSET_SIZE);
   if (src1->array && src2->array)
   {
      i1 = 0;
      i2 = 0;
      while (i1 < INTCOUNT(*src1) && i2 < INTCOUNT(*src2))
      {
/*	 Skip the elements in src1 which are less than the current element in src2 */

	 while (i1 < INTCOUNT(*src1) && INTSET(*src1, i1) < INTSET(*src2, i2))
	    i1++;

/*	 If the current elements in src1 and src2 are equal copy 1 and skip the other */

	 if (i1 < INTCOUNT(*src1) && INTSET(*src1, i1) == INTSET(*src2, i2))
	 {
	    dyncheck(dst, INTSIZE(*dst) * 2);
	    INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src1, i1++);
	    i2++;
	 }

/*	 Skip the elements in src2 which are less than the current element in src1 */

	 while (i1 < INTCOUNT(*src1) && i2 < INTCOUNT(*src2) && INTSET(*src2, i2) < INTSET(*src1, i1))
	    i2++;
      }
   }
}


int intset_size
(
   intset *set
)
{
/* Calculate the size of the string displayed by display_intset */

   int size;
   int i;

   size = 0;
   for (i = 0; i < INTCOUNT(*set); i++)
   {
      size += digit_count(INTSET(*set, i));
      if (i + 1 < INTCOUNT(*set))
	 size++;
   }
   return(size);
}


void intset_union
(
   intset *dst,
   intset *src1,
   intset *src2
)
{
/* Create the union of two sets of integers */

   int i1;
   int i2;

   if (src1->array && src2->array)
   {
      intset_alloc(dst, INITIAL_INTSET_SIZE);
      i1 = 0;
      i2 = 0;
      while (i1 < INTCOUNT(*src1) && i2 < INTCOUNT(*src2))
      {
/*	 Copy over the elements in src1 which are less than the current element in src2 */

	 while (i1 < INTCOUNT(*src1) && INTSET(*src1, i1) < INTSET(*src2, i2))
	 {
	    dyncheck(dst, INTSIZE(*dst) * 2);
	    INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src1, i1++);
	 }

/*	 If the current elements in src1 and src2 are equal copy 1 and skip the other */

	 if (i1 < INTCOUNT(*src1) && INTSET(*src1, i1) == INTSET(*src2, i2))
	 {
	    dyncheck(dst, INTSIZE(*dst) * 2);
	    INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src1, i1++);
	    i2++;
	 }

/*	 Copy over the elements in src2 which are less than the current element in src1 */

	 while (i1 < INTCOUNT(*src1) && i2 < INTCOUNT(*src2) && INTSET(*src2, i2) < INTSET(*src1, i1))
	 {
	    dyncheck(dst, INTSIZE(*dst) * 2);
	    INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src2, i2++);
	 }
      }

/*    Copy any remaining elements in src1 or src2 whichever is non-empty */

      while (i1 < INTCOUNT(*src1))
      {
	 dyncheck(dst, INTSIZE(*dst) * 2);
	 INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src1, i1++);
      }
      while (i2 < INTCOUNT(*src2))
      {
	 dyncheck(dst, INTSIZE(*dst) * 2);
	 INTSET(*dst, INTCOUNT(*dst)++) = INTSET(*src2, i2++);
      }
   }
   else
      if (src1->array)
	 dyncopy(dst, src1);
      else
         if (src2->array)
	    dyncopy(dst, src2);
	 else
	    intset_alloc(dst, INITIAL_INTSET_SIZE);
}
