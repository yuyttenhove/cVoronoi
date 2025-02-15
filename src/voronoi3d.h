//
// Created by yuyttenh on 08/06/2021.
//

/**
 * @file voronoi3d.h
 *
 * @brief 3D Voronoi grid and functions.
 */

#ifndef CVORONOI_VORONOI3D_H
#define CVORONOI_VORONOI3D_H

#include "queues.h"
#include "tuples.h"

/**
 * @brief Voronoi interface.
 *
 * An interface is a connection between two neighbouring Voronoi cells. It is
 * completely defined by the indices of the generators that generate the two
 * neighbouring cells, a surface area and a midpoint position.
 */
struct voronoi_pair {
  /*! Pointer to particle corresponding to the generator on the left of the
   * interface (always a particle within the local cell). */
  int left;

  /*! Pointer to particle corresponding to the generator on the right of the
   * interface (can be a local particle, but also a particle in a
   * neighbouring cell). */
  int right;

  struct cell *right_cell;

  /*! Surface area of the interface. */
  double surface_area;

  /*! Midpoint of the interface. */
  double midpoint[3];

#ifdef VORONOI_STORE_CONNECTIONS
  /*! Vertices of the interface. */
  double *vertices;

  /*! Number of vertices of this face. */
  int n_vertices;
#endif
};

/**
 * @brief Initializes a voronoi pair (i.e. a face). The vertices of the face are
 * only stored if VORONOI_STORE_CONNECTIONS is defined.
 *
 * @param pair Voronoi pair to initialize
 * @param c Pointer to the SWIFT cell in which the right particle lives
 * (NULL if this is the same cell as the left particle)
 * @param left_part_pointer Pointer to the left particle of this pair
 * @param right_part_pointer Pointer to the right particle of this pair
 * @param vertices Vertices making up the face
 * @param n_vertices Number of vertices in the vertices array.
 */
inline static void voronoi_pair_init(struct voronoi_pair *pair,
                                     struct cell *restrict c,
                                     int left_part_pointer,
                                     int right_part_pointer, double *vertices,
                                     int n_vertices) {
  pair->right_cell = c;
  pair->left = left_part_pointer;
  pair->right = right_part_pointer;

  pair->surface_area =
      geometry3d_compute_centroid_area(vertices, n_vertices, pair->midpoint);

#ifdef VORONOI_STORE_CONNECTIONS
  pair->vertices = (double *)malloc(3 * n_vertices * sizeof(double));
  pair->n_vertices = n_vertices;
  for (int i = 0; i < n_vertices; i++) {
    pair->vertices[3 * i] = vertices[3 * i];
    pair->vertices[3 * i + 1] = vertices[3 * i + 1];
    pair->vertices[3 * i + 2] = vertices[3 * i + 2];
  }
#endif
}

/**
 * @brief Free up memory allocated by a voronoi pair (only necessary if
 * VORONOI_STORE_CONNECTIONS is defined).
 *
 * @param pair
 */
inline static void voronoi_pair_destroy(struct voronoi_pair *pair) {
#ifdef VORONOI_STORE_CONNECTIONS
  free(pair->vertices);
#endif
}

/**
 * @brief Voronoi cell.
 *
 * A cell stores geometrical information about a Voronoi cell: its volume and
 * the location of its centroid.
 */
struct voronoi_cell {
  /*! Cell volume. */
  double volume;

  /*! Cell centroid. */
  double centroid[3];

#ifdef VORONOI_STORE_GENERATORS
  /*! Position of the cell generator. */
  double generator[3];
#endif

#ifdef VORONOI_STORE_CELL_STATS
  /*! Number of faces of this cell. */
  int nface;
#endif
};

struct voronoi {
  /*! @brief Voronoi cells. */
  struct voronoi_cell *cells;

  /*! @brief Number of cells. */
  int number_of_cells;

  /*! @brief Voronoi cell pairs. We store these per (SWIFT) cell, i.e. pairs[0]
   *  contains all pairs that are completely contained within this cell, while
   *  pairs[1] corresponds to pairs crossing the boundary between this cell and
   *  the cell with coordinates that are lower in all coordinate directions (the
   *  cell to the left, front, bottom, sid=0), and so on. */
  struct voronoi_pair *pairs[2];

