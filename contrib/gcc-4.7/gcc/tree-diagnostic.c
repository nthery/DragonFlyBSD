/* Language-independent diagnostic subroutines for the GNU Compiler
   Collection that are only for use in the compilers proper and not
   the driver or other programs.
   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
   2009, 2010 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "diagnostic.h"
#include "tree-diagnostic.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "vec.h"

/* Prints out, if necessary, the name of the current function
   that caused an error.  Called from all error and warning functions.  */
void
diagnostic_report_current_function (diagnostic_context *context,
				    diagnostic_info *diagnostic)
{
  diagnostic_report_current_module (context, diagnostic->location);
  lang_hooks.print_error_function (context, input_filename, diagnostic);
}

void
default_tree_diagnostic_starter (diagnostic_context *context,
				 diagnostic_info *diagnostic)
{
  diagnostic_report_current_function (context, diagnostic);
  pp_set_prefix (context->printer, diagnostic_build_prefix (context,
							    diagnostic));
}

/* This is a pair made of a location and the line map it originated
   from.  It's used in the maybe_unwind_expanded_macro_loc function
   below.  */
typedef struct
{
  const struct line_map *map;
  source_location where;
} loc_map_pair;

DEF_VEC_O (loc_map_pair);
DEF_VEC_ALLOC_O (loc_map_pair, heap);

/* Unwind the different macro expansions that lead to the token which
   location is WHERE and emit diagnostics showing the resulting
   unwound macro expansion trace.  Let's look at an example to see how
   the trace looks like.  Suppose we have this piece of code,
   artificially annotated with the line numbers to increase
   legibility:

    $ cat -n test.c
      1    #define OPERATE(OPRD1, OPRT, OPRD2) \
      2      OPRD1 OPRT OPRD2;
      3
      4    #define SHIFTL(A,B) \
      5      OPERATE (A,<<,B)
      6
      7    #define MULT(A) \
      8      SHIFTL (A,1)
      9
     10    void
     11    g ()
     12    {
     13      MULT (1.0);// 1.0 << 1; <-- so this is an error.
     14    }

   Here is the diagnostic that we want the compiler to generate:

    test.c: In function 'g':
    test.c:5:14: error: invalid operands to binary << (have 'double' and 'int')
    test.c:2:9: note: in expansion of macro 'OPERATE'
    test.c:5:3: note: expanded from here
    test.c:5:14: note: in expansion of macro 'SHIFTL'
    test.c:8:3: note: expanded from here
    test.c:8:3: note: in expansion of macro 'MULT2'
    test.c:13:3: note: expanded from here

   The part that goes from the third to the eighth line of this
   diagnostic (the lines containing the 'note:' string) is called the
   unwound macro expansion trace.  That's the part generated by this
   function.

   If FIRST_EXP_POINT_MAP is non-null, *FIRST_EXP_POINT_MAP is set to
   the map of the location in the source that first triggered the
   macro expansion.  This must be an ordinary map.  */

static void
maybe_unwind_expanded_macro_loc (diagnostic_context *context,
                                 diagnostic_info *diagnostic,
                                 source_location where,
                                 const struct line_map **first_exp_point_map)
{
  const struct line_map *map;
  VEC(loc_map_pair,heap) *loc_vec = NULL;
  unsigned ix;
  loc_map_pair loc, *iter;

  map = linemap_lookup (line_table, where);
  if (!linemap_macro_expansion_map_p (map))
    return;

  /* Let's unwind the macros that got expanded and led to the token
     which location is WHERE.  We are going to store these macros into
     LOC_VEC, so that we can later walk it at our convenience to
     display a somewhat meaningful trace of the macro expansion
     history to the user.  Note that the first macro of the trace
     (which is OPERATE in the example above) is going to be stored at
     the beginning of LOC_VEC.  */

  do
    {
      loc.where = where;
      loc.map = map;

      VEC_safe_push (loc_map_pair, heap, loc_vec, &loc);

      /* WHERE is the location of a token inside the expansion of a
         macro.  MAP is the map holding the locations of that macro
         expansion.  Let's get the location of the token inside the
         context that triggered the expansion of this macro.
         This is basically how we go "down" in the trace of macro
         expansions that led to WHERE.  */
      where = linemap_unwind_toward_expansion (line_table, where, &map);
    } while (linemap_macro_expansion_map_p (map));

  if (first_exp_point_map)
    *first_exp_point_map = map;

  /* Walk LOC_VEC and print the macro expansion trace, unless the
     first macro which expansion triggered this trace was expanded
     inside a system header.  */
  if (!LINEMAP_SYSP (map))
    FOR_EACH_VEC_ELT (loc_map_pair, loc_vec, ix, iter)
      {
        source_location resolved_def_loc = 0, resolved_exp_loc = 0;
        diagnostic_t saved_kind;
        const char *saved_prefix;
        source_location saved_location;

        /* Okay, now here is what we want.  For each token resulting
           from macro expansion we want to show: 1/ where in the
           definition of the macro the token comes from; 2/ where the
           macro got expanded.  */

        /* Resolve the location iter->where into the locus 1/ of the
           comment above.  */
        resolved_def_loc =
          linemap_resolve_location (line_table, iter->where,
                                    LRK_MACRO_DEFINITION_LOCATION, NULL);

        /* Resolve the location of the expansion point of the macro
           which expansion gave the token represented by def_loc.
           This is the locus 2/ of the earlier comment.  */
        resolved_exp_loc =
          linemap_resolve_location (line_table,
                                    MACRO_MAP_EXPANSION_POINT_LOCATION (iter->map),
                                    LRK_MACRO_DEFINITION_LOCATION, NULL);

        saved_kind = diagnostic->kind;
        saved_prefix = context->printer->prefix;
        saved_location = diagnostic->location;

        diagnostic->kind = DK_NOTE;
        diagnostic->location = resolved_def_loc;
        pp_base_set_prefix (context->printer,
                            diagnostic_build_prefix (context,
                                                     diagnostic));
        pp_newline (context->printer);
        pp_printf (context->printer, "in expansion of macro '%s'",
                   linemap_map_get_macro_name (iter->map));
        pp_destroy_prefix (context->printer);

        diagnostic->location = resolved_exp_loc;
        pp_base_set_prefix (context->printer,
                            diagnostic_build_prefix (context,
                                                     diagnostic));
        pp_newline (context->printer);
        pp_printf (context->printer, "expanded from here");
        pp_destroy_prefix (context->printer);

        diagnostic->kind = saved_kind;
        diagnostic->location = saved_location;
        context->printer->prefix = saved_prefix;
      }

  VEC_free (loc_map_pair, heap, loc_vec);
}

/*  This is a diagnostic finalizer implementation that is aware of
    virtual locations produced by libcpp.

    It has to be called by the diagnostic finalizer of front ends that
    uses libcpp and wish to get diagnostics involving tokens resulting
    from macro expansion.

    For a given location, if said location belongs to a token
    resulting from a macro expansion, this starter prints the context
    of the token.  E.g, for multiply nested macro expansion, it
    unwinds the nested macro expansions and prints them in a manner
    that is similar to what is done for function call stacks, or
    template instantiation contexts.  */
void
virt_loc_aware_diagnostic_finalizer (diagnostic_context *context,
				     diagnostic_info *diagnostic)
{
  maybe_unwind_expanded_macro_loc (context, diagnostic,
				   diagnostic->location,
				   NULL);
}
