//
// Created by yuyttenh on 08/06/2021.
//

#ifndef CVORONOI_DELAUNAY3D_H
#define CVORONOI_DELAUNAY3D_H

#include "geometry.h"
#include "hydro_space.h"
#include "tetrahedron.h"

/* Forward declarations */
struct delaunay;
inline static void delaunay_check_tessellation(struct delaunay* d);
inline static int delaunay_new_vertex(struct delaunay* restrict d, double x,
                                      double y, double z);
inline static void delaunay_add_vertex(struct delaunay* restrict d, int v);
inline static int delaunay_new_tetrahedron(struct delaunay* restrict d);
inline static void delaunay_tetrahedron_init(struct delaunay* restrict d, int t,
                                             int v0, int v1, int v2, int v3);
inline static int delaunay_free_indices_queue_pop(struct delaunay* restrict d);
inline static void delaunay_free_index_enqueue(struct delaunay* restrict d,
                                               int idx);
inline static int delaunay_tetrahedron_queue_pop(struct delaunay* restrict d);
inline static void delaunay_tetrahedron_enqueue(struct delaunay* restrict d,
                                                int t);
inline static void delaunay_append_tetrahedron_containing_vertex(
    struct delaunay* d, int t);
inline static int delaunay_find_tetrahedra_containing_vertex(struct delaunay* d,
                                                             int v);
inline static void delaunay_one_to_four_flip(struct delaunay* d, int v, int t);
inline static void delaunay_two_to_six_flip(struct delaunay* d, int v, int* t);
inline static void delaunay_n_to_2n_flip(struct delaunay* d, int v, int* t,
                                         int n);
inline static void delaunay_check_tetrahedra(struct delaunay* d, int v);
inline static int delaunay_check_tetrahedron(struct delaunay* d, int t, int v);
inline static int positive_permutation(int a, int b, int c, int d);

struct delaunay {

  /*! @brief Anchor of the simulation volume. */
  double anchor[3];

  /*! @brief Inverse side length of the simulation volume. */
  double inverse_side;

  /*! @brief Vertex positions. This array is a copy of the array defined in
   *  main() and we probably want to get rid of it in a SWIFT implementation. */
  double* vertices;

#ifdef DELAUNAY_NONEXACT
  /*! @brief Vertex positions, rescaled to the range 1-2. Kept here in case we
   *  want to adopt hybrid geometrical checks (floating point checks when safe,
   *  integer checks when there is a risk of numerical error leading to the
   *  wrong result) to speed things up. */
  double* rescaled_vertices;
#endif

  /*! @brief Integer vertices. These are the vertex coordinates that are
   *  actually used during the incremental construction. */
  unsigned long int* integer_vertices;

  /*! @brief Vertex search radii. For every vertex, this array contains twice
   *  the radius of the largest circumsphere of the tetrahedra that vertex is
   *  part of. */
  double* search_radii;

  /*! @brief Next available index within the vertex array. Corresponds to the
   *  actual size of the vertex array. */
  int vertex_index;

  /*! @brief Current size of the vertex array in memory. If vertex_size matches
   *  vertex_index, the memory buffer is full and needs to be expanded. */
  int vertex_size;

  /*! @brief Begin index of the normal vertices. This skips the 3 auxiliary
   *  vertices required for the incremental construction algorithm. */
  int vertex_start;

  /*! @brief End index of the normal vertices. This variable is set by calling
   *  delaunay_consolidate() and contains the offset of the ghost vertices
   *  within the vertex array. */
  int vertex_end;

  /*! @brief Tetrahedra that make up the tessellation. */
  struct tetrahedron* tetrahedra;

  /*! @brief Next available index within the tetrahedron array. Corresponds to
   * the actual size of the tetrahedron array. */
  int tetrahedron_index;

  /*! @brief Current size of the tetrahedron array in memory. If
   * tetrahedron_size matches tetrahedron_index, the memory buffer is full and
   * needs to be expanded. */
  int tetrahedron_size;

  /*! @brief Lifo queue of tetrahedra that need checking during the incremental
   *  construction algorithm. After a new vertex has been added, all new
   *  tetrahedra are added to this queue and are tested to see if the
   *  Delaunay criterion (empty circumcircles) still holds. New tetrahedra
   *  created when flipping invalid tetrahedra are also added to the queue. */
  int* tetrahedron_queue;

  /*! @brief Next available index in the tetrahedron_queue. Determines both the
   * actual size of the tetrahedron_queue as the first element that will be
   * popped from the tetrahedron_queue. */
  int tetrahedron_q_index;

  /*! @brief Current size of the tetrahedron_queue in memory. If
   * tetrahedron_q_size matches tetrahedron_q_index, the memory buffer
   * is full and needs to be expanded. */
  int tetrahedron_q_size;

  /*! @brief Lifo queue of free spots in the tetrahedra array. Sometimes 3
   * tetrahedra can be split into 2 new ones. This leaves a free spot in the
   * array.
   */
  int* free_indices_queue;

  /*! @brief Next available index in de free_indices_queue. Determines both the
   * actual size of the free_indices_queue as the first element that will be
   * popped from the free_indices_queue. */
  int free_indices_q_index;

  /*! @brief Current size of the free_indices_queue in memory. If
   * free_indices_q_size matches free_indices_q_index, the memory buffer
   * is full and needs to be expanded. */
  int free_indices_q_size;

  /*! @brief Array of tetrahedra containing the current vertex */
  int* tetrahedra_containing_vertex;

  /*! @brief Next index in the array of tetrahedra containing the current
   * vertex. This is also the number of tetrahedra containing the current
   * vertex. */
  int tetrahedra_containing_vertex_index;

  /*! @brief Size of the array of tetrahedra containing the current vertex in
   * memory. */
  int tetrahedra_containing_vertex_size;

  /*! @brief Index of the last tetrahedron that was accessed. Used as initial
   *  guess for the tetrahedron that contains the next vertex that will be
   * added. If vertices are added in some sensible order (e.g. in Peano-Hilbert
   * curve order) then this will greatly speed up the algorithm. */
  int last_tetrahedron;

  /*! @brief Geometry variables. Auxiliary variables used by the exact integer
   *  geometry tests that need to be stored in between tests, since allocating
   *  and deallocating them for every test is too expensive. */
  struct geometry geometry;
};

/**
 * @brief Initialize the Delaunay tessellation.
 *
 * This function allocates memory for all arrays that make up the tessellation
 * and initializes the variables used for bookkeeping.
 *
 * It then sets up a large tetrahedron that contains the entire simulation box
 * and additional buffer space to deal with boundary ghost vertices, and 4
 * additional dummy tetrahedron that provide valid neighbours for the 4 sides of
 * this tetrahedron (these dummy tetrahedra themselves have an invalid tip
 * vertex and are therefore simply placeholders).
 *
 * @param d Delaunay tessellation.
 * @param hs Spatial extents of the simulation box.
 * @param vertex_size Initial size of the vertex array.
 * @param tetrahedron_size Initial size of the tetrahedra array.
 */
