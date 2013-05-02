/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#ifndef _KASSERT_H
#define _KASSERT_H

#include <panic.h>
#include <stdnoreturn.h>


// kassert_exec wird die Kondition auch bei NDEBUG noch auswerten (im Gegensatz zum normalen assert)
#define kassert_exec(cond) do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s", __FILE__, __LINE__, __func__, #cond); }  while (0)
#define kassert(cond)      do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s", __FILE__, __LINE__, __func__, #cond); }  while (0)

// kassert_exec_print wird die Kondition auswerten und außerdem den übergebenen Formatstring ausgeben (der nur die
// Formatangaben %i, %s und %p enthalten darf).
#define kassert_exec_print(cond, format, ...) do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s\n\n" format, __FILE__, __LINE__, __func__, #cond, ##__VA_ARGS__); } while (0)
#define kassert_print(cond, format, ...)      do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s\n\n" format, __FILE__, __LINE__, __func__, #cond, ##__VA_ARGS__); } while (0)

#endif