  /*! @brief Current number of pairs per cell index. */
  int pair_index[2];

  /*! @brief Allocated number of pairs per cell index. */
  int pair_size[2];
};

/* Forward declarations */
inline static int double_cmp(double double1, double double2,
                             unsigned long precision);
inline static int voronoi_new_face(struct voronoi *v, int sid,
                                   struct cell *restrict c,
                                   int left_part_pointer,
                                   int right_part_pointer, double *vertices,
                                   int n_vertices);
inline static void voronoi_check_grid(struct voronoi *restrict v);

/**
 * @brief Initialise the Voronoi grid based on the given Delaunay tessellation.
 *
 * This function allocates the memory for the Voronoi grid arrays and creates
 * the grid in linear time by
 *  1. Computing the grid vertices as the midpoints of the circumcircles of the
 *     Delaunay tetrahedra.
 *  2. Looping over all vertices and for each generator looping over all
 *     tetrahedra that link to that vertex. This is done by looping around all
 *     the Delaunay edges connected to that generator. While looping around an
 *     edge, for each tetrahedron, we add the edges of that tetrahedron which
 *     are connected to the current generator to a queue of next edges to loop
 *     around (if we did not already do so).
 *
 * During the second step, the geometrical properties (cell centroid, volume
 * and face midpoint, area) are computed as well.
 *
 * @param v Voronoi grid.
 * @param d Delaunay tessellation (read-only).
 */