inline static void delaunay_init(struct delaunay* restrict d,
                                 const struct hydro_space* restrict hs,
                                 int vertex_size, int tetrahedron_size) {
  /* allocate memory for the vertex arrays */
  d->vertices = (double*)malloc(vertex_size * 3 * sizeof(double));
#ifdef DELAUNAY_NONEXACT
  d->rescaled_vertices = (double*)malloc(vertex_size * 3 * sizeof(double));
#endif
  d->integer_vertices =
      (unsigned long int*)malloc(vertex_size * 3 * sizeof(unsigned long int));
  d->search_radii = (double*)malloc(vertex_size * sizeof(double));
  d->vertex_size = vertex_size;
  /* set vertex start and end (indicating where the local vertices start and
   * end)*/
  d->vertex_start = 0;
  d->vertex_end = vertex_size;
  /* we add the dummy vertices behind the local vertices and before the ghost
   * vertices (see below) */
  d->vertex_index = vertex_size;

  /* allocate memory for the tetrahedra array */
  d->tetrahedra = (struct tetrahedron*)malloc(tetrahedron_size *
                                              sizeof(struct tetrahedron));
  d->tetrahedron_index = 0;
  d->tetrahedron_size = tetrahedron_size;

  /* allocate memory for the tetrahedron_queue (note that the tetrahedron_queue
     size of 10 was chosen arbitrarily, and a proper value should be chosen
     based on performance measurements) */
  d->tetrahedron_queue = (int*)malloc(10 * sizeof(int));
  d->tetrahedron_q_index = 0;
  d->tetrahedron_q_size = 10;

  /* allocate memory for the free_indices_queue */
  d->free_indices_queue = (int*)malloc(10 * sizeof(int));
  d->free_indices_q_index = 0;
  d->free_indices_q_size = 10;

  /* allocate memory for the array of tetrahedra containing the current vertex
   * Initial size is 10, may grow a lot for degenerate cases... */
  d->tetrahedra_containing_vertex = (int*)malloc(10 * sizeof(int));
  d->tetrahedra_containing_vertex_index = 0;
  d->tetrahedra_containing_vertex_size = 10;

  /* determine the size of a box large enough to accommodate the entire
   * simulation volume and all possible ghost vertices required to deal with
   * boundaries. Note that we convert the generally rectangular box to a
   * square. */
  double box_anchor[3] = {hs->anchor[0] - hs->side[0],
                          hs->anchor[1] - hs->side[1],
                          hs->anchor[2] - hs->side[2]};
  /* Notice we have to take box_side rather large, because we want to fit the
   * cell and all neighbouring cells inside the first tetrahedron. This comes at
   * a loss of precision in the integer arithmetic, though... A better solution
   * would possibly be to start from 5 tetrahedra forming a cube (box_side would
   * have to be 3 in that case). */
  double box_side = fmax(hs->side[0], hs->side[1]);
  box_side = 9 * fmax(box_side, hs->side[2]);
  /* store the anchor and inverse side_length for the conversion from box
     coordinates to rescaled (integer) coordinates */
  d->anchor[0] = box_anchor[0];
  d->anchor[1] = box_anchor[1];
  d->anchor[2] = box_anchor[2];
  /* the 1.e-13 makes sure converted values are in the range [1, 2[ instead of
   * [1,2] (unlike Springel, 2010) */
  d->inverse_side = (1. - 1.e-13) / box_side;

  /* initialise the structure used to perform exact geometrical tests */
  geometry_init(&d->geometry);

  /* set up vertices for large initial tetrahedron */
  int v0 = delaunay_new_vertex(d, d->anchor[0], d->anchor[1], d->anchor[2]);
  int v1 = delaunay_new_vertex(d, d->anchor[0] + box_side, d->anchor[1],
                               d->anchor[2]);
  int v2 = delaunay_new_vertex(d, d->anchor[0], d->anchor[1] + box_side,
                               d->anchor[2]);
  int v3 = delaunay_new_vertex(d, d->anchor[0], d->anchor[1],
                               d->anchor[2] + box_side);
  /* Create initial large tetrahedron and 4 dummy neighbours */
  int dummy0 = delaunay_new_tetrahedron(d); /* opposite of v0 */
  int dummy1 = delaunay_new_tetrahedron(d); /* opposite of v1 */
  int dummy2 = delaunay_new_tetrahedron(d); /* opposite of v2 */
  int dummy3 = delaunay_new_tetrahedron(d); /* opposite of v3 */
  int first_tetrahedron = delaunay_new_tetrahedron(d);
  delaunay_log("Creating dummy tetrahedron at %i with vertices: %i %i %i %i",
               dummy0, v1, v2, v3, -1);
  tetrahedron_init(&d->tetrahedra[dummy0], v1, v2, v3, -1);
  delaunay_log("Creating dummy tetrahedron at %i with vertices: %i %i %i %i",
               dummy1, v2, v0, v3, -1);
  tetrahedron_init(&d->tetrahedra[dummy1], v2, v0, v3, -1);
  delaunay_log("Creating dummy tetrahedron at %i with vertices: %i %i %i %i",
               dummy2, v3, v0, v1, -1);
  tetrahedron_init(&d->tetrahedra[dummy2], v3, v0, v1, -1);
  delaunay_log("Creating dummy tetrahedron at %i with vertices: %i %i %i %i",
               dummy3, v0, v2, v1, -1);
  tetrahedron_init(&d->tetrahedra[dummy3], v0, v2, v1, -1);
  delaunay_tetrahedron_init(d, first_tetrahedron, v0, v1, v2, v3);

  /* Setup neighour relations */
  tetrahedron_swap_neighbour(&d->tetrahedra[dummy0], 3, first_tetrahedron, 0);
  tetrahedron_swap_neighbour(&d->tetrahedra[dummy1], 3, first_tetrahedron, 1);
  tetrahedron_swap_neighbour(&d->tetrahedra[dummy2], 3, first_tetrahedron, 2);
  tetrahedron_swap_neighbour(&d->tetrahedra[dummy3], 3, first_tetrahedron, 3);
  tetrahedron_swap_neighbours(&d->tetrahedra[first_tetrahedron], dummy0, dummy1,
                              dummy2, dummy3, 3, 3, 3, 3);

  /* TODO: Setup vertex-tetrahedra links... */

  /* Perform sanity checks */
  delaunay_check_tessellation(d);
  delaunay_log("Passed post init check");
}

inline static void delaunay_destroy(struct delaunay* restrict d) {
  free(d->vertices);
#ifdef DELAUNAY_NONEXACT
  free(d->rescaled_vertices);
#endif
  free(d->integer_vertices);
  free(d->search_radii);
  free(d->tetrahedra);
  free(d->tetrahedron_queue);
  geometry_destroy(&d->geometry);
}

inline static int delaunay_new_tetrahedron(struct delaunay* restrict d) {
  /* check whether there is a free spot somewhere in the array */
  int index = delaunay_free_indices_queue_pop(d);
  if (index >= 0) {
    return index;
  }
  /* Else: check that we still have space for tetrahedrons available */
  if (d->tetrahedron_index == d->tetrahedron_size) {
    d->tetrahedron_size <<= 1;
    d->tetrahedra = (struct tetrahedron*)realloc(
        d->tetrahedra, d->tetrahedron_size * sizeof(struct tetrahedron));
  }
  /* return and then increase */
  return d->tetrahedron_index++;
}

/**
 * @brief Utility method to initialize a tetrahedron.
 *
 * If Checks are enabled, this will also check the orientation of the
 * tetrahedron.
 *
 * @param d Delaunay tesselation
 * @param t Index to initialize tetrahedron at
 * @param v0, v1, v2, v3 Indices of the vertices of the tetrahedron
 */
inline static void delaunay_tetrahedron_init(struct delaunay* restrict d, int t,
                                             int v0, int v1, int v2, int v3) {
  delaunay_log("Initializing tetrahedron at %i with vertices: %i %i %i %i", t,
               v0, v1, v2, v3);
#ifdef DELAUNAY_CHECKS
  const unsigned long aix = d->integer_vertices[3 * v0];
  const unsigned long aiy = d->integer_vertices[3 * v0 + 1];
  const unsigned long aiz = d->integer_vertices[3 * v0 + 2];

  const unsigned long bix = d->integer_vertices[3 * v1];
  const unsigned long biy = d->integer_vertices[3 * v1 + 1];
  const unsigned long biz = d->integer_vertices[3 * v1 + 2];

  const unsigned long cix = d->integer_vertices[3 * v2];
  const unsigned long ciy = d->integer_vertices[3 * v2 + 1];
  const unsigned long ciz = d->integer_vertices[3 * v2 + 2];

  const unsigned long dix = d->integer_vertices[3 * v3];
  const unsigned long diy = d->integer_vertices[3 * v3 + 1];
  const unsigned long diz = d->integer_vertices[3 * v3 + 2];

  const int test = geometry_orient_exact(&d->geometry, aix, aiy, aiz, bix, biy,
                                         biz, cix, ciy, ciz, dix, diy, diz);
  if (test > 0) {
    fprintf(stderr, "Initializing tetrahedron with incorrect orientation!\n");
    fprintf(stderr, "\tTetrahedron: %i\n\tVertices: %i %i %i %i\n", t, v0, v1,
            v2, v3);
    abort();
  }
#endif
  tetrahedron_init(&d->tetrahedra[t], v0, v1, v2, v3);

  /* Touch the last initialized tetrahedron, This will be our next guess upon
   * insertion. */
  d->last_tetrahedron = t;
}

inline static void delaunay_init_vertex(struct delaunay* restrict d,
                                        const int v, double x, double y,
                                        double z) {
  /* store a copy of the vertex coordinates (we should get rid of this for
     SWIFT) */
  d->vertices[3 * v] = x;
  d->vertices[3 * v + 1] = y;
  d->vertices[3 * v + 2] = z;

  /* compute the rescaled coordinates. We do this because floating point values
     in the range [1,2[ all have the same exponent (0), which guarantees that
     their mantissas form a linear sequence */
  double rescaled_x = 1. + (x - d->anchor[0]) * d->inverse_side;
  double rescaled_y = 1. + (y - d->anchor[1]) * d->inverse_side;
  double rescaled_z = 1. + (z - d->anchor[2]) * d->inverse_side;

  delaunay_assert(rescaled_x >= 1.);
  delaunay_assert(rescaled_x < 2.);
  delaunay_assert(rescaled_y >= 1.);
  delaunay_assert(rescaled_y < 2.);
  delaunay_assert(rescaled_z >= 1.);
  delaunay_assert(rescaled_z < 2.);

#ifdef DELAUNAY_NONEXACT
  /* store a copy of the rescaled coordinates to apply non-exact tests */
  d->rescaled_vertices[3 * d->vertex_index] = rescaled_x;
  d->rescaled_vertices[3 * d->vertex_index + 1] = rescaled_y;
  d->rescaled_vertices[3 * d->vertex_index + 2] = rescaled_z;
#endif

  /* convert the rescaled coordinates to integer coordinates and store these */
  d->integer_vertices[3 * v] = delaunay_double_to_int(rescaled_x);
  d->integer_vertices[3 * v + 1] = delaunay_double_to_int(rescaled_y);
  d->integer_vertices[3 * v + 2] = delaunay_double_to_int(rescaled_z);

  /* TODO: initialise the variables that keep track of the link between vertices
   * and tetrahedra. We use negative values so that we can later detect missing
   * links. */
  /*d->vertex_triangles[v] = -1;
  d->vertex_triangle_index[v] = -1;*/

  /* initialise the search radii to the largest possible value */
  d->search_radii[v] = DBL_MAX;
}

