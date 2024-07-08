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

#if !defined(_INCLUDED_SDTGEN_DEFINITIONS_H)
#define	  _INCLUDED_SDTGEN_DEFINITIONS_H

#define MAXLINE		80	/* Maximum table line length */

/* Sdtgen listing command line argument flags */

#define	DISPLAY_G	0x0001	/* Display the standardized grammar */
#define	DISPLAY_L	0x0002	/* Display the input file as it is parsed */
#define	DISPLAY_R	0x0004	/* Display token regular expressions */
#define DISPLAY_T	0x0008	/* Display the LR parsing tables */
#define DISPLAY_V	0x0010  /* Display shift-reduce and reduce-reduce resolution */
#define	DISPLAY_X	0x0020	/* Display a cross-reference of tokens */

/* Sdtgen debug command line argument flags */

#define DEBUG_A		0x0001	/* Display state ancestors */
#define DEBUG_D		0x0002	/* Display deterministic automaton before minimization */
#define DEBUG_E		0x0004	/* Display error repair values */
#define DEBUG_F		0x0008	/* Display nonterminal first sets */
#define DEBUG_G		0x0010	/* Display grammar productions */
#define DEBUG_I		0x0020	/* Display canonical collection of LR items */
#define DEBUG_M		0x0040	/* Display deterministic automaton after minimization */
#define DEBUG_N		0x0080	/* Display nondeterministic automaton */
#define DEBUG_P		0x0100	/* Display parser syntax tree */
#define DEBUG_S		0x0200	/* Display scanner syntax tree */

/* Sdtgen grammar option flags */

#define ERRORREPAIR	0x0001	/* Construct error repair tables */
#define DEFAULTREDUCE	0x0002	/* Use shiftreduce actions to reduce table size */
#define	AMBIGUOUS	0x0004	/* Use precedence and associativity to resolve shift-reduce conflicts */
#define SPLITSTATES	0x0008	/* Split states to resolve reduce-reduce conflicts */
#endif /* _INCLUDED_SDTGEN_DEFINITIONS_H */
