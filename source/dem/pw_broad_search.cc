#include <dem/pw_broad_search.h>

using namespace dealii;

template <int dim>
PWBroadSearch<dim>::PWBroadSearch()
{}

template <int dim>
void
PWBroadSearch<dim>::find_PW_Contact_Pairs(
  std::map<int, boundary_cells_info_struct<dim>> &boundary_cells_information,
  Particles::ParticleHandler<dim> &               particle_handler,
  std::unordered_map<
    int,
    std::unordered_map<
      int,
      std::tuple<Particles::ParticleIterator<dim>, Tensor<1, dim>, Point<dim>>>>
    &pw_contact_candidates)
{
  // Since the pw_contact_candidates (which is the real output of the
  // function) is defined as an input of the function, it should be cleared
  pw_contact_candidates.clear();

  // Iterating over the boundary_cells_information vector which is the output of
  // the find_boundary_cells_information find_boundary_cells_information class.
  // This vector contains all the required information of the system boundary
  // cells and faces. In this loop we find the particles located in each of
  // these boundary cells
  for (auto boundary_cells_information_iterator =
         boundary_cells_information.begin();
       boundary_cells_information_iterator != boundary_cells_information.end();
       ++boundary_cells_information_iterator)
    {
      auto boundary_cells_content = boundary_cells_information_iterator->second;
      auto cell                   = boundary_cells_content.cell;

      // Finding particles located in the corresponding cell
      // (boundary_cells_content.cell)
      typename Particles::ParticleHandler<dim>::particle_iterator_range
        particles_in_cell = particle_handler.particles_in_cell(cell);

      const bool particles_exist_in_main_cell = !particles_in_cell.empty();

      // If the main cell is not empty
      if (particles_exist_in_main_cell)
        {
          for (typename Particles::ParticleHandler<dim>::
                 particle_iterator_range::iterator particles_in_cell_iterator =
                   particles_in_cell.begin();
               particles_in_cell_iterator != particles_in_cell.end();
               ++particles_in_cell_iterator)
            {
              // Making the tuple and adding it to the pw_contact_candidates
              // vector. This vector is the output of this function

              std::tuple map_content =
                std::make_tuple(particles_in_cell_iterator,
                                boundary_cells_content.normal_vector,
                                boundary_cells_content.point_on_face);

              pw_contact_candidates[particles_in_cell_iterator->get_id()]
                .insert({boundary_cells_content.boundary_face_id, map_content});
            }
        }
    }
}

template class PWBroadSearch<2>;
template class PWBroadSearch<3>;