/**
 * @brief Add a new vertex with the given coordinates.
 *
 * This function first makes sure there is sufficient memory to store the
 * vertex and all its properties. It then initializes the vertex
 *
 * @param d Delaunay tessellation.
 * @param x Horizontal coordinate of the vertex.
 * @param y Vertical coordinate of the vertex.
 * @param z Z position of the vertex.
 * @return Index of the new vertex within the vertex array.
 */
inline static int delaunay_new_vertex(struct delaunay* restrict d, double x,
                                      double y, double z) {
  delaunay_log("Adding new vertex at %i with coordinates: %g %g %g", d->vertex_index, x, y, z);
  /* check the size of the vertex arrays against the allocated memory size */
  if (d->vertex_index == d->vertex_size) {
    /* dynamically grow the size of the arrays with a factor 2 */
    d->vertex_size <<= 1;
    d->vertices =
        (double*)realloc(d->vertices, d->vertex_size * 3 * sizeof(double));
#ifdef DELAUNAY_NONEXACT
    d->rescaled_vertices = (double*)realloc(
        d->rescaled_vertices, d->vertex_size * 3 * sizeof(double));
#endif
    d->integer_vertices = (unsigned long int*)realloc(
        d->integer_vertices, d->vertex_size * 3 * sizeof(unsigned long int));
    d->search_radii =
        (double*)realloc(d->search_radii, d->vertex_size * sizeof(double));
  }

  delaunay_init_vertex(d, d->vertex_index, x, y, z);

  /* return the vertex index and then increase it by 1.
     After this operation, vertex_index will correspond to the size of the
     vertex arrays and is also the index of the next vertex that will be
     created. */
  return d->vertex_index++;
}

/**
 * @brief Add a local (non ghost) vertex at the given index.
 * @param d Delaunay tessellation
 * @param v Index to add vertex at
 * @param x, y, z Position of vertex
 */
inline static void delaunay_add_local_vertex(struct delaunay* restrict d, int v,
                                             double x, double y, double z) {
  delaunay_assert(v < d->vertex_end && d->vertex_start <= v);
  delaunay_log("Adding local vertex at %i with coordinates: %g %g %g", v, x, y, z);
  delaunay_init_vertex(d, v, x, y, z);
  delaunay_add_vertex(d, v);
}

/**
 * @brief Add a new (ghost) vertex.
 * @param d Delaunay tessellation
 * @param x, y, z Position of vertex
 */
inline static void delaunay_add_new_vertex(struct delaunay* restrict d,
                                           double x, double y, double z) {
  int v = delaunay_new_vertex(d, x, y, z);
  delaunay_add_vertex(d, v);
}

/**
 * @brief Finalize adding a new vertex to the tessellation.
 *
 * This function locates the tetrahedron in the current tessellation that
 * contains the new vertex. Depending on the case (see below) new tetrahedra are
 * added to the tessellation and some are removed.
 *
 * Cases:
 * 1. Point is fully inside a tetrahedron. In this case, this tetrahedron will
 *    be replaced by 4 new tetrahedra.
 * 2. Point is on face between two tetrahedra. In this case, these two will be
 *    replaced by 6 new ones.
 * 3. Point is on edge of N tetrahedra. These N tetrahedra will be replaced by
 *    2N new ones.
 *
 * @param d Delaunay tessellation.
 * @param v Index of new vertex
 */
inline static void delaunay_add_vertex(struct delaunay* restrict d, int v) {
  int number_of_tetrahedra = delaunay_find_tetrahedra_containing_vertex(d, v);

  if (number_of_tetrahedra == 1) {
    /* normal case: split 'd->tetrahedra_containing_vertex[0]' into 4 new
     * tetrahedra */
    delaunay_log("Vertex %i lies fully inside tetrahedron %i", v,
                 d->tetrahedra_containing_vertex[0]);
    delaunay_one_to_four_flip(d, v, d->tetrahedra_containing_vertex[0]);
  } else if (number_of_tetrahedra == 2) {
    /* point on face: replace the 2 tetrahedra with 6 new ones */
    delaunay_log("Vertex %i on the face between tetrahedra %i and %i", v,
                 d->tetrahedra_containing_vertex[0],
                 d->tetrahedra_containing_vertex[0]);
    delaunay_two_to_six_flip(d, v, d->tetrahedra_containing_vertex);
  } else if (number_of_tetrahedra > 2) {
    /* point on edge: replace the N tetrahedra with 2N new ones */
    delaunay_log(
        "Vertex %i lies on the edge shared by tetrahedra %i, %i and %i", v,
        d->tetrahedra_containing_vertex[0], d->tetrahedra_containing_vertex[1],
        d->tetrahedra_containing_vertex[number_of_tetrahedra - 1]);
    delaunay_n_to_2n_flip(d, v, d->tetrahedra_containing_vertex,
                          number_of_tetrahedra);
  } else {
    fprintf(stderr, "Unknown case of number of tetrahedra: %i!\n",
            number_of_tetrahedra);
    abort();
  }

  /* Now check all tetrahedra in de queue */
  delaunay_check_tetrahedra(d, v);

  /* perform sanity checks if enabled */
  delaunay_check_tessellation(d);
  delaunay_log("Passed checks after inserting vertex %i", v);
}

/**
 * @brief Find tetrahedra containing the given vertex
 * The tetrahedra are stored in d->tetrahedra_containing_vertex
 *
 * @param d Delaunay tessellation
 * @param v The vertex
 * @return The number of tetrahedra containing the given vertex.
 */
