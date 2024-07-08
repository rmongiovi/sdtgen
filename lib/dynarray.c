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
#include <stdlib.h>
#include <string.h>

#include "dynarray_definitions.h"

#include "dynarray_functions.h"
#include "utility_functions.h"


void dynalloc
(
   dynarray *array,
   int	     element,
   int	     size
)
{
/* Allocate dynamically resizeable array */

   if (array->array = malloc(size * element))
   {
      array->element = element;
      array->count   = 0;
      array->size    = size;
   }
   else
      out_of_memory();
}


bool dyncheck
(
   dynarray *array,
   int	     size
)
{
/* Set new size of dynamically resizeable array if it is full */

   if (array->count >= array->size)
   {
      dynresize(array, size);
      return(true);
   }
   else
      return(false);
}


void dyncopy
(
   dynarray *dst,
   dynarray *src
)
{
/* Create a copy of a dynamically resizeable array */

   dynalloc(dst, src->element, src->size);
   if (src->count > 0)
      memcpy(dst->array, src->array, (dst->count = src->count) * src->element);
}


void dynfree
(
   dynarray *array
)
{
/* Free dynamically resizable array */

   free(array->array);
   array->array   = NULL;
   array->element = 0;
   array->count   = 0;
   array->size    = 0;
}


void dynresize
(
   dynarray *array,
   int	     size
)
{
/* Change size of dynamically resizeable array */

   if (array->array = realloc(array->array, size * array->element))
   {
      array->size = size;
      if (array->count > array->size)
	 array->count = array->size;
   }
   else
      out_of_memory();
}
