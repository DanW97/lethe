// SPDX-FileCopyrightText: Copyright (c) 2023-2024 The Lethe Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR LGPL-2.1-or-later

#ifndef lethe_isothermal_compressible_navier_stokes_vof_assembler_h
#define lethe_isothermal_compressible_navier_stokes_vof_assembler_h

#include <core/simulation_control.h>

#include <solvers/auxiliary_physics.h>
#include <solvers/copy_data.h>
#include <solvers/navier_stokes_assemblers.h>
#include <solvers/navier_stokes_scratch_data.h>


/**
 * @brief Class that assembles the core of the Navier-Stokes equation with
 * free surface using VOF modeling.
 * According to the following weak form:
 * \f$\nabla \cdot (\rho \mathbf{u}) + \rho \mathbf{u} \cdot \nabla
 * \mathbf{u} - \nabla p - \mu \nabla\cdot (\nabla \mathbf{u} +
 * (\nabla \mathbf{u})^T) = 0 \f$ with a full GLS stabilization
 *
 * @tparam dim An integer that denotes the number of spatial dimensions
 *
 * @ingroup assemblers
 */
template <int dim>
class GLSIsothermalCompressibleNavierStokesVOFAssemblerCore
  : public NavierStokesAssemblerBase<dim>
{
public:
  GLSIsothermalCompressibleNavierStokesVOFAssemblerCore(
    const std::shared_ptr<SimulationControl> &simulation_control,
    const SimulationParameters<dim>          &nsparam)
    : simulation_control(simulation_control)
    , vof_parameters(nsparam.multiphysics.vof_parameters)
  {}

  /**
   * @brief assemble_matrix Assembles the matrix
   * @param scratch_data (see base class)
   * @param copy_data (see base class)
   */
  virtual void
  assemble_matrix(const NavierStokesScratchData<dim>   &scratch_data,
                  StabilizedMethodsTensorCopyData<dim> &copy_data) override;

  /**
   * @brief assemble_rhs Assembles the rhs
   * @param scratch_data (see base class)
   * @param copy_data (see base class)
   */
  virtual void
  assemble_rhs(const NavierStokesScratchData<dim>   &scratch_data,
               StabilizedMethodsTensorCopyData<dim> &copy_data) override;

  const std::shared_ptr<SimulationControl> simulation_control;
  const Parameters::VOF                    vof_parameters;
};


template <int dim>
class GLSIsothermalCompressibleNavierStokesVOFAssemblerBDF
  : public NavierStokesAssemblerBase<dim>
{
public:
  GLSIsothermalCompressibleNavierStokesVOFAssemblerBDF(
    const std::shared_ptr<SimulationControl> &simulation_control)
    : simulation_control(simulation_control)
  {}

  /**
   * @brief assemble_matrix Assembles the matrix
   * @param scratch_data (see base class)
   * @param copy_data (see base class)
   */
  virtual void
  assemble_matrix(const NavierStokesScratchData<dim>   &scratch_data,
                  StabilizedMethodsTensorCopyData<dim> &copy_data) override;

  /**
   * @brief assemble_rhs Assembles the rhs
   * @param scratch_data (see base class)
   * @param copy_data (see base class)
   */
  virtual void
  assemble_rhs(const NavierStokesScratchData<dim>   &scratch_data,
               StabilizedMethodsTensorCopyData<dim> &copy_data) override;

  const std::shared_ptr<SimulationControl> simulation_control;
};


#endif