inline static int delaunay_find_tetrahedra_containing_vertex(
    struct delaunay* restrict d, const int v) {
  /* Before we do anything: reset the index in the array of tetrahedra
   * containing the current vertex */
  d->tetrahedra_containing_vertex_index = 0;

  /* Get the last tetrahedron index */
  int tetrahedron_idx = d->last_tetrahedron;
  /* Get the coordinates of the test vertex */
#ifdef DELAUNAY_NONEXACT
  const double ex = d->vertices[3 * v];
  const double ey = d->vertices[3 * v + 1];
  const double ez = d->vertices[3 * v + 2];
#endif
  const unsigned long eix = d->integer_vertices[3 * v];
  const unsigned long eiy = d->integer_vertices[3 * v + 1];
  const unsigned long eiz = d->integer_vertices[3 * v + 2];

  while (d->tetrahedra_containing_vertex_index == 0) {
    const struct tetrahedron* tetrahedron = &d->tetrahedra[tetrahedron_idx];
    const int v0 = tetrahedron->vertices[0];
    const int v1 = tetrahedron->vertices[1];
    const int v2 = tetrahedron->vertices[2];
    const int v3 = tetrahedron->vertices[3];
    /* Get the coordinates of the vertices of the tetrahedron */
#ifdef DELAUNAY_NONEXACT
    const double ax = d->vertices[3 * v0];
    const double ay = d->vertices[3 * v0 + 1];
    const double az = d->vertices[3 * v0 + 2];

    const double bx = d->vertices[3 * v1];
    const double by = d->vertices[3 * v1 + 1];
    const double bz = d->vertices[3 * v1 + 2];

    const double cx = d->vertices[3 * v2];
    const double cy = d->vertices[3 * v2 + 1];
    const double cz = d->vertices[3 * v2 + 2];

    const double dx = d->vertices[3 * v3];
    const double dy = d->vertices[3 * v3 + 1];
    const double dz = d->vertices[3 * v3 + 2];
#endif
    const unsigned long aix = d->integer_vertices[3 * v0];
    const unsigned long aiy = d->integer_vertices[3 * v0 + 1];
    const unsigned long aiz = d->integer_vertices[3 * v0 + 2];

    const unsigned long bix = d->integer_vertices[3 * v1];
    const unsigned long biy = d->integer_vertices[3 * v1 + 1];
    const unsigned long biz = d->integer_vertices[3 * v1 + 2];

    const unsigned long cix = d->integer_vertices[3 * v2];
    const unsigned long ciy = d->integer_vertices[3 * v2 + 1];
    const unsigned long ciz = d->integer_vertices[3 * v2 + 2];

    const unsigned long dix = d->integer_vertices[3 * v3];
    const unsigned long diy = d->integer_vertices[3 * v3 + 1];
    const unsigned long diz = d->integer_vertices[3 * v3 + 2];

#ifdef DELAUNAY_CHECKS
    /* made sure the tetrahedron is correctly oriented */
    if (geometry_orient_exact(&d->geometry, aix, aiy, aiz, bix, biy, biz, cix,
                              ciy, ciz, dix, diy, diz) >= 0) {
      fprintf(stderr, "Incorrect orientation for tetrahedron %i!",
              tetrahedron_idx);
      abort();
    }
#endif
    int non_axis_v_idx[2];
    /* Check whether the point is inside or outside all four faces */
    const int test_abce =
        geometry_orient_exact(&d->geometry, aix, aiy, aiz, bix, biy, biz, cix,
                              ciy, ciz, eix, eiy, eiz);
    if (test_abce > 0) {
      /* v outside face opposite of v3 */
      tetrahedron_idx = tetrahedron->neighbours[3];
      continue;
    }
    const int test_acde =
        geometry_orient_exact(&d->geometry, aix, aiy, aiz, cix, ciy, ciz, dix,
                              diy, diz, eix, eiy, eiz);
    if (test_acde > 0) {
      /* v outside face opposite of v1 */
      tetrahedron_idx = tetrahedron->neighbours[1];
      continue;
    }
    const int test_adbe =
        geometry_orient_exact(&d->geometry, aix, aiy, aiz, dix, diy, diz, bix,
                              biy, biz, eix, eiy, eiz);
    if (test_adbe > 0) {
      /* v outside face opposite of v2 */
      tetrahedron_idx = tetrahedron->neighbours[2];
      continue;
    }
    const int test_bdce =
        geometry_orient_exact(&d->geometry, bix, biy, biz, dix, diy, diz, cix,
                              ciy, ciz, eix, eiy, eiz);
    if (test_bdce > 0) {
      /* v outside face opposite of v0 */
      tetrahedron_idx = tetrahedron->neighbours[0];
      continue;
    } else {
      /* Point inside tetrahedron, check for degenerate cases */
      delaunay_append_tetrahedron_containing_vertex(d, tetrahedron_idx);
      if (test_abce == 0) {
        non_axis_v_idx[d->tetrahedra_containing_vertex_index] = 3;
        delaunay_append_tetrahedron_containing_vertex(
            d, tetrahedron->neighbours[3]);
      }
      if (test_adbe == 0) {
        non_axis_v_idx[d->tetrahedra_containing_vertex_index] = 2;
        delaunay_append_tetrahedron_containing_vertex(
            d, tetrahedron->neighbours[2]);
      }
      if (test_acde == 0) {
        non_axis_v_idx[d->tetrahedra_containing_vertex_index] = 1;
        delaunay_append_tetrahedron_containing_vertex(
            d, tetrahedron->neighbours[1]);
      }
      if (test_bdce == 0) {
        non_axis_v_idx[d->tetrahedra_containing_vertex_index] = 0;
        delaunay_append_tetrahedron_containing_vertex(
            d, tetrahedron->neighbours[0]);
      }
    }

    if (d->tetrahedra_containing_vertex_index > 3) {
      /* Impossible case, the vertex cannot simultaneously lie in this
       * tetrahedron and 3 or more of its direct neighbours */
      fprintf(stderr,
              "Impossible scenario encountered while searching for tetrahedra "
              "containing vertex %i!",
              v);
      abort();
    }
    if (d->tetrahedra_containing_vertex_index > 2) {
      /* Vertex on edge of tetrahedron. This edge can be shared by any number of
       * tetrahedra, of which we already know three. Find the other ones by
       * rotating around this edge. */
      const int non_axis_idx0 = non_axis_v_idx[0];
      const int non_axis_idx1 = non_axis_v_idx[1];
      int axis_idx0 = (non_axis_idx0 + 1) % 4;
      if (axis_idx0 == non_axis_idx1) {
        axis_idx0 = (axis_idx0 + 1) % 4;
      }
      const int axis_idx1 = 6 - axis_idx0 - non_axis_idx0 - non_axis_idx1;
      delaunay_assert(
          axis_idx0 != axis_idx1 && axis_idx0 != non_axis_idx0 &&
          axis_idx0 != non_axis_idx1 && axis_idx1 != non_axis_idx0 &&
          axis_idx1 != non_axis_idx1 && non_axis_idx0 != non_axis_idx1);
      /* a0 and a1 are the vertices shared by all tetrahedra */
      const int a0 = tetrahedron->vertices[axis_idx0];
      const int a1 = tetrahedron->vertices[axis_idx1];
      /* We now walk around the axis and add all tetrahedra to the list of
       * tetrahedra containing v. */
      const int last_t = d->tetrahedra_containing_vertex[1];
      int next_t = d->tetrahedra_containing_vertex[2];
      int next_vertex = tetrahedron->index_in_neighbour[non_axis_idx1];
      /* We are going to add d->tetrahedra_containing_vertex[2] and
       * d->tetrahedra_containing_vertex[1] back to the array of tetrahedra
       * containing v, but now with all other tetrahedra that also share the
       * edge in between, so make sure they are not added twice. */
      d->tetrahedra_containing_vertex_index -= 2;
      while (next_t != last_t) {
        delaunay_append_tetrahedron_containing_vertex(d, next_t);
        next_vertex = (next_vertex + 1) % 4;
        if (d->tetrahedra[next_t].vertices[next_vertex] == a0 ||
            d->tetrahedra[next_t].vertices[next_vertex] == a1) {
          next_vertex = (next_vertex + 1) % 4;
        }
        if (d->tetrahedra[next_t].vertices[next_vertex] == a0 ||
            d->tetrahedra[next_t].vertices[next_vertex] == a1) {
          next_vertex = (next_vertex + 1) % 4;
        }
        delaunay_assert(d->tetrahedra[next_t].vertices[next_vertex] != a0 &&
                        d->tetrahedra[next_t].vertices[next_vertex] != a1);

        const int cur_vertex = next_vertex;
        next_vertex = d->tetrahedra[next_t].index_in_neighbour[cur_vertex];
        next_t = d->tetrahedra[next_t].neighbours[cur_vertex];
      }
    }
  }
  return d->tetrahedra_containing_vertex_index;
}

/**
 * @brief Replace the given tetrahedron with four new ones by inserting the
 * given new vertex.
 *
 * @image html newvoronoicell_one_to_four_flip.png
 *
 * The original tetrahedron is positively oriented, and hence its vertices are
 * ordered as shown in the figure. We construct four new tetrahedra by replacing
 * one of the four original vertices with the new vertex. If we keep the
 * ordering of the vertices, then the new tetrahedra will also be positively
 * oriented. The new neighbour relations can be easily deduced from the figure,
 * and the new index relations follow automatically from the way we construct
 * the new tetrahedra.
 *
 * For clarity, the common faces of the new tetrahedra are marked in green in
 * the figure.
 *
 * The first new tetrahedron replaces the original tetrahedron, while the three
 * extra new tetrahedra are added to the list.
 *
 * @param d Delaunay tessellation
 * @param v New vertex to insert.
 * @param t Tetrahedron to replace.
 */
inline static void delaunay_one_to_four_flip(struct delaunay* d, int v, int t) {
  delaunay_log("Flipping tetrahedron %i to 4 new ones.", t);

  /* Extract necessary information */
  const int vertices[4] = {
      d->tetrahedra[t].vertices[0], d->tetrahedra[t].vertices[1],
      d->tetrahedra[t].vertices[2], d->tetrahedra[t].vertices[3]};
  const int ngbs[4] = {
      d->tetrahedra[t].neighbours[0], d->tetrahedra[t].neighbours[1],
      d->tetrahedra[t].neighbours[2], d->tetrahedra[t].neighbours[3]};
  const int idx_in_ngbs[4] = {d->tetrahedra[t].index_in_neighbour[0],
                              d->tetrahedra[t].index_in_neighbour[1],
                              d->tetrahedra[t].index_in_neighbour[2],
                              d->tetrahedra[t].index_in_neighbour[3]};

  /* Replace t and create 3 new tetrahedra */
  delaunay_tetrahedron_init(d, t, vertices[0], vertices[1], vertices[2], v);
  const int t1 = delaunay_new_tetrahedron(d);
  delaunay_tetrahedron_init(d, t1, vertices[0], vertices[1], v, vertices[3]);
  const int t2 = delaunay_new_tetrahedron(d);
  delaunay_tetrahedron_init(d, t2, vertices[0], v, vertices[2], vertices[3]);
  const int t3 = delaunay_new_tetrahedron(d);
  delaunay_tetrahedron_init(d, t3, v, vertices[1], vertices[2], vertices[3]);

  /* update neighbour relations */
  tetrahedron_swap_neighbours(&d->tetrahedra[t], t3, t2, t1, ngbs[3], 3, 3, 3,
                              idx_in_ngbs[3]);
  tetrahedron_swap_neighbours(&d->tetrahedra[t1], t3, t2, ngbs[2], t, 2, 2,
                              idx_in_ngbs[2], 2);
  tetrahedron_swap_neighbours(&d->tetrahedra[t2], t3, ngbs[1], t1, t, 1,
                              idx_in_ngbs[1], 1, 1);
  tetrahedron_swap_neighbours(&d->tetrahedra[t3], ngbs[0], t2, t1, t,
                              idx_in_ngbs[0], 0, 0, 0);

  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[0]], idx_in_ngbs[0], t3, 0);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[1]], idx_in_ngbs[1], t2, 1);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[2]], idx_in_ngbs[2], t1, 2);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[3]], idx_in_ngbs[3], t, 3);

  /* enqueue all new/updated tetrahedra for delaunay checks */
  delaunay_tetrahedron_enqueue(d, t);
  delaunay_tetrahedron_enqueue(d, t1);
  delaunay_tetrahedron_enqueue(d, t2);
  delaunay_tetrahedron_enqueue(d, t3);
}

/**
 * @brief Replace the given two tetrahedra with six new ones by inserting the
 * given new vertex.
 *
 * @image html newvoronoicell_two_to_six_flip.png
 *
 * The two positively oriented tetrahedra (0123) and (0134) are replaced with
 * six new ones by replacing the common triangle vertices one at a time: (0125),
 * (0523), (5123), (0154), (0534), and (5134). The new neighbour relations can
 * be easily deduced from the figure, while the new neighbour indices follow
 * automatically from the way we set up the tetrahedra.
 *
 * @param d Delaunay tessellation
 * @param v The new vertex
 * @param t Tetrahedra to replace
 */
inline static void delaunay_two_to_six_flip(struct delaunay* d, int v, int* t) {
  // TODO
  fprintf(stderr, "Degenerate case not implemented!");
  abort();
}