inline static void voronoi_init(struct voronoi *restrict v,
                                struct delaunay *restrict d) {
  delaunay_assert(d->vertex_end > 0);

  /* the number of cells equals the number of non-ghost and non-dummy
     vertex_indices in the Delaunay tessellation */
  v->number_of_cells = d->vertex_end - d->vertex_start;
  /* allocate memory for the voronoi cells */
  v->cells = (struct voronoi_cell *)malloc(v->number_of_cells *
                                           sizeof(struct voronoi_cell));
  /* Allocate memory to store voronoi vertices (will be freed at end) */
  double *voronoi_vertices =
      (double *)malloc(3 * (d->tetrahedron_index - 4) * sizeof(double));

  /* loop over the tetrahedra in the Delaunay tessellation and compute the
     midpoints of their circumspheres. These happen to be the vertices of
     the Voronoi grid (because they are the points of equal distance to 3
     generators, while the Voronoi edges are the lines of equal distance to 2
     generators) */
  for (int i = 0; i < d->tetrahedron_index - 4; i++) {
    struct tetrahedron *t = &d->tetrahedra[i + 4];
    /* Get the indices of the vertices of the tetrahedron */
    int v0 = t->vertices[0];
    int v1 = t->vertices[1];
    int v2 = t->vertices[2];
    int v3 = t->vertices[3];

    /* if the tetrahedron is inactive or not linked to a non-ghost, non-dummy
     * vertex, it is not a grid vertex and we can skip it. */
    if (!t->active || (v0 >= v->number_of_cells && v1 >= v->number_of_cells &&
        v2 >= v->number_of_cells && v3 >= v->number_of_cells)) {
      voronoi_vertices[3 * i] = NAN;
      voronoi_vertices[3 * i + 1] = NAN;
      voronoi_vertices[3 * i + 2] = NAN;
      continue;
    }
    /* Check that the vertices are valid */
    voronoi_assert(v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0);

    /* Extract coordinates from the Delaunay vertices (generators)
     * FUTURE NOTE: In swift we should read this from the particles themselves!
     * */
    double v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z;
    if (v0 < d->vertex_end || v0 >= d->ghost_offset) {
      v0x = d->vertices[3 * v0];
      v0y = d->vertices[3 * v0 + 1];
      v0z = d->vertices[3 * v0 + 2];
    } else {
      /* This could mean that a neighbouring cell of this grids cell is empty!
       * Or that we did not add all the necessary ghost vertex_indices to the
       * delaunay tesselation. */
      voronoi_error(
          "Vertex is part of tetrahedron with Dummy vertex! This could mean "
          "that one of the neighbouring cells is empty.");
    }
    if (v1 < d->vertex_end || v1 >= d->ghost_offset) {
      v1x = d->vertices[3 * v1];
      v1y = d->vertices[3 * v1 + 1];
      v1z = d->vertices[3 * v1 + 2];
    } else {
      voronoi_error(
          "Vertex is part of tetrahedron with Dummy vertex! This could mean "
          "that one of the neighbouring cells is empty.");
    }
    if (v2 < d->vertex_end || v2 >= d->ghost_offset) {
      v2x = d->vertices[3 * v2];
      v2y = d->vertices[3 * v2 + 1];
      v2z = d->vertices[3 * v2 + 2];
    } else {
      voronoi_error(
          "Vertex is part of tetrahedron with Dummy vertex! This could mean "
          "that "
          "one of the neighbouring cells is empty.");
    }
    if (v3 < d->vertex_end || v3 >= d->ghost_offset) {
      v3x = d->vertices[3 * v3];
      v3y = d->vertices[3 * v3 + 1];
      v3z = d->vertices[3 * v3 + 2];
    } else {
      voronoi_error(
          "Vertex is part of tetrahedron with Dummy vertex! This could mean "
          "that one of the neighbouring cells is empty.");
    }

    geometry3d_compute_circumcenter(v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z,
                                    v3x, v3y, v3z, &voronoi_vertices[3 * i]);
#ifdef VORONOI_CHECKS
    const double cx = voronoi_vertices[3 * i];
    const double cy = voronoi_vertices[3 * i + 1];
    const double cz = voronoi_vertices[3 * i + 2];

    const double r0 = (cx - v0x) * (cx - v0x) + (cy - v0y) * (cy - v0y) +
                      (cz - v0z) * (cz - v0z);
    const double r1 = (cx - v1x) * (cx - v1x) + (cy - v1y) * (cy - v1y) +
                      (cz - v1z) * (cz - v1z);
    const double r2 = (cx - v2x) * (cx - v2x) + (cy - v2y) * (cy - v2y) +
                      (cz - v2z) * (cz - v2z);
    const double r3 = (cx - v3x) * (cx - v3x) + (cy - v3y) * (cy - v3y) +
                      (cz - v3z) * (cz - v3z);
    voronoi_assert(double_cmp(r0, r1, 1e10) && double_cmp(r0, r2, 1e10) &&
                   double_cmp(r0, r3, 1e10));
#endif
  } /* loop over the Delaunay tetrahedra and compute the circumcenters */

  /* Allocate memory for the voronoi pairs (faces). */
  for (int i = 0; i < 2; ++i) {
    v->pairs[i] =
        (struct voronoi_pair *)malloc(10 * sizeof(struct voronoi_pair));
    v->pair_index[i] = 0;
    v->pair_size[i] = 10;
  }

  /* Allocate memory for the neighbour flags and initialize them to 0 */
  int *neighbour_flags = (int *)malloc(d->vertex_index * sizeof(int));
  for (int i = 0; i < d->vertex_index; i++) {
    neighbour_flags[i] = 0;
  }

  /* Allocate a tetrahedron_vertex_queue */
  struct int3_fifo_queue neighbour_info_q;
  int3_fifo_queue_init(&neighbour_info_q, 10);

  /* The size of the array used to temporarily store the vertices of the voronoi
   * faces in */
  int face_vertices_size = 10;
  /* Temporary array to store face vertices in */
  double *face_vertices =
      (double *)malloc(3 * face_vertices_size * sizeof(double));

  /* loop over all cell generators, and hence over all non-ghost, non-dummy
     Delaunay vertex_indices */
  for (int gen_idx_in_d = 0; gen_idx_in_d < v->number_of_cells;
       gen_idx_in_d++) {
    /* First reset the tetrahedron_vertex_queue */
    int3_fifo_queue_reset(&neighbour_info_q);
    /* Set the flag of the central generator so that we never pick it as
     * possible neighbour */
    neighbour_flags[gen_idx_in_d] = 1;

    /* Create a new voronoi cell for this generator */
    struct voronoi_cell *this_cell = &v->cells[gen_idx_in_d - d->vertex_start];
    this_cell->volume = 0.;
    this_cell->centroid[0] = 0.;
    this_cell->centroid[1] = 0.;
    this_cell->centroid[2] = 0.;
    int nface = 0;

    /* get the generator position, we use it during centroid/volume
       calculations */
    voronoi_assert(gen_idx_in_d < d->vertex_end);
    double ax = d->vertices[3 * gen_idx_in_d];
    double ay = d->vertices[3 * gen_idx_in_d + 1];
    double az = d->vertices[3 * gen_idx_in_d + 2];

#ifdef VORONOI_STORE_GENERATORS
    this_cell->generator[0] = ax;
    this_cell->generator[1] = ay;
    this_cell->generator[2] = az;
#endif

    /* Get a tetrahedron containing the central generator */
    int t_idx = d->vertex_tetrahedron_links[gen_idx_in_d];
    int gen_idx_in_t = d->vertex_tetrahedron_index[gen_idx_in_d];

    /* Pick another vertex (generator) from this tetrahedron and add it to the
     * queue */
    int other_v_idx_in_t = (gen_idx_in_t + 1) % 4;
    struct tetrahedron *t = &d->tetrahedra[t_idx];
    int other_v_idx_in_d = t->vertices[other_v_idx_in_t];
    int3 info = {._0 = t_idx, ._1 = other_v_idx_in_d, ._2 = other_v_idx_in_t};
    int3_fifo_queue_push(&neighbour_info_q, info);
    /* update flag of the other vertex */
    neighbour_flags[other_v_idx_in_d] = 1;

    while (!int3_fifo_queue_is_empty(&neighbour_info_q)) {
      /* with each delaunay edge corresponds a voronoi face */
      nface++;

      /* Pop the next axis vertex and corresponding tetrahedron from the queue
       */
      info = int3_fifo_queue_pop(&neighbour_info_q);
      int first_t_idx = info._0;
      int axis_idx_in_d = info._1;
      int axis_idx_in_t = info._2;
      voronoi_assert(axis_idx_in_d >= 0 && (axis_idx_in_d < d->vertex_end ||
                                            axis_idx_in_d >= d->ghost_offset));
      struct tetrahedron *first_t = &d->tetrahedra[first_t_idx];

      /* Get a non axis vertex from first_t */
      int non_axis_idx_in_first_t = (axis_idx_in_t + 1) % 4;
      if (first_t->vertices[non_axis_idx_in_first_t] == gen_idx_in_d) {
        non_axis_idx_in_first_t = (non_axis_idx_in_first_t + 1) % 4;
      }
      int non_axis_idx_in_d = first_t->vertices[non_axis_idx_in_first_t];

      if (!neighbour_flags[non_axis_idx_in_d]) {
        /* Add this vertex and tetrahedron to the queue and update its flag */
        int3 new_info = {._0 = first_t_idx,
                         ._1 = non_axis_idx_in_d,
                         ._2 = non_axis_idx_in_first_t};
        int3_fifo_queue_push(&neighbour_info_q, new_info);
        neighbour_flags[non_axis_idx_in_d] |= 1;
      }

      /* Get a neighbouring tetrahedron of first_t sharing the axis */
      int cur_t_idx = first_t->neighbours[non_axis_idx_in_first_t];
      struct tetrahedron *cur_t = &d->tetrahedra[cur_t_idx];
      int prev_t_idx_in_cur_t =
          first_t->index_in_neighbour[non_axis_idx_in_first_t];

      /* Get a neighbouring tetrahedron of cur_t that is not first_t, sharing
       * the same axis */
      int next_t_idx_in_cur_t = (prev_t_idx_in_cur_t + 1) % 4;
      while (cur_t->vertices[next_t_idx_in_cur_t] == gen_idx_in_d ||
             cur_t->vertices[next_t_idx_in_cur_t] == axis_idx_in_d) {
        next_t_idx_in_cur_t = (next_t_idx_in_cur_t + 1) % 4;
      }
      int next_t_idx = cur_t->neighbours[next_t_idx_in_cur_t];

      /* Get the next non axis vertex and add it to the queue if necessary */
      int next_non_axis_idx_in_d = cur_t->vertices[next_t_idx_in_cur_t];
      if (!neighbour_flags[next_non_axis_idx_in_d]) {
        int3 new_info = {._0 = cur_t_idx,
                         ._1 = next_non_axis_idx_in_d,
                         ._2 = next_t_idx_in_cur_t};
        int3_fifo_queue_push(&neighbour_info_q, new_info);
        neighbour_flags[next_non_axis_idx_in_d] |= 1;
      }

      /* Get the coordinates of the voronoi vertex of the new face */
      int vor_vertex0_idx = first_t_idx - 4;
      face_vertices[0] = voronoi_vertices[3 * vor_vertex0_idx];
      face_vertices[1] = voronoi_vertices[3 * vor_vertex0_idx + 1];
      face_vertices[2] = voronoi_vertices[3 * vor_vertex0_idx + 2];
      int face_vertices_index = 1;

      /* Loop around the axis */
      while (next_t_idx != first_t_idx) {
        /* Get the coordinates of the voronoi vertex corresponding to cur_t and
         * next_t */
        if (face_vertices_index + 6 > face_vertices_size) {
          face_vertices_size <<= 1;
          face_vertices = (double *)realloc(
              face_vertices, 3 * face_vertices_size * sizeof(double));
        }
        const int vor_vertex1_idx = cur_t_idx - 4;
        face_vertices[3 * face_vertices_index] =
            voronoi_vertices[3 * vor_vertex1_idx];
        face_vertices[3 * face_vertices_index + 1] =
            voronoi_vertices[3 * vor_vertex1_idx + 1];
        face_vertices[3 * face_vertices_index + 2] =
            voronoi_vertices[3 * vor_vertex1_idx + 2];
        const int vor_vertex2_idx = next_t_idx - 4;
        face_vertices[3 * face_vertices_index + 3] =
            voronoi_vertices[3 * vor_vertex2_idx];
        face_vertices[3 * face_vertices_index + 4] =
            voronoi_vertices[3 * vor_vertex2_idx + 1];
        face_vertices[3 * face_vertices_index + 5] =
            voronoi_vertices[3 * vor_vertex2_idx + 2];
        face_vertices_index += 2;

        /* Update cell volume and tetrahedron_centroid */
        double tetrahedron_centroid[3];
        const double V = geometry3d_compute_centroid_volume_tetrahedron(
            ax, ay, az, face_vertices[0], face_vertices[1], face_vertices[2],
            face_vertices[3 * face_vertices_index - 6],
            face_vertices[3 * face_vertices_index - 5],
            face_vertices[3 * face_vertices_index - 4],
            face_vertices[3 * face_vertices_index - 3],
            face_vertices[3 * face_vertices_index - 2],
            face_vertices[3 * face_vertices_index - 1], tetrahedron_centroid);
        this_cell->volume += V;
        this_cell->centroid[0] += V * tetrahedron_centroid[0];
        this_cell->centroid[1] += V * tetrahedron_centroid[1];
        this_cell->centroid[2] += V * tetrahedron_centroid[2];

        /* Update variables */
        prev_t_idx_in_cur_t = cur_t->index_in_neighbour[next_t_idx_in_cur_t];
        cur_t_idx = next_t_idx;
        cur_t = &d->tetrahedra[cur_t_idx];
        next_t_idx_in_cur_t = (prev_t_idx_in_cur_t + 1) % 4;
        while (cur_t->vertices[next_t_idx_in_cur_t] == gen_idx_in_d ||
               cur_t->vertices[next_t_idx_in_cur_t] == axis_idx_in_d) {
          next_t_idx_in_cur_t = (next_t_idx_in_cur_t + 1) % 4;
        }
        next_t_idx = cur_t->neighbours[next_t_idx_in_cur_t];
        /* Get the next non axis vertex and add it to the queue if necessary */
        next_non_axis_idx_in_d = cur_t->vertices[next_t_idx_in_cur_t];
        if (!neighbour_flags[next_non_axis_idx_in_d]) {
          int3 new_info = {._0 = cur_t_idx,
                           ._1 = next_non_axis_idx_in_d,
                           ._2 = next_t_idx_in_cur_t};
          int3_fifo_queue_push(&neighbour_info_q, new_info);
          neighbour_flags[next_non_axis_idx_in_d] |= 1;
        }
      }
      if (axis_idx_in_d < d->vertex_end) {
        /* Store faces only once */
        if (gen_idx_in_d < axis_idx_in_d) {
          voronoi_new_face(v, 0, NULL, gen_idx_in_d, axis_idx_in_d,
                           face_vertices, face_vertices_index);
        }
      } else { /* axis_idx_in_d >= d->ghost_offset */
        voronoi_new_face(v, 1, NULL, gen_idx_in_d, axis_idx_in_d, face_vertices,
                         face_vertices_index);
      }
    }
    this_cell->centroid[0] /= this_cell->volume;
    this_cell->centroid[1] /= this_cell->volume;
    this_cell->centroid[2] /= this_cell->volume;
#ifdef VORONOI_STORE_CELL_STATS
    this_cell->nface = nface;
#endif
    /* reset flags for all neighbours of this cell */
    neighbour_flags[gen_idx_in_d] = 0;
    for (int i = 0; i < neighbour_info_q.end; i++) {
      voronoi_assert(neighbour_info_q.values[i]._1 < d->vertex_index)
          neighbour_flags[neighbour_info_q.values[i]._1] = 0;
    }
#ifdef VORONOI_CHECKS
    for (int i = 0; i < d->vertex_index; i++) {
      voronoi_assert(neighbour_flags[i] == 0);
    }
#endif
  }
  free(voronoi_vertices);
  free(neighbour_flags);
  int3_fifo_queue_destroy(&neighbour_info_q);
  free(face_vertices);
  voronoi_check_grid(v);
}

