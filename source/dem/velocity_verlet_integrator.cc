// SPDX-FileCopyrightText: Copyright (c) 2020-2024 The Lethe Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR LGPL-2.1-or-later

#include <core/dem_properties.h>
#include <core/tensors_and_points_dimension_manipulation.h>

#include <dem/adaptive_sparse_contacts.h>
#include <dem/velocity_verlet_integrator.h>

using namespace DEM;

template <int dim, typename PropertiesIndex>
void
VelocityVerletIntegrator<dim, PropertiesIndex>::integrate_half_step_location(
  Particles::ParticleHandler<dim> &particle_handler,
  const Tensor<1, 3>              &g,
  const double                     dt,
  const std::vector<Tensor<1, 3>> &torque,
  const std::vector<Tensor<1, 3>> &force,
  const std::vector<double>       &MOI)
{
  Tensor<1, dim> particle_acceleration;

  for (auto particle = particle_handler.begin();
       particle != particle_handler.end();
       ++particle)
    {
      // Get the total array view to the particle properties once to improve
      // efficiency
      auto                  particle_properties = particle->get_properties();
      auto                  particle_position   = particle->get_location();
      types::particle_index particle_id         = particle->get_local_index();

      for (int d = 0; d < dim; ++d)
        {
          // Update acceleration
          particle_acceleration[d] =
            g[d] + (force[particle_id][d]) /
                     particle_properties[PropertiesIndex::mass];

          // Half-step velocity
          particle_properties[PropertiesIndex::v_x + d] +=
            0.5 * particle_acceleration[d] * dt;

          // Half-step angular velocity
          particle_properties[PropertiesIndex::omega_x + d] +=
            0.5 * (torque[particle_id][d] / MOI[particle_id]) * dt;

          // Update particle position using half-step velocity
          particle_position[d] +=
            particle_properties[PropertiesIndex::v_x + d] * dt;
        }
      particle->set_location(particle_position);
    }
}

template <int dim, typename PropertiesIndex>
void
VelocityVerletIntegrator<dim, PropertiesIndex>::integrate(
  Particles::ParticleHandler<dim> &particle_handler,
  const Tensor<1, 3>              &g,
  const double                     dt,
  std::vector<Tensor<1, 3>>       &torque,
  std::vector<Tensor<1, 3>>       &force,
  const std::vector<double>       &MOI)
{
  Point<3>           particle_position;
  const Tensor<1, 3> dt_g = g * dt;

  for (auto &particle : particle_handler)
    {
      // Get the total array view to the particle properties once to improve
      // efficiency
      types::particle_index particle_id = particle.get_local_index();

      auto          particle_properties = particle.get_properties();
      Tensor<1, 3> &particle_torque     = torque[particle_id];
      Tensor<1, 3> &particle_force      = force[particle_id];

      double dt_mass_inverse = dt / particle_properties[PropertiesIndex::mass];
      double dt_MOI_inverse  = dt / MOI[particle_id];

      particle_position = [&] {
        if constexpr (dim == 3)
          {
            return particle.get_location();
          }
        else
          {
            return (point_nd_to_3d(particle.get_location()));
          }
      }();

      // Loop is manually unrolled for performance reason.
      // for (int d = 0; d < 3; ++d)
      {
        // Particle velocity integration
        particle_properties[PropertiesIndex::v_x] +=
          dt_g[0] + particle_force[0] * dt_mass_inverse;
        particle_properties[PropertiesIndex::v_y] +=
          dt_g[1] + particle_force[1] * dt_mass_inverse;
        particle_properties[PropertiesIndex::v_z] +=
          dt_g[2] + particle_force[2] * dt_mass_inverse;


        // Particle location integration
        particle_position[0] += particle_properties[PropertiesIndex::v_x] * dt;
        particle_position[1] += particle_properties[PropertiesIndex::v_y] * dt;
        particle_position[2] += particle_properties[PropertiesIndex::v_z] * dt;

        // Updating angular velocity
        particle_properties[PropertiesIndex::omega_x] +=
          particle_torque[0] * dt_MOI_inverse;
        particle_properties[PropertiesIndex::omega_y] +=
          particle_torque[1] * dt_MOI_inverse;
        particle_properties[PropertiesIndex::omega_z] +=
          particle_torque[2] * dt_MOI_inverse;
      }

      // Reinitialize force and torque of particle
      particle_force  = 0;
      particle_torque = 0;

      if constexpr (dim == 3)
        particle.set_location(particle_position);

      if constexpr (dim == 2)
        {
          Point<2> position_2d;
          position_2d[0] = particle_position[0];
          position_2d[1] = particle_position[1];
          particle.set_location(position_2d);
        }
    }
}