/**
 * @brief Replace the given @f$n@f$ tetrahedra with @f$2n@f$ new ones by
 * inserting the given new vertex.
 *
 * @image html newvoronoicell_n_to_2n_flip.png
 *
 * The @f$n@f$ tetrahedra
 * (v0 v@f$(n+1)@f$ v1 v@f$(n)@f$),
 * @f$...@f$,
 * (v@f$(i-1)@f$ v@f$(n+1)@f$ v@f$(i)@f$ v@f$(n)@f$),
 * @f$...@f$,
 * (v@f$(n-1)@f$ v@f$(n+1)@f$ v0 v@f$(n)@f$)
 * are replaced with the @f$2n@f$ tetrahedra
 * (v0 v@f$(n+2)@f$ v1 v@f$(n)@f$),
 * (v0 v@f$(n+1)@f$ v1 v@f$(n+2)@f$),
 * @f$...@f$,
 * (v@f$(i-1)@f$ v@f$(n+2)@f$ v@f$(i)@f$ v@f$(n)@f$),
 * (v@f$(i-1)@f$ v@f$(n+1)@f$ v@f$(i)@f$ v@f$(n+2)@f$),
 * @f$...@f$,
 * (v@f$(n-1)@f$ v@f$(n+2)@f$ v0 v@f$(n)@f$),
 * (v@f$(n-1)@f$ v@f$(n+1)@f$ v0 v@f$(n+2)@f$).
 *
 * The new neighbour relations can easily be deduced from the figure, while the
 * new neighbour indices are set automatically by the way the new tetrahedra are
 * constructed.
 *
 * @param d Delaunay tessellation
 * @param v The new vertex
 * @param t The tetrahedra to replace
 * @param n The number of tetrahedra to replace
 */
inline static void delaunay_n_to_2n_flip(struct delaunay* d, int v, int* t,
                                         int n) {
  // TODO
  fprintf(stderr, "Degenerate case not implemented!");
  abort();
}

/**
 * @brief Replace the given two tetrahedra with three new tetrahedra.
 *
 * @image html newvoronoicell_two_to_three_flip.png
 *
 * The two positively oriented tetrahedra (v0 v1 v2 v3) and (v0 v1 v3 v4) (that
 * share the red face in the figure) are
 * replaced with three new positively oriented tetrahedra (that share the green
 * faces): (v0 v1 v2 v4), (v0 v4 v2 v3), and (v4 v1 v2 v3).
 *
 * Before the flip, t0 has ngb0, ngb3 and ngb4 as neighbours, while t1 has ngb1,
 * ngb2 and ngb5 as neighbours. After the flip, t'0 has ngb4 and ngb5 as
 * neighbours, t'1 has ngb3 and ngb4 as neighbours, and t'2 has ngb0 and ngb1
 * as neighbours.
 *
 * We first figure out the indices of the common triangle vertices v0, v1 and v3
 * in both tetrahedra. Once we know these, it is very straigthforward to match
 * each index to one of these three vertices (requiring that t0 is positively
 * oriented). We can then get the actual vertices, neighbours and neighbour
 * indices and construct the new tetrahedra.
 *
 * @param d Delaunay tessellation
 * @param t0 First tetrahedron
 * @param t1 Second tetrahedron
 * @param top0 Index of the vertex of the first tetrahedron opposite the second
 * tetrahedron.
 * @param top1 Index of the vertex of the second tetrahedron opposite the first
 * tetrahedron.
 */
inline static void delaunay_two_to_three_flip(struct delaunay* restrict d,
                                              int t0, int t1, int top0,
                                              int top1) {
  /* get the indices of the common triangle of the tetrahedra, and make sure we
   * know which index in tetrahedron0 matches which index in tetrahedron1 */
  int triangle[2][3];
  for (int i = 0; i < 3; ++i) {
    triangle[0][i] = (top0 + i + 1) % 4;
    triangle[1][i] = 0;
    while (d->tetrahedra[t0].vertices[triangle[0][i]] !=
           d->tetrahedra[t1].vertices[triangle[1][i]]) {
      ++triangle[1][i];
    }
  }
  /* Make sure that we start from a positively oriented tetrahedron. The weird
   * index ordering is chosen to match the vertices in the documentation figure
   */
  if (!positive_permutation(triangle[0][1], triangle[0][2], top0,
                            triangle[0][0])) {
    int tmp = triangle[0][1];
    triangle[0][1] = triangle[0][2];
    triangle[0][2] = tmp;

    tmp = triangle[1][1];
    triangle[1][1] = triangle[1][2];
    triangle[1][2] = tmp;
  }
  const int v0_0 = triangle[0][1];
  const int v1_0 = triangle[0][2];
  const int v2_0 = top0;
  const int v3_0 = triangle[0][0];

  const int v0_1 = triangle[1][1];
  const int v1_1 = triangle[1][2];
  const int v3_1 = triangle[1][0];
  const int v4_1 = top1;

  /* set some variables to the names used in the documentation figure */
  const int vert[5] = {
      d->tetrahedra[t0].vertices[v0_0], d->tetrahedra[t0].vertices[v1_0],
      d->tetrahedra[t0].vertices[v2_0], d->tetrahedra[t0].vertices[v3_0],
      d->tetrahedra[t1].vertices[v4_1]};

  const int ngbs[6] = {
      d->tetrahedra[t0].neighbours[v0_0], d->tetrahedra[t1].neighbours[v0_1],
      d->tetrahedra[t1].neighbours[v1_1], d->tetrahedra[t0].neighbours[v1_0],
      d->tetrahedra[t0].neighbours[v3_0], d->tetrahedra[t1].neighbours[v3_1]};

  const int idx_in_ngb[6] = {d->tetrahedra[t0].index_in_neighbour[v0_0],
                             d->tetrahedra[t1].index_in_neighbour[v0_1],
                             d->tetrahedra[t1].index_in_neighbour[v1_1],
                             d->tetrahedra[t0].index_in_neighbour[v1_0],
                             d->tetrahedra[t0].index_in_neighbour[v3_0],
                             d->tetrahedra[t1].index_in_neighbour[v3_1]};

  /* overwrite t0 and t1 and create a new tetrahedron */
  delaunay_tetrahedron_init(d, t0, vert[0], vert[1], vert[2], vert[4]);
  delaunay_tetrahedron_init(d, t1, vert[0], vert[4], vert[2], vert[3]);
  const int t2 = delaunay_new_tetrahedron(d);
  delaunay_tetrahedron_init(d, t2, vert[4], vert[1], vert[2], vert[3]);

  /* fix neighbour relations */
  tetrahedron_swap_neighbours(&d->tetrahedra[t0], t2, t1, ngbs[5], ngbs[4], 3,
                              3, idx_in_ngb[5], idx_in_ngb[4]);
  tetrahedron_swap_neighbours(&d->tetrahedra[t1], t2, ngbs[3], ngbs[2], t0, 1,
                              idx_in_ngb[3], idx_in_ngb[2], 1);
  tetrahedron_swap_neighbours(&d->tetrahedra[t2], ngbs[0], t1, ngbs[1], t0,
                              idx_in_ngb[0], 0, idx_in_ngb[1], 0);

  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[0]], idx_in_ngb[0], t2, 0);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[1]], idx_in_ngb[1], t2, 2);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[2]], idx_in_ngb[2], t1, 2);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[3]], idx_in_ngb[3], t1, 1);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[4]], idx_in_ngb[4], t0, 3);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[5]], idx_in_ngb[5], t0, 2);

  /* add new/updated tetrahedrons to queue */
  delaunay_tetrahedron_enqueue(d, t0);
  delaunay_tetrahedron_enqueue(d, t1);
  delaunay_tetrahedron_enqueue(d, t2);
}

/**
 * @brief Replace the given four tetrahedra with four new tetrahedra.
 *
 * @image html newvoronoicell_four_to_four_flip.png
 *
 * The four positively oriented tetrahedra (v0 v1 v2 v3), (v0 v1 v3 v4),
 * (v0 v1 v5 v2), and (v0 v5 v1 v4),
 * that share the edge (v0 v1) are replaced by four new positively oriented
 * tetrahedra that share the edge (v3 v5) (dashed line in figure):
 * (v0 v3 v5 v2), (v1 v5 v3 v2), (v0 v5 v3 v4), and (v1 v3 v5 v4).
 *
 * The original red shared faces are hence replaced by the new green shared
 * faces in the figure; the blue shared faces will also be shared by the new
 * tetrahedra.
 *
 * Originally, t0 has ngb0 and ngb3 as neighbours, t1 has ngb1 and ngb2 as
 * neighbours, t2 has ngb4 and ngb7 as neighbours, and t3 has ngb5 and ngb6 as
 * neighbours. After the flip, t'0 has ngb3 and ngb7 as neighbours, t'1 has ngb0
 * and ngb4 as neighbours, t'2 has ngb2 and ngb6 as neighbours, and t'3 has ngb1
 * and ngb5 as neighbours.
 *
 * The tetrahedra should be given to this routine in the expected order: t0
 * should have t1 and t2 as neighbours, while t3 should be a common neighbour of
 * t1 and t2, but not of t3.
 *
 * The first thing we do is figure out how the internal vertex indices of the
 * four tetrahedra map to the names in the figure. We do this by identifying the
 * indices of the common axis vertices v0 and v1 in all tetrahedra, and the
 * index of v2 in t0. Once we know v0, v1 and v2 in t0, we can deduce the index
 * of v3 in in t0, and require that (v0 v1 v2 v3) is positively oriented (if
 * not, we swap v0 and v1 in all tetrahedra so that it is).
 *
 * Once the indices have been mapped, it is straightforward to deduce the
 * actual vertices, neighbours and neighbour indices, and we can simply
 * construct the new tetrahedra based on the figure.
 *
 * @param d Delaunay tessellation
 * @param t0 First tetrahedron.
 * @param t1 Second tetrahedron.
 * @param t2 Third tetrahedron.
 * @param t3 Fourth tetrahedron.
 */