/**
 * @brief Free up all memory used by the Voronoi grid.
 *
 * @param v Voronoi grid.
 */
inline static void voronoi_destroy(struct voronoi *restrict v) {
  free(v->cells);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < v->pair_index[i]; j++) {
      voronoi_pair_destroy(&v->pairs[i][j]);
    }
    free(v->pairs[i]);
  }
}

/**
 * @brief Add a face (two particle pair) to the mesh.
 *
 * The grid connectivity is stored per cell sid: sid=13 corresponds to particle
 * pairs encountered during a self task (both particles are within the local
 * cell), while sid=0-12 and 14-26 correspond to particle interactions for which
 * the right neighbour is part of one of the 26 neighbouring cells.
 *
 * For each pair, we compute and store all the quantities required to compute
 * fluxes between the Voronoi cells: the surface area and midpoint of the
 * interface.
 *
 * @param v Voronoi grid.
 * @param sid 0 for pairs entirely in this cell, 1 for pairs between this cell
 * and a neighbouring cell (in SWIFT we use the convention from the
 * description).
 * @param cell Pointer to the cell of the right particle (NULL if the right
 * particle lives in the same cell as the left particle). For SWIFT only.
 * @param left_part_pointer Index of left particle in cell (particle in the
 * cell linked to this grid). FUTURE NOTE: For SWIFT, replace this with direct
 * pointer to the left particle.
 * @param right_part_pointer Index of right particle in cell (particle in the
 * cell linked to this grid), or -1 for ghost vertices. FUTURE NOTE: For SWIFT,
 * replace this with direct pointer to the right particle.
 * @param vertices Vertices of the interface.
 * @param n_vertices Number of vertices in the vertices array.
 */