template <int dim, typename PropertiesIndex>
void
VelocityVerletIntegrator<dim, PropertiesIndex>::integrate(
  Particles::ParticleHandler<dim>                 &particle_handler,
  const Tensor<1, 3>                              &g,
  const double                                     dt,
  std::vector<Tensor<1, 3>>                       &torque,
  std::vector<Tensor<1, 3>>                       &force,
  const std::vector<double>                       &MOI,
  const parallel::distributed::Triangulation<dim> &triangulation,
  AdaptiveSparseContacts<dim, PropertiesIndex>    &sparse_contacts_object)
{
  auto *action_manager = DEMActionManager::get_action_manager();

  bool use_default_function =
    !action_manager->check_sparse_contacts_enabled() ||
    action_manager->check_mobility_status_reset();

  // Regular integration process if sparse contacts are not enabled or if this
  // is the first iteration.
  if (use_default_function)
    {
      integrate(particle_handler, g, dt, torque, force, MOI);
      return;
    }

  // If there are advected particles, we use another function since the average
  // velocity and acceleration of cells are computed for mobile cells
  if (sparse_contacts_object.has_advected_particles())
    {
      integrate_with_advected_particles(particle_handler,
                                        g,
                                        dt,
                                        torque,
                                        force,
                                        MOI,
                                        triangulation,
                                        sparse_contacts_object);
    }
  else
    {
      Point<3>           particle_position;
      const Tensor<1, 3> dt_g = g * dt;

      // Get the map of mobility status of cells
      auto &cell_mobility_status_map =
        sparse_contacts_object.get_mobility_status();

      for (auto &cell : triangulation.active_cell_iterators())
        {
          if (cell->is_locally_owned())
            {
              // Get the mobility status of the cell
              unsigned int mobility_status =
                cell_mobility_status_map.at(cell->active_cell_index());

              // We loop over the particles, even if cell is not mobile, to
              // reset the force and torques value of the particle with the
              // particle id
              auto particles_in_cell = particle_handler.particles_in_cell(cell);
              unsigned int n_particles_in_cell =
                particle_handler.n_particles_in_cell(cell);

              if (n_particles_in_cell > 0)
                {
                  if (mobility_status ==
                      AdaptiveSparseContacts<dim, PropertiesIndex>::mobile)
                    {
                      for (auto &particle : particles_in_cell)
                        {
                          types::particle_index particle_id =
                            particle.get_local_index();
                          auto particle_properties = particle.get_properties();

                          Tensor<1, 3> &particle_torque = torque[particle_id];
                          Tensor<1, 3> &particle_force  = force[particle_id];

                          double dt_mass_inverse =
                            dt / particle_properties[PropertiesIndex::mass];
                          double dt_MOI_inverse = dt / MOI[particle_id];

                          particle_position = [&] {
                            if constexpr (dim == 3)
                              {
                                return particle.get_location();
                              }
                            else
                              {
                                return (
                                  point_nd_to_3d(particle.get_location()));
                              }
                          }();

                          for (unsigned int d = 0; d < 3; ++d)
                            {
                              // Particle velocity integration
                              particle_properties[PropertiesIndex::v_x + d] +=
                                dt_g[d] + particle_force[d] * dt_mass_inverse;

                              // Particle location integration
                              particle_position[d] +=
                                particle_properties[PropertiesIndex::v_x + d] *
                                dt;

                              // Updating angular velocity
                              particle_properties[PropertiesIndex::omega_x +
                                                  d] +=
                                particle_torque[d] * dt_MOI_inverse;
                            }

                          // Reinitialize force and torque of particle
                          particle_force  = 0.0;
                          particle_torque = 0.0;

                          // Update particle location
                          if constexpr (dim == 3)
                            particle.set_location(particle_position);

                          if constexpr (dim == 2)
                            {
                              Point<2> position_2d;
                              position_2d[0] = particle_position[0];
                              position_2d[1] = particle_position[1];
                              particle.set_location(position_2d);
                            }
                        }
                    }
                  else // Active and inactive cells
                    {
                      for (auto &particle : particles_in_cell)
                        {
                          types::particle_index particle_id =
                            particle.get_local_index();

                          // Reset forces
                          force[particle_id]  = 0.0;
                          torque[particle_id] = 0.0;
                        }
                    }
                }
            }
        }
    }
}