inline static void delaunay_four_to_four_flip(struct delaunay* restrict d,
                                              int t0, int t1, int t2, int t3) {
  // TODO
  fprintf(stderr, "Degenerate case not implemented!");
  abort();
}

/**
 * @brief Replace the given three tetrahedra with two new tetrahedra.
 *
 * @image html newvoronoicell_three_to_two_flip.png
 *
 * The three positively oriented tetrahedra (v0 v1 v2 v4), (v0 v4 v2 v3), and
 * (v4 v1 v2 v3) (with the red common faces in the figure) are
 * replaced with two new positively oriented tetrahedra (with a green common
 * face in the figure): (v0 v1 v2 v3) and (v0 v1 v3 v4).
 *
 * Originally, t0 has ngb4 and ngb5 as neighbours, t1 has ngb2 and ngb3 as
 * neighbours, and t2 has ngb0 and ngb1 as neighbours. After the flip, t'0 has
 * ngb0, ngb3 and ngb4 as neighbours, while t'1 has ngb1, ngb2 and ngb5 as
 * neighbours.
 *
 * We first find the indices of the common axis (v2 v4) in all three tetrahedra,
 * plus the index of v0 (the third common vertex of t0 and t1) in t0. Once we
 * have these we also know the index of v1 in t0, and we can find out which of
 * the two axis indices corresponds to v2 and which to v4 by requiring that the
 * four indices are a positively oriented permutation of 0123. Once this is
 * done, it is very straightforward to obtain the other indices in the other
 * tetrahedra. We can then get the actual vertices, neighbours and neighbour
 * indices, and construct the two new tetrahedra.
 *
 * Note that because this flip removes a tetrahedron, it will free up a spot in
 * the tetrahedra vector. Since removing the tetrahedron from that vector would
 * be very expensive (since it requires a reshuffle of all tetrahedra behind it
 * and requires us to update the neighbour relations for all of these
 * tetrahedra), we just leave it in and keep an extra stack of free spots in the
 * tetrahedra array, which can be filled by other flips.
 *
 * @param d Delaunay tessellation
 * @param t0 First tetrahedron.
 * @param t1 Second tetrahedron.
 * @param t2 Third tetrahedron.
 * @return The index of the freed tetrahedron.
 */
inline static int delaunay_three_to_two_flip(struct delaunay* restrict d,
                                             int t0, int t1, int t2) {
  /* get the common axis of the three tetrahedra */
  int axis[3][4];
  int num_axis = 0;
  for (int i = 0; i < 4; ++i) {
    int idx_in_t0 = i;
    int idx_in_t1 = 0;
    while (idx_in_t1 < 4 && d->tetrahedra[t0].vertices[idx_in_t0] !=
                                d->tetrahedra[t1].vertices[idx_in_t1]) {
      ++idx_in_t1;
    }
    int idx_in_t2 = 0;
    while (idx_in_t2 < 4 && d->tetrahedra[t0].vertices[idx_in_t0] !=
                                d->tetrahedra[t2].vertices[idx_in_t2]) {
      ++idx_in_t2;
    }
    if (idx_in_t1 < 4 && idx_in_t2 < 4) {
      axis[0][num_axis] = idx_in_t0;
      axis[1][num_axis] = idx_in_t1;
      axis[2][num_axis] = idx_in_t2;
      ++num_axis;
    } else {
      if (idx_in_t1 < 4) {
        axis[0][2] = idx_in_t0;
      }
    }
  }
  axis[0][3] = 6 - axis[0][0] - axis[0][1] - axis[0][2];
  if (!positive_permutation(axis[0][2], axis[0][3], axis[0][0], axis[0][1])) {
    int tmp = axis[0][0];
    axis[0][0] = axis[0][1];
    axis[0][1] = tmp;

    tmp = axis[1][0];
    axis[1][0] = axis[1][1];
    axis[1][1] = tmp;

    tmp = axis[2][0];
    axis[2][0] = axis[2][1];
    axis[2][1] = tmp;
  }

  const int v0_0 = axis[0][2];
  const int v1_0 = axis[0][3];
  const int v2_0 = axis[0][0];
  const int v4_0 = axis[0][1];

  const int v2_1 = axis[1][0];
  const int v3_1 = d->tetrahedra[t0].index_in_neighbour[v1_0];
  const int v4_1 = axis[1][1];

  const int v2_2 = axis[2][0];
  const int v4_2 = axis[2][1];

  /* set some variables to the names used in the documentation figure */
  const int vert[5] = {
      d->tetrahedra[t0].vertices[v0_0], d->tetrahedra[t0].vertices[v1_0],
      d->tetrahedra[t0].vertices[v2_0], d->tetrahedra[t1].vertices[v3_1],
      d->tetrahedra[t0].vertices[v4_0],
  };

  const int ngbs[6] = {
      d->tetrahedra[t2].neighbours[v4_2], d->tetrahedra[t2].neighbours[v2_2],
      d->tetrahedra[t1].neighbours[v2_1], d->tetrahedra[t1].neighbours[v4_1],
      d->tetrahedra[t0].neighbours[v4_0], d->tetrahedra[t0].neighbours[v2_0]};

  const int idx_in_ngb[6] = {d->tetrahedra[t2].index_in_neighbour[v4_2],
                             d->tetrahedra[t2].index_in_neighbour[v2_2],
                             d->tetrahedra[t1].index_in_neighbour[v2_1],
                             d->tetrahedra[t1].index_in_neighbour[v4_1],
                             d->tetrahedra[t0].index_in_neighbour[v4_0],
                             d->tetrahedra[t0].index_in_neighbour[v2_0]};

  /* Overwrite two new tetrahedra and free the third one. */
  delaunay_tetrahedron_init(d, t0, vert[0], vert[1], vert[2], vert[3]);
  delaunay_tetrahedron_init(d, t1, vert[0], vert[1], vert[3], vert[4]);
  delaunay_log("Deactivating tetrahedron %i", t2);
  tetrahedron_deactivate(&d->tetrahedra[t2]);

  /* update neighbour relations */
  tetrahedron_swap_neighbours(&d->tetrahedra[t0], ngbs[0], ngbs[3], t1, ngbs[4],
                              idx_in_ngb[0], idx_in_ngb[3], 3, idx_in_ngb[4]);
  tetrahedron_swap_neighbours(&d->tetrahedra[t1], ngbs[1], ngbs[2], ngbs[5], t0,
                              idx_in_ngb[1], idx_in_ngb[2], idx_in_ngb[5], 2);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[0]], idx_in_ngb[0], t0, 0);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[1]], idx_in_ngb[1], t1, 0);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[2]], idx_in_ngb[2], t1, 1);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[3]], idx_in_ngb[3], t0, 1);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[4]], idx_in_ngb[4], t0, 3);
  tetrahedron_swap_neighbour(&d->tetrahedra[ngbs[5]], idx_in_ngb[5], t1, 2);

  /* add updated tetrahedra to queue */
  delaunay_tetrahedron_enqueue(d, t0);
  delaunay_tetrahedron_enqueue(d, t1);

  /* return invalidated tetrahedron */
  return t2;
}

/**
 * @brief Check the Delaunay criterion for tetrahedra in the queue until the
 * queue is empty.
 * @param d Delaunay tessellation
 * @param v The new vertex that might cause invalidation of tetrahedra.
 */
inline static void delaunay_check_tetrahedra(struct delaunay* d, int v) {
  int n_freed = 0;
  int* freed = (int*)malloc(10 * sizeof(int));
  int freed_size = 10;
  int freed_tetrahedron;
  int t = delaunay_tetrahedron_queue_pop(d);
  while (t >= 0) {
    freed_tetrahedron = delaunay_check_tetrahedron(d, t, v);
    /* Did we free a tetrahedron? */
    if (freed_tetrahedron >= 0) {
      if (n_freed >= freed_size) {
        /* Grow array */
        freed_size <<= 1;
        freed = (int*)realloc(freed, freed_size * sizeof(int));
      }
      freed[n_freed] = freed_tetrahedron;
      n_freed++;
    }
    /* Pop next tetrahedron to check */
    t = delaunay_tetrahedron_queue_pop(d);
  }
  /* Enqueue the newly freed tetrahedra indices */
  for (int i = 0; i < n_freed; i++) {
    delaunay_free_index_enqueue(d, freed[i]);
  }
  free(freed);
}

/**
 * @brief Check if the given tetrahedron satisfies the empty circumsphere
 * criterion that marks it as a Delaunay tetrahedron.
 *
 * Per convention, we assume this check was triggered by inserting the final
 * vertex of this tetrahedron, so only one check is required.
 * -> TODO does this work?
 * If this check fails, this function also performs the necessary flips. All
 * new tetrahedra created by this function are also pushed to the queue for
 * checking.
 * @param d Delaunay tessellation
 * @param t The tetrahedron to check.
 * @param v The new vertex that might cause invalidation of the tetrahedron.
 * @return Index of freed tetrahedron, or negative if no tetrahedra are freed
 */