inline static int voronoi_new_face(struct voronoi *v, int sid,
                                   struct cell *restrict c,
                                   int left_part_pointer,
                                   int right_part_pointer, double *vertices,
                                   int n_vertices) {
  if (v->pair_index[sid] == v->pair_size[sid]) {
    v->pair_size[sid] <<= 1;
    v->pairs[sid] = (struct voronoi_pair *)realloc(
        v->pairs[sid], v->pair_size[sid] * sizeof(struct voronoi_pair));
  }
  /* Initialize pair */
  struct voronoi_pair *this_pair = &v->pairs[sid][v->pair_index[sid]];
  voronoi_pair_init(this_pair, c, left_part_pointer, right_part_pointer,
                    vertices, n_vertices);
  /* return and then increase */
  return v->pair_index[sid]++;
}

/**
 * @brief Sanity checks on the grid.
 *
 * Right now, this only checks the total volume of the cells.
 */
inline static void voronoi_check_grid(struct voronoi *restrict v) {
#ifdef VORONOI_CHECKS
  double total_volume = 0.;
  for (int i = 0; i < v->number_of_cells; i++) {
    total_volume += v->cells[i].volume;
  }
  fprintf(stderr, "Total volume: %g\n", total_volume);
#endif
}

/**
 * @brief Write the Voronoi grid information to the given file.
 *
 * The output depends on the configuration. The maximal output contains 3
 * different types of output lines:
 *  - "G\tgx\tgx: x and y position of a single grid generator (optional).
 *  - "C\tcx\tcy\tV\tnface": centroid position, volume and (optionally) number
 *    of faces for a single Voronoi cell.
 *  - "F\tsid\tarea\tcx\tcy\tcz\t(v0x, v0y, v0z)\t...\t(vNx, vNy, vNz)": sid,
 *    area, coordinates of centroid, coordinates of vertices of face (optional).
 *
 * @param v Voronoi grid.
 * @param file File to write to.
 */
