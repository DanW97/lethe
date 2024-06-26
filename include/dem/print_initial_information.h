/* ---------------------------------------------------------------------
 *
 * Copyright (C) 2019 - 2020 by the Lethe authors
 *
 * This file is part of the Lethe library
 *
 * The Lethe library is free software; you can use it, redistribute
 * it, and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * The full text of the license can be found in the file LICENSE at
 * the top level of the Lethe distribution.
 *
 * ---------------------------------------------------------------------
 *
 */

#include <dem/dem_solver_parameters.h>

#ifndef print_initial_information_h
#  define print_initial_information_h

using namespace std;

/**
 * Prints the initial information of the dem simulation including the number
 * of processors
 *
 * @param n_mpi_processes Number of processors in the simulation
 * @param pcout Printing in parallel
 *
 */

void
print_initial_information(const ConditionalOStream &pcout,
                          const unsigned int       &n_mpi_processes);

#endif /* print_initial_information_h */