inline static int delaunay_check_tetrahedron(struct delaunay* d, const int t,
                                             const int v) {
  struct tetrahedron* tetrahedron = &d->tetrahedra[t];
  const int v0 = tetrahedron->vertices[0];
  const int v1 = tetrahedron->vertices[1];
  const int v2 = tetrahedron->vertices[2];
  const int v3 = tetrahedron->vertices[3];

  /* Determine which vertex is the newly added vertex */
  int top;
  if (v == v0) {
    top = 0;
  } else if (v == v1) {
    top = 1;
  } else if (v == v2) {
    top = 2;
  } else if (v == v3) {
    top = 3;
  } else {
    fprintf(stderr,
            "Checking tetrahedron %i which does not contain the last added "
            "vertex %i",
            t, v);
    abort();
  }

  /* Get neighbouring tetrahedron opposite of newly added vertex */
  const int ngb = tetrahedron->neighbours[top];
  const int idx_in_ngb = tetrahedron->index_in_neighbour[top];
  /* Get the vertex in the neighbouring tetrahedron opposite of t */
  const int v4 = d->tetrahedra[ngb].vertices[idx_in_ngb];

  /* check if we have a neighbour that can be checked (dummies are not real and
     should not be tested) */
  if (ngb < 4) {
    delaunay_log("Dummy neighbour! Skipping checks for %i...", t);
    delaunay_assert(v4 == -1);
    return -1;
  }

  /* Get the coordinates of all vertices */
#ifdef DELAUNAY_NONEXACT
  // TODO
#endif
  const unsigned long aix = d->integer_vertices[3 * v0];
  const unsigned long aiy = d->integer_vertices[3 * v0 + 1];
  const unsigned long aiz = d->integer_vertices[3 * v0 + 2];

  const unsigned long bix = d->integer_vertices[3 * v1];
  const unsigned long biy = d->integer_vertices[3 * v1 + 1];
  const unsigned long biz = d->integer_vertices[3 * v1 + 2];

  const unsigned long cix = d->integer_vertices[3 * v2];
  const unsigned long ciy = d->integer_vertices[3 * v2 + 1];
  const unsigned long ciz = d->integer_vertices[3 * v2 + 2];

  const unsigned long dix = d->integer_vertices[3 * v3];
  const unsigned long diy = d->integer_vertices[3 * v3 + 1];
  const unsigned long diz = d->integer_vertices[3 * v3 + 2];

  const unsigned long eix = d->integer_vertices[3 * v4];
  const unsigned long eiy = d->integer_vertices[3 * v4 + 1];
  const unsigned long eiz = d->integer_vertices[3 * v4 + 2];

  const int test =
      geometry_in_sphere_exact(&d->geometry, aix, aiy, aiz, bix, biy, biz, cix,
                               ciy, ciz, dix, diy, diz, eix, eiy, eiz);
  if (test < 0) {
    delaunay_log("Tetrahedron %i was invalidated by adding vertex %i", t, v);
    /* Figure out which flip is needed to restore the tetrahedra */
    int tests[4] = {-1, -1, -1, -1};
    if (top != 3) {
      tests[0] = geometry_orient_exact(&d->geometry, aix, aiy, aiz, bix, biy,
                                       biz, cix, ciy, ciz, eix, eiy, eiz);
    }
    if (top != 2) {
      tests[1] = geometry_orient_exact(&d->geometry, aix, aiy, aiz, bix, biy,
                                       biz, eix, eiy, eiz, dix, diy, diz);
    }
    if (top != 1) {
      tests[2] = geometry_orient_exact(&d->geometry, aix, aiy, aiz, eix, eiy,
                                       eiz, cix, ciy, ciz, dix, diy, diz);
    }
    if (top != 0) {
      tests[3] = geometry_orient_exact(&d->geometry, eix, eiy, eiz, bix, biy,
                                       biz, cix, ciy, ciz, dix, diy, diz);
    }
    int i;
    for (i = 0; i < 4 && tests[i] < 0; ++i) {
    }
    if (i == 4) {
      /* v4 inside sphere around v1, v2 and v4: need to do a 2 to 3 flip */
      delaunay_log("Performing 2 to 3 flip with %i and %i", t, ngb);
      delaunay_two_to_three_flip(d, t, ngb, top, idx_in_ngb);
    } else if (tests[i] == 0) {
      /* degenerate case: possible 4 to 4 flip needed. The line that connects v
       * and v4 intersects an edge of the triangle formed by the other 3
       * vertices of t. If that edge is shared by exactly 4 tetrahedra in total,
       * the 2 neighbours are involved in the 4 to 4 flip. If it isn't, we
       * cannot solve this situation now, it will be solved later by another
       * flip. */

      /* the non_axis point is simply the vertex not present in the relevant
       * orientation test */
      const int non_axis = 3 - i;
      /* get the other involved neighbour of t */
      const int other_ngb = d->tetrahedra[t].neighbours[non_axis];
      /* get the index of 'new_vertex' in 'other_ngb', as the neighbour
       * opposite that vertex is the other neighbour we need to check */
      int idx_v_in_other_ngb;
      for (idx_v_in_other_ngb = 0;
           idx_v_in_other_ngb < 4 &&
           d->tetrahedra[other_ngb].vertices[idx_v_in_other_ngb] != v;
           idx_v_in_other_ngb++) {
      }
      const int other_ngbs_ngb =
          d->tetrahedra[other_ngb].neighbours[idx_v_in_other_ngb];
      /* check if other_ngbs_ngb is also a neighbour of ngb. */
      int second_idx_in_ngb =
          tetrahedron_is_neighbour(&d->tetrahedra[ngb], other_ngbs_ngb);
      if (second_idx_in_ngb < 4) {
        delaunay_log("Performing 4 to 4 flip between %i, %i, %i and %i!", t,
                     other_ngb, ngb, other_ngbs_ngb);
        delaunay_four_to_four_flip(d, t, other_ngb, ngb, other_ngbs_ngb);
      } else {
        delaunay_log("4 to 4 with %i and %i flip not possible!", t, ngb);
      }
    } else {
      /* check that this is indeed the only case left */
      delaunay_assert(tests[i] > 0);
      /* Outside: possible 3 to 2 flip.
       * The line that connects 'new_vertex' and 'v4' lies outside an edge of
       * the triangle formed by the other 3 vertices of 'tetrahedron'. We need
       * to check if the neighbouring tetrahedron opposite the non-edge point
       * of that triangle is the same for 't' and 'ngb'. If it is, that is the
       * third tetrahedron for the 3 to 2 flip. If it is not, we cannot solve
       * this faulty situation now, but it will be solved by another flip later
       * on */

      /* the non_axis point is simply the vertex not present in the relevant
       * orientation test */
      const int non_axis = 3 - i;
      /* get the other involved neighbour of t */
      const int other_ngb = d->tetrahedra[t].neighbours[non_axis];
      /* check if other_ngb is also a neigbour of ngb */
      const int other_ngb_idx_in_ngb =
          tetrahedron_is_neighbour(&d->tetrahedra[ngb], other_ngb);
      if (other_ngb_idx_in_ngb < 4) {
        delaunay_log("Performing 3 to 2 flip with %i, %i and %i!", t, ngb,
                     other_ngb);
        return delaunay_three_to_two_flip(d, t, ngb, other_ngb);
      } else {
        delaunay_log("3 to 2 with %i and %i flip not possible!", t, ngb);
      }
    }
  } else {
    delaunay_log("Tetrahedron %i is valid!", t)
  }
  return -1;
}

/**
 * @brief Add the given index to the queue of free indices in the tetrahedra
 * array.
 *
 * @param d Delaunay tessellation.
 * @param idx New free index.
 */
inline static void delaunay_free_index_enqueue(struct delaunay* restrict d,
                                               int idx) {
  /* make sure there is sufficient space in the queue */
  if (d->free_indices_q_index == d->free_indices_q_size) {
    /* there isn't: increase the size of the queue with a factor 2. */
    d->free_indices_q_size <<= 1;
    d->free_indices_queue = (int*)realloc(d->free_indices_queue,
                                          d->free_indices_q_size * sizeof(int));
  }
  delaunay_log("Enqueuing free index %i", idx);
  /* add the free index to the queue and advance the queue index */
  d->free_indices_queue[d->free_indices_q_index] = idx;
  ++d->free_indices_q_index;
}

/**
 * @brief Pop a free index in the tetrahedra array from the end of the queue.
 *
 * If no more indices are queued, this function returns a negative value.
 *
 * Note that the returned index is effectively removed from the queue
 * and will be overwritten by subsequent calls to delaunay_free_index_enqueue().
 *
 * @param d Delaunay tessellation.
 * @return Next free index, or -1 if the queue is empty.
 */
inline static int delaunay_free_indices_queue_pop(struct delaunay* restrict d) {
  if (d->free_indices_q_index > 0) {
    --d->free_indices_q_index;
    return d->free_indices_queue[d->free_indices_q_index];
  } else {
    return -1;
  }
}

/**
 * @brief Add the given tetrahedron to the queue of tetrahedra that need
 * checking.
 *
 * @param d Delaunay tessellation.
 * @param t Tetrahedron index.
 */