inline static void voronoi_print_grid(const struct voronoi *v,
                                      const char *filename) {
  FILE *file = fopen(filename, "w");

  /* first write the cells (and generators, if those are stored) */
  for (int i = 0; i < v->number_of_cells; ++i) {
    struct voronoi_cell *this_cell = &v->cells[i];
#ifdef VORONOI_STORE_GENERATORS
    fprintf(file, "G\t%g\t%g\t%g\n", this_cell->generator[0],
            this_cell->generator[1], this_cell->generator[2]);
#endif
    fprintf(file, "C\t%g\t%g\t%g\t%g", this_cell->centroid[0],
            this_cell->centroid[1], this_cell->centroid[2], this_cell->volume);
#ifdef VORONOI_STORE_CELL_STATS
    fprintf(file, "\t%i", this_cell->nface);
#endif
    fprintf(file, "\n");
  }

  /* now write the faces */
  for (int ngb = 0; ngb < 2; ++ngb) {
    for (int i = 0; i < v->pair_index[ngb]; ++i) {
      struct voronoi_pair *pair = &v->pairs[ngb][i];
      fprintf(file, "F\t%i\t%g\t%g\t%g\t%g", ngb, pair->surface_area,
              pair->midpoint[0], pair->midpoint[1], pair->midpoint[2]);
#ifdef VORONOI_STORE_CONNECTIONS
      for (int j = 0; j < pair->n_vertices; j++) {
        fprintf(file, "\t(%g, %g, %g)", pair->vertices[3 * j],
                pair->vertices[3 * j + 1], pair->vertices[3 * j + 2]);
      }
#endif
      fprintf(file, "\n");
    }
  }

  fclose(file);
}

/**
 * @brief Check whether two doubles are equal up to the given precision.
 *
 * @param double1
 * @param double2
 * @param precision
 * @return 1 for equality 0 else.
 */
inline static int double_cmp(double double1, double double2,
                             unsigned long precision) {
  long long1, long2;
  if (double1 > 0)
    long1 = (long)(double1 * precision + .5);
  else
    long1 = (long)(double1 * precision - .5);
  if (double2 > 0)
    long2 = (long)(double2 * precision + .5);
  else
    long2 = (long)(double2 * precision - .5);
  return (long1 == long2);
}

#endif  // CVORONOI_VORONOI3D_H