template <int dim, typename PropertiesIndex>
void
VelocityVerletIntegrator<dim, PropertiesIndex>::
  integrate_with_advected_particles(
    Particles::ParticleHandler<dim>                 &particle_handler,
    const Tensor<1, 3>                              &g,
    const double                                     dt,
    std::vector<Tensor<1, 3>>                       &torque,
    std::vector<Tensor<1, 3>>                       &force,
    const std::vector<double>                       &MOI,
    const parallel::distributed::Triangulation<dim> &triangulation,
    AdaptiveSparseContacts<dim, PropertiesIndex>    &sparse_contacts_object)
{
  Point<3>           particle_position;
  const Tensor<1, 3> dt_g = g * dt;

  // Get the map of average velocities and accelerations of cells
  auto cell_velocities_accelerations_map =
    sparse_contacts_object.get_velocities_accelerations();

  // Get the map of mobility status of cells
  auto cell_mobility_status_map = sparse_contacts_object.get_mobility_status();

  for (auto &cell : triangulation.active_cell_iterators())
    {
      if (cell->is_locally_owned())
        {
          // Get the mobility status of the cell
          unsigned int mobility_status =
            cell_mobility_status_map.at(cell->active_cell_index());

          // Get the average velocity and acceleration * dt of the cell
          auto &[velocity_cell_average, acc_dt_cell_average] =
            cell_velocities_accelerations_map[cell];

          // We loop over the particles, even if cell is not mobile, to reset
          // the force and torques value of the particle with the particle id
          auto particles_in_cell = particle_handler.particles_in_cell(cell);
          unsigned int n_particles_in_cell =
            particle_handler.n_particles_in_cell(cell);

          if (n_particles_in_cell > 0)
            {
              if (mobility_status ==
                  AdaptiveSparseContacts<dim, PropertiesIndex>::mobile)
                {
                  // When mobile, the average velocity of the cell and the
                  // acceleration is updated at the same step that the particles
                  // are integrated of performance reasons
                  velocity_cell_average.clear();
                  acc_dt_cell_average.clear();

                  for (auto &particle : particles_in_cell)
                    {
                      types::particle_index particle_id =
                        particle.get_local_index();
                      auto particle_properties = particle.get_properties();

                      Tensor<1, 3> &particle_torque = torque[particle_id];
                      Tensor<1, 3> &particle_force  = force[particle_id];

                      double dt_mass_inverse =
                        dt / particle_properties[PropertiesIndex::mass];
                      double dt_MOI_inverse = dt / MOI[particle_id];

                      particle_position = [&] {
                        if constexpr (dim == 3)
                          {
                            return particle.get_location();
                          }
                        else
                          {
                            return (point_nd_to_3d(particle.get_location()));
                          }
                      }();

                      // Calculate acceleration * dt and add velocity value for
                      // average acceleration computation
                      Tensor<1, 3> acc_dt_particle =
                        dt_g + particle_force * dt_mass_inverse;
                      acc_dt_cell_average += acc_dt_particle;

                      for (unsigned int d = 0; d < 3; ++d)
                        {
                          // Particle velocity integration
                          particle_properties[PropertiesIndex::v_x + d] +=
                            acc_dt_particle[d];

                          // Add velocity value for average velocity computation
                          velocity_cell_average[d] +=
                            particle_properties[PropertiesIndex::v_x + d];

                          // Particle location integration
                          particle_position[d] +=
                            particle_properties[PropertiesIndex::v_x + d] * dt;

                          // Updating angular velocity
                          particle_properties[PropertiesIndex::omega_x + d] +=
                            particle_torque[d] * dt_MOI_inverse;
                        }

                      // Reinitialize force and torque of particle
                      particle_force  = 0.0;
                      particle_torque = 0.0;

                      // Update particle location
                      if constexpr (dim == 3)
                        particle.set_location(particle_position);

                      if constexpr (dim == 2)
                        {
                          Point<2> position_2d;
                          position_2d[0] = particle_position[0];
                          position_2d[1] = particle_position[1];
                          particle.set_location(position_2d);
                        }
                    }

                  // Compute average velocity and acceleration
                  acc_dt_cell_average /= n_particles_in_cell;
                  velocity_cell_average /= n_particles_in_cell;
                }
              else if (mobility_status ==
                         AdaptiveSparseContacts<dim,
                                                PropertiesIndex>::advected ||
                       mobility_status ==
                         AdaptiveSparseContacts<dim, PropertiesIndex>::
                           advected_active)
                {
                  for (auto &particle : particles_in_cell)
                    {
                      types::particle_index particle_id =
                        particle.get_local_index();

                      particle_position = [&] {
                        if constexpr (dim == 3)
                          {
                            return particle.get_location();
                          }
                        else
                          {
                            return (point_nd_to_3d(particle.get_location()));
                          }
                      }();

                      // Update the new average velocity of the cell : v + a*dt
                      velocity_cell_average += acc_dt_cell_average;

                      // Particle location integration with the average velocity
                      for (unsigned int d = 0; d < 3; ++d)
                        {
                          particle_position[d] += velocity_cell_average[d] * dt;
                        }

                      // Reset forces
                      force[particle_id]  = 0.0;
                      torque[particle_id] = 0.0;

                      // Update particle location
                      if constexpr (dim == 3)
                        particle.set_location(particle_position);

                      if constexpr (dim == 2)
                        {
                          Point<2> position_2d;
                          position_2d[0] = particle_position[0];
                          position_2d[1] = particle_position[1];
                          particle.set_location(position_2d);
                        }
                    }
                }
              else // Static active and inactive cells (no particles
                   // integration)
                {
                  for (auto &particle : particles_in_cell)
                    {
                      types::particle_index particle_id =
                        particle.get_local_index();

                      // Reset forces
                      force[particle_id]  = 0.0;
                      torque[particle_id] = 0.0;
                    }
                }
            }
        }
    }
}


template class VelocityVerletIntegrator<2, DEM::DEMProperties::PropertiesIndex>;
template class VelocityVerletIntegrator<2,
                                        DEM::CFDDEMProperties::PropertiesIndex>;
template class VelocityVerletIntegrator<2,
                                        DEM::DEMMPProperties::PropertiesIndex>;
template class VelocityVerletIntegrator<3, DEM::DEMProperties::PropertiesIndex>;
template class VelocityVerletIntegrator<3,
                                        DEM::CFDDEMProperties::PropertiesIndex>;
template class VelocityVerletIntegrator<3,
                                        DEM::DEMMPProperties::PropertiesIndex>;