inline static void delaunay_tetrahedron_enqueue(struct delaunay* restrict d,
                                                int t) {
  /* make sure there is sufficient space in the queue */
  if (d->tetrahedron_q_index == d->tetrahedron_q_size) {
    /* there isn't: increase the size of the queue with a factor 2. */
    d->tetrahedron_q_size <<= 1;
    d->tetrahedron_queue =
        (int*)realloc(d->tetrahedron_queue, d->tetrahedron_size * sizeof(int));
  }
  delaunay_log("Enqueuing tetrahedron %i", t);
  /* add the tetrahedron to the queue and advance the queue index */
  d->tetrahedron_queue[d->tetrahedron_q_index] = t;
  ++d->tetrahedron_q_index;
}

/**
 * @brief Pop the next active tetrahedron to check from the end of the queue.
 *
 * If no more active tetrahedrons are queued, this function returns a negative
 * value.
 * Note that the returned tetrahedron index is effectively removed from the
 * queue and will be overwritten by subsequent calls to
 * delaunay_tetrahedron_enqueue().
 *
 * @param d Delaunay tessellation.
 * @return Index of the next tetrahedron to test, or -1 if the queue is empty.
 */
inline static int delaunay_tetrahedron_queue_pop(struct delaunay* restrict d) {
  int active = 0;
  int t;
  while (!active && d->tetrahedron_q_index > 0) {
    --d->tetrahedron_q_index;
    t = d->tetrahedron_queue[d->tetrahedron_q_index];
    active = d->tetrahedra[t].active;
  }
  return active ? t : -1;
}

/**
 * @brief Append a tetrahedron containing the current vertex to the array.
 * @param d Delaunay tessellation
 * @param t The tetrahedron containing the current vertex.
 */
inline static void delaunay_append_tetrahedron_containing_vertex(
    struct delaunay* d, const int t) {
  /* Enough space? */
  if (d->tetrahedra_containing_vertex_index ==
      d->tetrahedra_containing_vertex_size) {
    d->tetrahedra_containing_vertex_size <<= 1;
    d->tetrahedra_containing_vertex =
        (int*)realloc(d->tetrahedra_containing_vertex,
                      d->tetrahedra_containing_vertex_size * sizeof(int));
  }
  /* Append and increase index for next tetrahedron */
  d->tetrahedra_containing_vertex[d->tetrahedra_containing_vertex_index] = t;
  d->tetrahedra_containing_vertex_index++;
}

inline static void delaunay_consolidate(struct delaunay* restrict d) {
  /* perform a consistency test if enabled */
  delaunay_check_tessellation(d);
}

inline static void delaunay_print_tessellation(
    const struct delaunay* restrict d, const char* file_name) {
  FILE* file = fopen(file_name, "w");

  for (int i = 0; i < d->vertex_index; ++i) {
    fprintf(file, "V\t%i\t%g\t%g\t%g\n", i, d->vertices[3 * i],
            d->vertices[3 * i + 1], d->vertices[3 * i + 2]);
  }
  for (int i = 4; i < d->tetrahedron_index; ++i) {
    if (!d->tetrahedra[i].active) {
      continue;
    }
    fprintf(file, "T\t%i\t%i\t%i\t%i\n", d->tetrahedra[i].vertices[0],
            d->tetrahedra[i].vertices[1], d->tetrahedra[i].vertices[2],
            d->tetrahedra[i].vertices[3]);
  }

  fclose(file);
}

inline static void delaunay_check_tessellation(struct delaunay* restrict d) {
#ifndef DELAUNAY_CHECKS
  /* No expensive checks will be performed */
  return;
#endif

  /* loop over all non-dummy tetrahedra */
  for (int t0 = 4; t0 < d->tetrahedron_index; t0++) {
    /* Skip temporary deleted tetrahedra */
    if (!d->tetrahedra[t0].active) {
      continue;
    }
    int vt0_0 = d->tetrahedra[t0].vertices[0];
    int vt0_1 = d->tetrahedra[t0].vertices[1];
    int vt0_2 = d->tetrahedra[t0].vertices[2];
    int vt0_3 = d->tetrahedra[t0].vertices[3];
    /* loop over neighbours */
    for (int i = 0; i < 4; i++) {
      int t_ngb = d->tetrahedra[t0].neighbours[i];
      /* check neighbour relations */
      int idx_in_ngb0 = d->tetrahedra[t0].index_in_neighbour[i];
      if (!d->tetrahedra[t_ngb].active) {
        fprintf(stderr, "Tetrahedron %i has an inactive neighbour: %i", t0,
                t_ngb);
      }
      if (d->tetrahedra[t_ngb].neighbours[idx_in_ngb0] != t0) {
        fprintf(stderr, "Wrong neighbour!\n");
        fprintf(stderr, "Tetrahedron %i: %i %i %i %i\n", t0, vt0_0, vt0_1,
                vt0_2, vt0_3);
        fprintf(
            stderr, "\tNeighbours: %i %i %i %i\n",
            d->tetrahedra[t0].neighbours[0], d->tetrahedra[t0].neighbours[1],
            d->tetrahedra[t0].neighbours[2], d->tetrahedra[t0].neighbours[3]);
        fprintf(stderr, "\tIndex in neighbour: %i %i %i %i\n",
                d->tetrahedra[t0].index_in_neighbour[0],
                d->tetrahedra[t0].index_in_neighbour[1],
                d->tetrahedra[t0].index_in_neighbour[2],
                d->tetrahedra[t0].index_in_neighbour[3]);
        fprintf(
            stderr, "Neighbour tetrahedron %i: %i %i %i %i\n", t_ngb,
            d->tetrahedra[t_ngb].vertices[0], d->tetrahedra[t_ngb].vertices[1],
            d->tetrahedra[t_ngb].vertices[2], d->tetrahedra[t_ngb].vertices[3]);
        fprintf(stderr, "\tNeighbours: %i %i %i %i\n",
                d->tetrahedra[t_ngb].neighbours[0],
                d->tetrahedra[t_ngb].neighbours[1],
                d->tetrahedra[t_ngb].neighbours[2],
                d->tetrahedra[t_ngb].neighbours[3]);
        fprintf(stderr, "\tIndex in neighbour: %i %i %i\n",
                d->tetrahedra[t_ngb].index_in_neighbour[0],
                d->tetrahedra[t_ngb].index_in_neighbour[1],
                d->tetrahedra[t_ngb].index_in_neighbour[2],
                d->tetrahedra[t_ngb].index_in_neighbour[3]);
        abort();
      }
      if (t_ngb < 4) {
        /* Don't check delaunayness for dummy neighbour tetrahedra */
        continue;
      }
      /* check in-sphere criterion for delaunayness */
      int vertex_to_check = d->tetrahedra[t_ngb].vertices[idx_in_ngb0];
#ifdef DELAUNAY_NONEXACT
      // TODO
#endif
      unsigned long int aix = d->integer_vertices[3 * vt0_0];
      unsigned long int aiy = d->integer_vertices[3 * vt0_0 + 1];
      unsigned long int aiz = d->integer_vertices[3 * vt0_0 + 2];

      unsigned long int bix = d->integer_vertices[3 * vt0_1];
      unsigned long int biy = d->integer_vertices[3 * vt0_1 + 1];
      unsigned long int biz = d->integer_vertices[3 * vt0_1 + 2];

      unsigned long int cix = d->integer_vertices[3 * vt0_2];
      unsigned long int ciy = d->integer_vertices[3 * vt0_2 + 1];
      unsigned long int ciz = d->integer_vertices[3 * vt0_2 + 2];

      unsigned long int dix = d->integer_vertices[3 * vt0_3];
      unsigned long int diy = d->integer_vertices[3 * vt0_3 + 1];
      unsigned long int diz = d->integer_vertices[3 * vt0_3 + 2];

      unsigned long int eix = d->integer_vertices[3 * vertex_to_check];
      unsigned long int eiy = d->integer_vertices[3 * vertex_to_check + 1];
      unsigned long int eiz = d->integer_vertices[3 * vertex_to_check + 2];

      int test =
          geometry_in_sphere_exact(&d->geometry, aix, aiy, aiz, bix, biy, biz,
                                   cix, ciy, ciz, dix, diy, diz, eix, eiy, eiz);
      if (test < 0) {
        fprintf(stderr, "Failed in-sphere test, value: %i!\n", test);
        fprintf(stderr, "\tTetrahedron %i: %i %i %i %i\n", t0, vt0_0, vt0_1,
                vt0_2, vt0_3);
        fprintf(stderr, "\tOpposite vertex: %i\n", vertex_to_check);
        abort();
      }
    }
  }
}

/**
 * @brief Check if abcd is a positive permutation of 0123 (meaning that if
 * 0123 are the vertices of a positively ordered tetrahedron, then abcd are
 * also the vertices of a positively ordered tetrahedron).
 *
 * @param a First index.
 * @param b Second index.
 * @param c Third index.
 * @param d Fourth index.
 * @return True if abcd is a positively oriented permutation of 0123.
 */
inline static int positive_permutation(int a, int b, int c, int d) {
  if ((a + 1) % 4 == b) {
    return c % 2 == 0;
  } else if ((a + 2) % 4 == b) {
    return b * c + a * d > b * d + a * c;
  } else {
    return d % 2 == 0;
  }
}

#endif  // CVORONOI_DELAUNAY3D_H
