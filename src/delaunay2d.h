/*******************************************************************************
 * This file is part of cVoronoi.
 * Copyright (c) 2020 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

/**
 * @file delaunay2d.h
 *
 * @brief 2D Delaunay tessellation.
 *
 * @author Bert Vandenbroucke (bert.vandenbroucke@ugent.be)
 */

#ifndef CVORONOI_DELAUNAY2D_H
#define CVORONOI_DELAUNAY2D_H

#include <float.h>
#include <math.h>

#include "geometry.h"
#include "hydro_space.h"
#include "triangle.h"

/**
 * @brief Delaunay tessellation.
 *
 * The tessellation contains all the triangles that make up the tessellation,
 * their connectivity is stored implicitly within the triangles themselves.
 */
struct delaunay {

  /* Box geometry2d: used to set up the initial vertices and triangles and to
   * convert input coordinates to integer coordinates. */

  /*! @brief Anchor of the simulation volume. */
  double anchor[2];

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

  /*! @brief Vertex-triangle connections. For every vertex in the tessellation,
   *  this array stores the index of a triangle that contains this vertex
   *  (usually one of the last triangles that was constructed with this vertex
   *  as a member vertex). This array is not required for the incremental
   *  construction algorithm itself, but is indispensable for the conversion
   *  from Delaunay tessellation to Voronoi grid, since it links each input
   *  vertex to one of the triangles it is connected to. Without this array, we
   *  would have no efficient way of finding a triangle that contains a given
   *  vertex. */
  int* vertex_triangles;

  /*! @brief Vertex-triangle connection indices. For every vertex-triangle pair
   *  stored in vertex_triangles, this array contains the index of the vertex
   *  within the vertex list of the triangle. This saves us from having to
   *  loop through the triangle vertices during Voronoi grid construction. */
  int* vertex_triangle_index;

  /*! @brief Vertex search radii. For every vertex, this array contains twice
   *  the radius of the largest circumcircle of the triangles that vertex is
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

  /*! @brief Offset of the ghost vertices. This will be set by
   * delaunay_consolidate() and is used in the construction of the voronoi
   * grid. */
  int ghost_offset;

  /*! @brief Triangles that make up the tessellation. */
  struct triangle* triangles;

  /*! @brief Next available index within the triangle array. Corresponds to the
   *  actual size of the triangle array. */
  int triangle_index;

  /*! @brief Current size of the triangle array in memory. If triangle_size
   *  matches triangle_index, the memory buffer is full and needs to be
   *  expanded. */
  int triangle_size;

  /*! @brief Queue of triangles that need checking during the incremental
   *  construction algorithm. After a new vertex has been added, all new
   *  triangles are added to this queue and are tested to see if the
   *  Delaunay criterion (empty circumcircles) still holds. New triangles
   *  created when flipping invalid triangles are also added to the queue. */
  int* queue;

  /*! @brief Next available index in the queue. Determines both the actual size
   *  of the queue as the first element that will be popped from the queue. */
  int queue_index;

  /*! @brief Current size of the queue in memory. If queue_size matches
   *  queue_index, the memory buffer is full and needs to be expanded. */
  int queue_size;

  /*! @brief Index of the last triangle that was accessed. Used as initial
   *  guess for the triangle that contains the next vertex that will be added.
   *  If vertices are added in some sensible order (e.g. in Peano-Hilbert curve
   *  order) then this will greatly speed up the algorithm. */
  int last_triangle;

  /*! @brief Geometry variables. Auxiliary variables used by the exact integer
   *  geometry2d tests that need to be stored in between tests, since allocating
   *  and deallocating them for every test is too expensive. */
  struct geometry2d geometry;
};

inline static void delaunay_init_vertex(struct delaunay* restrict d,
                                        const int v, double x, double y) {
  /* store a copy of the vertex coordinates (we should get rid of this for
     SWIFT) */
  d->vertices[2 * v] = x;
  d->vertices[2 * v + 1] = y;

  /* compute the rescaled coordinates. We do this because floating point values
     in the range [1,2[ all have the same exponent (0), which guarantees that
     their mantissas form a linear sequence */
  double rescaled_x = 1. + (x - d->anchor[0]) * d->inverse_side;
  double rescaled_y = 1. + (y - d->anchor[1]) * d->inverse_side;

  delaunay_assert(rescaled_x >= 1.);
  delaunay_assert(rescaled_x < 2.);
  delaunay_assert(rescaled_y >= 1.);
  delaunay_assert(rescaled_y < 2.);

#ifdef DELAUNAY_NONEXACT
  /* store a copy of the rescaled coordinates to apply non-exact tests */
  d->rescaled_vertices[2 * v] = rescaled_x;
  d->rescaled_vertices[2 * v + 1] = rescaled_y;
#endif

  /* convert the rescaled coordinates to integer coordinates and store these */
  d->integer_vertices[2 * v] = delaunay_double_to_int(rescaled_x);
  d->integer_vertices[2 * v + 1] = delaunay_double_to_int(rescaled_y);

  /* initialise the variables that keep track of the link between vertices and
     triangles.
     We use negative values so that we can later detect missing links. */
  d->vertex_triangles[v] = -1;
  d->vertex_triangle_index[v] = -1;

  /* initialise the search radii to the largest possible value */
  d->search_radii[v] = DBL_MAX;

  delaunay_log("Initialized new vertex with index %i", v);
}

/**
 * @brief Add a new vertex with the given coordinates.
 *
 * This function first makes sure there is sufficient memory to store the
 * vertex and all its properties. It then converts the vertex coordinates from
 * box coordinates to coordinates in the range [1,2[ and stores the 52-bit
 * mantissas of these coordintes as integer coordinates.
 * The function also initialises all vertex properties to sensible values.
 *
 * @param d Delaunay tessellation.
 * @param x Horizontal coordinate of the vertex.
 * @param y Vertical coordinate of the vertex.
 * @return Index of the new vertex within the vertex array.
 */
inline static int delaunay_new_vertex(struct delaunay* restrict d, double x,
                                      double y) {

  /* check the size of the vertex arrays against the allocated memory size */
  if (d->vertex_index == d->vertex_size) {
    /* dynamically grow the size of the arrays with a factor 2 */
    d->vertex_size <<= 1;
    d->vertices =
        (double*)realloc(d->vertices, d->vertex_size * 2 * sizeof(double));
#ifdef DELAUNAY_NONEXACT
    d->rescaled_vertices = (double*)realloc(
        d->rescaled_vertices, d->vertex_size * 2 * sizeof(double));
#endif
    d->integer_vertices = (unsigned long int*)realloc(
        d->integer_vertices, d->vertex_size * 2 * sizeof(unsigned long int));
    d->vertex_triangles =
        (int*)realloc(d->vertex_triangles, d->vertex_size * sizeof(int));
    d->vertex_triangle_index =
        (int*)realloc(d->vertex_triangle_index, d->vertex_size * sizeof(int));
    d->search_radii =
        (double*)realloc(d->search_radii, d->vertex_size * sizeof(double));
  }

  delaunay_init_vertex(d, d->vertex_index, x, y);

  /* return the vertex index and then increase it by 1.
     After this operation, vertex_index will correspond to the size of the
     vertex arrays and is also the index of the next vertex that will be
     created. */
  return d->vertex_index++;
}

/**
 * @brief Claim a new triangle in the triangle array.
 *
 * This function first ensures that the triangle array is still large enough
 * to hold all triangles, and then returns the index of the next available
 * triangle.
 *
 * @param d Delaunay tessellation.
 * @return Index of the next available triangle in the triangle array.
 */
inline static int delaunay_new_triangle(struct delaunay* restrict d) {

  /* check that we still have triangles available */
  if (d->triangle_index == d->triangle_size) {
    /* no: increase the size of the triangle array with a factor 2 and
       reallocate it in memory */
    d->triangle_size <<= 1;
    d->triangles = (struct triangle*)realloc(
        d->triangles, d->triangle_size * sizeof(struct triangle));
  }

  /* return the triangle index and then increase it by 1.
     After this operation, triangle_index will correspond to the size of the
     triangle array and is also the index of the next triangle that will be
     created. */
  return d->triangle_index++;
}

/**
 * @brief Initialize the Delaunay tessellation.
 *
 * This function allocates memory for all arrays that make up the tessellation
 * and initializes the variables used for bookkeeping.
 *
 * It then sets up a large triangle that contains the entire simulation box and
 * additional buffer space to deal with boundary ghost vertices, and 3
 * additional dummy triangles that provide valid neighbours for the 3 sides of
 * this triangle (these dummy triangles themselves have an invalid tip vertex
 * and are therefore simply placeholders).
 *
 * @param d Delaunay tesselation.
 * @param hs Spatial extents of the simulation box.
 * @param vertex_size Initial size of the vertex array.
 * @param triangle_size Initial size of the triangle array.
 */
inline static void delaunay_init(struct delaunay* restrict d,
                                 const struct hydro_space* restrict hs,
                                 int vertex_size, int triangle_size) {

  /* allocate memory for the vertex arrays */
  d->vertices = (double*)malloc(vertex_size * 2 * sizeof(double));
#ifdef DELAUNAY_NONEXACT
  d->rescaled_vertices = (double*)malloc(vertex_size * 2 * sizeof(double));
#endif
  d->integer_vertices =
      (unsigned long int*)malloc(vertex_size * 2 * sizeof(unsigned long int));
  d->vertex_triangles = (int*)malloc(vertex_size * sizeof(int));
  d->vertex_triangle_index = (int*)malloc(vertex_size * sizeof(int));
  d->search_radii = (double*)malloc(vertex_size * sizeof(double));
  d->vertex_size = vertex_size;
  /* set vertex start and end (indicating where the local vertices start and
   * end)*/
  d->vertex_start = 0;
  d->vertex_end = vertex_size;
  d->ghost_offset = 0;
  /* we add the dummy vertices behind the local vertices and before the ghost
   * vertices (see below) */
  d->vertex_index = vertex_size;

  /* allocate memory for the triangle array */
  d->triangles =
      (struct triangle*)malloc(triangle_size * sizeof(struct triangle));
  d->triangle_index = 0;
  d->triangle_size = triangle_size;

  /* allocate memory for the queue (note that the queue size of 10 was chosen
     arbitrarily, and a proper value should be chosen based on performance
     measurements) */
  d->queue = (int*)malloc(10 * sizeof(int));
  d->queue_index = 0;
  d->queue_size = 10;

  /* determine the size of a box large enough to accommodate the entire
     simulation volume and all possible ghost vertices required to deal with
     boundaries. Note that the box we use is quite big, since it is chosen to
     work for even the very degenerate case of a single vertex that could be
     anywhere in the box. Also note that we convert the generally rectangular
     box to a square. */
  double box_anchor[2] = {hs->anchor[0] - hs->side[0],
                          hs->anchor[1] - hs->side[1]};
  double box_side = 6. * fmax(hs->side[0], hs->side[1]);

  /* store the anchor and inverse side_length for the conversion from box
     coordinates to rescaled (integer) coordinates */
  d->anchor[0] = box_anchor[0];
  d->anchor[1] = box_anchor[1];
  /* the 1.e-13 makes sure converted values are in the range [1, 2[ instead of
   * [1,2] (unlike Springel, 2010) */
  d->inverse_side = (1. - 1.e-13) / box_side;

  /* initialise the structure used to perform exact geometrical tests */
  geometry2d_init(&d->geometry);

  /* set up the large triangle and the 3 dummies */
  /* mind the orientation: counterclockwise w.r.t. the z-axis. */
  int v0 = delaunay_new_vertex(d, box_anchor[0], box_anchor[1]);
  delaunay_log("Creating vertex %i: %g %g", v0, box_anchor[0], box_anchor[1]);
  int v1 = delaunay_new_vertex(d, box_anchor[0] + box_side, box_anchor[1]);
  delaunay_log("Creating vertex %i: %g %g", v1, box_anchor[0] + box_side,
               box_anchor[1]);
  int v2 = delaunay_new_vertex(d, box_anchor[0], box_anchor[1] + box_side);
  delaunay_log("Creating vertex %i: %g %g", v2, box_anchor[0],
               box_anchor[1] + box_side);

  /* we also create 3 dummy triangles with a fake tip (just to create valid
     neighbours for the big triangle */
  int dummy0 = delaunay_new_triangle(d);
  int dummy1 = delaunay_new_triangle(d);
  int dummy2 = delaunay_new_triangle(d);
  int first_triangle = delaunay_new_triangle(d);
  delaunay_log("Creating triangle %i: %i %i %i", dummy0, v1, v0, -1);
  triangle_init(&d->triangles[dummy0], v1, v0, -1);
  triangle_swap_neighbour(&d->triangles[dummy0], 2, first_triangle, 2);
  delaunay_log("Creating triangle %i: %i %i %i", dummy1, v2, v1, -1);
  triangle_init(&d->triangles[dummy1], v2, v1, -1);
  triangle_swap_neighbour(&d->triangles[dummy1], 2, first_triangle, 0);
  delaunay_log("Creating triangle %i: %i %i %i", dummy2, v0, v2, -1);
  triangle_init(&d->triangles[dummy2], v0, v2, -1);
  triangle_swap_neighbour(&d->triangles[dummy2], 2, first_triangle, 1);
  delaunay_log("Creating triangle %i: %i %i %i", first_triangle, v0, v1, v2);
  triangle_init(&d->triangles[first_triangle], v0, v1, v2);
  triangle_swap_neighbour(&d->triangles[first_triangle], 0, dummy1, 2);
  triangle_swap_neighbour(&d->triangles[first_triangle], 1, dummy2, 2);
  triangle_swap_neighbour(&d->triangles[first_triangle], 2, dummy0, 2);

  /* set up the vertex-triangle links for the initial triangle (not for the
     dummies) */
  d->vertex_triangles[v0] = first_triangle;
  d->vertex_triangle_index[v0] = 0;
  d->vertex_triangles[v1] = first_triangle;
  d->vertex_triangle_index[v1] = 1;
  d->vertex_triangles[v2] = first_triangle;
  d->vertex_triangle_index[v2] = 2;

  d->last_triangle = first_triangle;

  /* Perform potential log output and sanity checks */
//  delaunay_log("Post init check");
//  delaunay_check_tessellation(d);
}

/**
 * @brief Randomly choose a neighbour from the given two options.
 *
 * @param ngb0 First option.
 * @param ngb1 Second option.
 * @return One of the neighbours. Both options have a 50% likelihood of being
 * returned based on random sampling using rand().
 */
inline static int delaunay_choose(int ngb0, int ngb1) {
  return (rand() % 2) ? ngb0 : ngb1;
}

/**
 * @brief Test if the given vertex is within the given triangle.
 *
 * This function has 3 possible return values:
 *  - 0: the vertex is not inside the triangle. In this case, t_new is set to
 *    the index of a triangle that is closer to the vertex and should be tested
 *    next.
 *  - 1: the vertex is inside the triangle. t_new is set to t.
 *  - 2: the vertex is on one of the edges of the triangle. t_new is set to the
 *    index on the other side of the edge, while ngb_index is set to the index
 *    of t_new within the neighbour list of t.
 *
 * This function uses arbitrarily exact geometric tests and therefore always
 * returns the correct answer if the vertex coordinates are expressed in
 * (rescaled) integer coordinates. The answer could still be different from the
 * answer that would be obtained when expressing the vertex coordinates in the
 * original box coordinates. However, the exactness of the results in rescaled
 * coordinates guarantees that all geometric tests will always be consistent;
 * if a vertex V is (very) close to the edge between triangles A and B, then the
 * tests for A and B will always agree on whether V is in A or B. Without exact
 * tests, the test for A and B could both be negative, causing the algorithm
 * to get stuck in an endless loop that jumps between both triangles.
 *
 * The geometrical tests used by this function require a certain level of
 * geometrical consistency that will make them fail if the vertices of t are
 * colinear or if the vertex v lies on top of one of the vertices of the
 * triangle. This function will detect these cases and will abort with an error
 * message if they occur.
 *
 * @param d Delaunay tessellation.
 * @param v Vertex index.
 * @param t Triangle index.
 * @param t_new Variable in which the next triangle index is stored (see cases
 * above for the exact meaning).
 * @param ngb_index Variable in which the index of t_new within the neighbour
 * list of t is stored, but only in case 2 (v lies on the edge between t and
 * t_new).
 * @return 0, 1 or 2, as explained above.
 */
inline static int delaunay_test_point_inside_triangle(
    struct delaunay* restrict d, int v, int t, int* t_new, int* ngb_index) {

  delaunay_log("Checking if vertex %i is inside triangle %i", v, t);

  /* make sure we are not testing a dummy triangle. Since the dummies lie
     outside the triangle that is supposed to contain all vertices (including
     all possible ghosts), this should never happen. */
  delaunay_assert(t > 2);

  /* get the vertex positions
     a is the vertex we want to test
     b, c, d are the vertices of the triangle */

  int vt0 = d->triangles[t].vertices[0];
  int vt1 = d->triangles[t].vertices[1];
  int vt2 = d->triangles[t].vertices[2];
  delaunay_log("Triangle vertices: %i %i %i", vt0, vt1, vt2);

#ifdef DELAUNAY_NONEXACT
  double ax = d->rescaled_vertices[2 * v];
  double ay = d->rescaled_vertices[2 * v + 1];

  double bx = d->rescaled_vertices[2 * vt0];
  double by = d->rescaled_vertices[2 * vt0 + 1];

  double cx = d->rescaled_vertices[2 * vt1];
  double cy = d->rescaled_vertices[2 * vt1 + 1];

  double dx = d->rescaled_vertices[2 * vt2];
  double dy = d->rescaled_vertices[2 * vt2 + 1];

  delaunay_log("orient2d: (%g %g) (%g %g) (%g %g) = %g", cx, cy, dx, dy, ax, ay,
               geometry2d_orient(cx, cy, dx, dy, ax, ay));
  double test0 = geometry2d_orient(cx, cy, dx, dy, ax, ay);

  delaunay_log("orient2d: (%g %g) (%g %g) (%g %g) = %g", dx, dy, bx, by, ax, ay,
               geometry2d_orient(dx, dy, bx, by, ax, ay));
  double test1 = geometry2d_orient(dx, dy, bx, by, ax, ay);

  delaunay_log("orient2d: (%g %g) (%g %g) (%g %g) = %g", bx, by, cx, cy, ax, ay,
               geometry2d_orient(bx, by, cx, cy, ax, ay));
  double test2 = geometry2d_orient(bx, by, cx, cy, ax, ay);
#endif

  unsigned long int aix = d->integer_vertices[2 * v];
  unsigned long int aiy = d->integer_vertices[2 * v + 1];

  unsigned long int bix = d->integer_vertices[2 * vt0];
  unsigned long int biy = d->integer_vertices[2 * vt0 + 1];

  unsigned long int cix = d->integer_vertices[2 * vt1];
  unsigned long int ciy = d->integer_vertices[2 * vt1 + 1];

  unsigned long int dix = d->integer_vertices[2 * vt2];
  unsigned long int diy = d->integer_vertices[2 * vt2 + 1];

  /* test all 3 edges of the triangle. This code could potentially be sped up
     by including conditional statements that only execute necessary tests.
     However, it is unclear whether such an approach is beneficial because it
     would introduce additional branching of the code. */
  int testi0 =
      geometry2d_orient_exact(&d->geometry, cix, ciy, dix, diy, aix, aiy);
  int testi1 =
      geometry2d_orient_exact(&d->geometry, dix, diy, bix, biy, aix, aiy);
  int testi2 =
      geometry2d_orient_exact(&d->geometry, bix, biy, cix, ciy, aix, aiy);

#ifdef DELAUNAY_NONEXACT
  delaunay_assert(test0 * testi0 >= 0);
  delaunay_assert(test1 * testi1 >= 0);
  delaunay_assert(test2 * testi2 >= 0);
#endif

  /* to simplify the evaluation of the various scenarios, we combine the test
     results into a single test value that is then checked within a switch
     statement. This approach is overall cleaner than using a bunch of nested
     conditionals. */
  int testsum = (testi0 + 1) << 4 | (testi1 + 1) << 2 | (testi2 + 1);
  switch (testsum) {
    case 1:
    case 2:
      /* testi0 and testi1 are negative */
      *t_new = delaunay_choose(d->triangles[t].neighbours[0],
                               d->triangles[t].neighbours[1]);
      return 0;
    case 4:
    case 8:
      /* testi0 and testi2 are negative */
      *t_new = delaunay_choose(d->triangles[t].neighbours[0],
                               d->triangles[t].neighbours[2]);
      return 0;
    case 5:
    case 6:
    case 9:
    case 10:
      /* testi0 is negative */
      *t_new = d->triangles[t].neighbours[0];
      return 0;
    case 16:
    case 32:
      /* testi1 and testi2 are negative */
      *t_new = delaunay_choose(d->triangles[t].neighbours[1],
                               d->triangles[t].neighbours[2]);
      return 0;
    case 17:
    case 18:
    case 33:
    case 34:
      /* testi1 is negative */
      *t_new = d->triangles[t].neighbours[1];
      return 0;
    case 20:
    case 24:
    case 36:
    case 40:
      /* testi2 is negative */
      *t_new = d->triangles[t].neighbours[2];
      return 0;
    case 26:
      /* testi0 is zero */
      *t_new = d->triangles[t].neighbours[0];
      *ngb_index = 0;
      return 2;
    case 38:
      /* testi1 is zero */
      *t_new = d->triangles[t].neighbours[1];
      *ngb_index = 1;
      return 2;
    case 41:
      /* testi2 is zero */
      *t_new = d->triangles[t].neighbours[2];
      *ngb_index = 2;
      return 2;
    case 42:
      /* all tests returned 1 (testsum is 32 + 8 + 2) */
      *t_new = t;
      return 1;
    default:
      /* we have ended up in a geometrically impossible scenario. This can
         happen if the triangle vertices are colinear, or if one or multiple
         vertices coincide. */
      fprintf(stderr, "Impossible case (%i)!\n", testsum);
      abort();
  }
}

/**
 * @brief Add the given triangle to the queue of triangles that need checking.
 *
 * @param d Delaunay tessellation.
 * @param t Triangle index.
 */
inline static void delaunay_enqueue(struct delaunay* restrict d, int t) {

  /* make sure there is sufficient space in the queue */
  if (d->queue_index == d->queue_size) {
    /* there isn't: increase the size of the queue with a factor 2. */
    d->queue_size <<= 1;
    d->queue = (int*)realloc(d->queue, d->queue_size * sizeof(int));
  }

  delaunay_log("Enqueuing triangle %i and vertex 2", t);

  /* add the triangle to the queue and advance the queue index */
  d->queue[d->queue_index] = t;
  ++d->queue_index;
}

/**
 * @brief Pop the next triangle to check from the end of the queue.
 *
 * If no more triangles are queued, this function returns a negative value.
 *
 * Note that the returned triangle index is effectively removed from the queue
 * and will be overwritten by subsequent calls to delaunay_enqueue().
 *
 * @param d Delaunay tessellation.
 * @return Index of the next triangle to test, or -1 if the queue is empty.
 */
inline static int delaunay_queue_pop(struct delaunay* restrict d) {
  if (d->queue_index > 0) {
    --d->queue_index;
    return d->queue[d->queue_index];
  } else {
    return -1;
  }
}

/**
 * @brief Check the Delaunay criterion for the given triangle.
 *
 * Per convention, we assume that the check is triggered by inserting the
 * final vertex of the given triangle in the tessellation. In this case, only
 * one check is required: we need to test the circumcircle criterion for the
 * top vertex of the neighbouring triangle opposite the new vertex (so the
 * vertex that is not shared between this triangle and that neighbour).
 *
 * This function is also responsible for performing the edge flip that restores
 * Delaunayhood in case the test fails. This boils down to erasing the edge that
 * is shared between this triangle and its neighbour and constructing a new
 * edge between the newly inserted vertex and the opposite vertex in the
 * neighbour. After this operation, the two newly created triangles need to be
 * checked for Delaunayhood themselves and are added to the end of the queue.
 *
 * As for delaunay_test_point_inside_triangle(), this function makes use of
 * arbitrarily exact geometric tests that guarantee consistency of test results
 * within the rescaled (integer) coordinate frame. This is to avoid situations
 * in which newly created triangles would appear to violate the Delaunay
 * criterion and restore a previously flipped edge, leading to an endless loop
 * and a catastrophic deadlock of the algorithm.
 *
 * @param d Delaunay tessellation.
 * @param t Triangle index.
 */
inline static void delaunay_check_triangle(struct delaunay* restrict d, int t) {

  delaunay_log("Checking triangle %i and vertex 2", t);

  int t2 = d->triangles[t].neighbours[2];
  /* check if we have a neighbour that can be checked (dummies are not real and
     should not be tested) */
  if (t2 < 3) {
    delaunay_log("No neighbour to check");
    return;
  }

  /* get the vertex positions
     a, b, c are the vertices of the triangle
     d is the vertex we want to test */
  int vt1_0 = d->triangles[t].vertices[0];
  int vt1_1 = d->triangles[t].vertices[1];
  int vt1_2 = d->triangles[t].vertices[2];

  delaunay_log("Vertices: %i %i %i", vt1_0, vt1_1, vt1_2);

  int i0 = d->triangles[t].index_in_neighbour[2];
  int vt2_0 = d->triangles[t2].vertices[i0];

  delaunay_log("Opposite vertex: %i", vt2_0);

#ifdef DELAUNAY_NONEXACT
  double ax = d->rescaled_vertices[2 * vt1_0];
  double ay = d->rescaled_vertices[2 * vt1_0 + 1];

  double bx = d->rescaled_vertices[2 * vt1_1];
  double by = d->rescaled_vertices[2 * vt1_1 + 1];

  double cx = d->rescaled_vertices[2 * vt1_2];
  double cy = d->rescaled_vertices[2 * vt1_2 + 1];

  double dx = d->rescaled_vertices[2 * vt2_0];
  double dy = d->rescaled_vertices[2 * vt2_0 + 1];

  double test = geometry2d_in_sphere(ax, ay, bx, by, cx, cy, dx, dy);
  delaunay_log("In circle: (%g %g) (%g %g) (%g %g) (%g %g) (%g) = %g", ax, ay,
               bx, by, cx, cy, dx, dy,
               geometry2d_orient(ax, ay, bx, by, cx, cy), test);
#endif

  unsigned long int aix = d->integer_vertices[2 * vt1_0];
  unsigned long int aiy = d->integer_vertices[2 * vt1_0 + 1];

  unsigned long int bix = d->integer_vertices[2 * vt1_1];
  unsigned long int biy = d->integer_vertices[2 * vt1_1 + 1];

  unsigned long int cix = d->integer_vertices[2 * vt1_2];
  unsigned long int ciy = d->integer_vertices[2 * vt1_2 + 1];

  unsigned long int dix = d->integer_vertices[2 * vt2_0];
  unsigned long int diy = d->integer_vertices[2 * vt2_0 + 1];

  int testi = geometry2d_in_sphere_exact(&d->geometry, aix, aiy, bix, biy, cix,
                                         ciy, dix, diy);

#ifdef DELAUNAY_NONEXACT
  delaunay_assert(test * testi >= 0);
#endif

  /* check if we need to flip the edge */
  if (testi > 0) {
    delaunay_log("Flipping triangle");

    /* flip edge. We first obtain the indices of the edge vertices in the
       neighbouring triangle (assuming they are correctly oriented) */
    int i1 = (i0 + 1) % 3;
    int i2 = (i0 + 2) % 3;
    delaunay_assert(d->triangles[t2].vertices[i1] == vt1_1);
    delaunay_assert(d->triangles[t2].vertices[i2] == vt1_0);

    /* obtain all the neighbouring information for the 4 neighbours of the
       two triangles that share the edge we want to flip */
    int ngb0 = d->triangles[t].neighbours[1];
    int ngbi0 = d->triangles[t].index_in_neighbour[1];

    int ngb1 = d->triangles[t].neighbours[0];
    int ngbi1 = d->triangles[t].index_in_neighbour[0];

    int ngb2 = d->triangles[t2].neighbours[i2];
    int ngbi2 = d->triangles[t2].index_in_neighbour[i2];

    int ngb3 = d->triangles[t2].neighbours[i1];
    int ngbi3 = d->triangles[t2].index_in_neighbour[i1];

    /* now create the 2 new triangles. We reuse the triangle indices of the
       original triangles. It helps enormously to make a sketch of the 2
       triangles in order to see that the neighbour relations below are
       correct. Also note that for future checks to work, we need to stick to
       the convention that the newly inserted vertex (vt1_2) is the last vertex
       of the new triangles. */
    delaunay_log("Creating triangle %i: %i %i %i", t, vt1_0, vt2_0, vt1_2);
    triangle_init(&d->triangles[t], vt1_0, vt2_0, vt1_2);
    triangle_swap_neighbour(&d->triangles[t], 0, t2, 1);
    triangle_swap_neighbour(&d->triangles[t], 1, ngb0, ngbi0);
    triangle_swap_neighbour(&d->triangles[ngb0], ngbi0, t, 1);
    triangle_swap_neighbour(&d->triangles[t], 2, ngb3, ngbi3);
    triangle_swap_neighbour(&d->triangles[ngb3], ngbi3, t, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t2, vt2_0, vt1_1, vt1_2);
    triangle_init(&d->triangles[t2], vt2_0, vt1_1, vt1_2);
    triangle_swap_neighbour(&d->triangles[t2], 0, ngb1, ngbi1);
    triangle_swap_neighbour(&d->triangles[ngb1], ngbi1, t2, 0);
    triangle_swap_neighbour(&d->triangles[t2], 1, t, 0);
    triangle_swap_neighbour(&d->triangles[t2], 2, ngb2, ngbi2);
    triangle_swap_neighbour(&d->triangles[ngb2], ngbi2, t2, 2);

    /* update the vertex-triangle links */
    d->vertex_triangles[vt1_0] = t;
    d->vertex_triangle_index[vt1_0] = 0;
    d->vertex_triangles[vt1_1] = t2;
    d->vertex_triangle_index[vt1_1] = 1;
    d->vertex_triangles[vt1_2] = t2;
    d->vertex_triangle_index[vt1_2] = 2;
    d->vertex_triangles[vt2_0] = t2;
    d->vertex_triangle_index[vt2_0] = 0;

    /* add the new triangles to the queue for checking */
    delaunay_enqueue(d, t);
    delaunay_enqueue(d, t2);

    /* store the index of t2 as starting point for future vertex insertions */
    d->last_triangle = t2;
  }
}

/**
 * @brief Test the Delaunay criterion for all triangles in the queue after a
 * vertex insertion event.
 *
 * @param d Delaunay tessellation.
 */
inline static void delaunay_check_triangles(struct delaunay* restrict d) {
  int t = delaunay_queue_pop(d);
  while (t >= 0) {
    delaunay_check_triangle(d, t);
    t = delaunay_queue_pop(d);
  }
}

/**
 * @brief Add a new vertex to the tessellation.
 *
 * This function locates the triangle in the current
 * tessellation that contains the new vertex. This triangle is split into 3 new
 * triangles by combining the 3 edges of the original triangle with the new
 * vertex. If the new vertex degenerately lies on the edge between two existing
 * triangles, those two triangles are split into 4 new triangles by connecting
 * the new vertex with the two vertices that are not on that edge.
 *
 * After the creation of 3 or 4 new triangles, all new triangles are added to a
 * queue and are tested against the Delaunay criterion. If required, additional
 * edge flips are executed to restore Delaunayhood. To enable efficient testing,
 * it is important that all new triangles satisfy two conventions: their
 * vertices should always be ordered counterclockwise, and the newly inserted
 * vertex should always be the final vertex of any newly created triangle.
 *
 * @param d Delaunay tessellation.
 * @param v Index of new vertex
 */
inline static void delaunay_add_vertex(struct delaunay* restrict d, int v) {

  /* find the triangle that contains the new vertex. Starting from an initial
     guess (the last triangle accessed by the previous insertion), we test if
     the vertex is inside or on the edge of the triangle. If the test finds the
     vertex to be outside of the triangle, a better guess is obtained by taking
     a neighbouring triangle of the current triangle that is closer to the
     current guess. */
  int t0, t1, ngb_index;
  t0 = d->last_triangle;
  int flag = delaunay_test_point_inside_triangle(d, v, t0, &t1, &ngb_index);
  int count = 0;
  while (flag == 0) {
    t0 = t1;
    flag = delaunay_test_point_inside_triangle(d, v, t0, &t1, &ngb_index);
    ++count;
    delaunay_assert(count < d->triangle_index);
  }
  delaunay_log("Found triangle: %i (flag %i)", t0, flag);

  /* check wheter we are in the normal (flag = 1) or degenerate (flag = 2)
     case */
  if (flag == 1) {
    /* normal case: split t0 into 3 new triangles */

    /* retrieve the properties of the old triangle */
    int tv0 = d->triangles[t0].vertices[0];
    int tv1 = d->triangles[t0].vertices[1];
    int tv2 = d->triangles[t0].vertices[2];
    int ngb0 = d->triangles[t0].neighbours[0];
    int ngb1 = d->triangles[t0].neighbours[1];
    int ngb2 = d->triangles[t0].neighbours[2];
    int ngbi0 = d->triangles[t0].index_in_neighbour[0];
    int ngbi1 = d->triangles[t0].index_in_neighbour[1];
    int ngbi2 = d->triangles[t0].index_in_neighbour[2];

    /* create 3 new triangles (drawing the triangles helps to check that the
       code below is indeed correct) */
    t1 = delaunay_new_triangle(d);
    int t2 = delaunay_new_triangle(d);
    delaunay_log("Creating triangle %i: %i %i %i", t0, tv0, tv1, v);
    triangle_init(&d->triangles[t0], tv0, tv1, v);
    triangle_swap_neighbour(&d->triangles[t0], 0, t1, 1);
    triangle_swap_neighbour(&d->triangles[t0], 1, t2, 0);
    triangle_swap_neighbour(&d->triangles[t0], 2, ngb2, ngbi2);
    triangle_swap_neighbour(&d->triangles[ngb2], ngbi2, t0, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t1, tv1, tv2, v);
    triangle_init(&d->triangles[t1], tv1, tv2, v);
    triangle_swap_neighbour(&d->triangles[t1], 0, t2, 1);
    triangle_swap_neighbour(&d->triangles[t1], 1, t0, 0);
    triangle_swap_neighbour(&d->triangles[t1], 2, ngb0, ngbi0);
    triangle_swap_neighbour(&d->triangles[ngb0], ngbi0, t1, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t2, tv2, tv0, v);
    triangle_init(&d->triangles[t2], tv2, tv0, v);
    triangle_swap_neighbour(&d->triangles[t2], 0, t0, 1);
    triangle_swap_neighbour(&d->triangles[t2], 1, t1, 0);
    triangle_swap_neighbour(&d->triangles[t2], 2, ngb1, ngbi1);
    triangle_swap_neighbour(&d->triangles[ngb1], ngbi1, t2, 2);

    /* update the vertex-triangle links */
    d->vertex_triangles[tv0] = t0;
    d->vertex_triangle_index[tv0] = 0;
    d->vertex_triangles[tv1] = t1;
    d->vertex_triangle_index[tv1] = 0;
    d->vertex_triangles[tv2] = t2;
    d->vertex_triangle_index[tv2] = 0;
    d->vertex_triangles[v] = t2;
    d->vertex_triangle_index[v] = 2;

    /* add the triangles to the queue for checking */
    delaunay_enqueue(d, t0);
    delaunay_enqueue(d, t1);
    delaunay_enqueue(d, t2);

    /* touch the last triangle; if nothing else happens, this triangle will be
       the initial guess for the next insertion */
    d->last_triangle = t2;
  } else {

    /* degenerate case: t0 and t1 are split into 4 new triangles */

    delaunay_log("Degenerate insertion!");

    delaunay_log("t0: %i, t1: %i, ngb_index: %i", t0, t1, ngb_index);
    delaunay_log("ngbs0: %i %i %i", d->triangles[t0].neighbours[0],
                 d->triangles[t0].neighbours[1],
                 d->triangles[t0].neighbours[2]);
    delaunay_log("ngbs1: %i %i %i", d->triangles[t1].neighbours[0],
                 d->triangles[t1].neighbours[1],
                 d->triangles[t1].neighbours[2]);

    delaunay_assert(d->triangles[t0].neighbours[ngb_index] == t1);

    /* the new vertex lies on the edge separating triangles t0 and t1
       this edge lies opposite vertex ngb_index
       we need to split t0 and t1 into 4 new triangles */
    int i0_1 = (ngb_index + 1) % 3;
    int i0_2 = (ngb_index + 2) % 3;

    int vt0_0 = d->triangles[t0].vertices[ngb_index];
    int vt0_1 = d->triangles[t0].vertices[i0_1];
    int vt0_2 = d->triangles[t0].vertices[i0_2];

    int i1_0 = d->triangles[t0].index_in_neighbour[ngb_index];
    int i1_1 = (i1_0 + 1) % 3;
    int i1_2 = (i1_0 + 2) % 3;

    delaunay_assert(d->triangles[t1].neighbours[i1_0] == t0);
    delaunay_assert(d->triangles[t1].vertices[i1_1] == vt0_2);
    delaunay_assert(d->triangles[t1].vertices[i1_2] == vt0_1);

    int vt1_0 = d->triangles[t1].vertices[i1_0];

    delaunay_log("Triangle %i: %i %i %i", t0, vt0_0, vt0_1, vt0_2);
    delaunay_log("Triangle %i: %i %i %i", t1, vt1_0, vt0_2, vt0_1);

    int ngb0_1 = d->triangles[t0].neighbours[i0_1];
    int ngbi0_1 = d->triangles[t0].index_in_neighbour[i0_1];
    int ngb0_2 = d->triangles[t0].neighbours[i0_2];
    int ngbi0_2 = d->triangles[t0].index_in_neighbour[i0_2];

    int ngb1_1 = d->triangles[t1].neighbours[i1_1];
    int ngbi1_1 = d->triangles[t1].index_in_neighbour[i1_1];
    int ngb1_2 = d->triangles[t1].neighbours[i1_2];
    int ngbi1_2 = d->triangles[t1].index_in_neighbour[i1_2];

    int t2 = delaunay_new_triangle(d);
    int t3 = delaunay_new_triangle(d);

    /* as always, a drawing is very helpful to check the code below */
    delaunay_log("Creating triangle %i: %i %i %i", t0, vt0_0, vt0_1, v);
    triangle_init(&d->triangles[t0], vt0_0, vt0_1, v);
    triangle_swap_neighbour(&d->triangles[t0], 0, t2, 1);
    triangle_swap_neighbour(&d->triangles[t0], 1, t1, 0);
    triangle_swap_neighbour(&d->triangles[t0], 2, ngb0_2, ngbi0_2);
    triangle_swap_neighbour(&d->triangles[ngb0_2], ngbi0_2, t0, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t1, vt0_2, vt0_0, v);
    triangle_init(&d->triangles[t1], vt0_2, vt0_0, v);
    triangle_swap_neighbour(&d->triangles[t1], 0, t0, 1);
    triangle_swap_neighbour(&d->triangles[t1], 1, t3, 0);
    triangle_swap_neighbour(&d->triangles[t1], 2, ngb0_1, ngbi0_1);
    triangle_swap_neighbour(&d->triangles[ngb0_1], ngbi0_1, t1, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t2, vt0_1, vt1_0, v);
    triangle_init(&d->triangles[t2], vt0_1, vt1_0, v);
    triangle_swap_neighbour(&d->triangles[t2], 0, t3, 1);
    triangle_swap_neighbour(&d->triangles[t2], 1, t0, 0);
    triangle_swap_neighbour(&d->triangles[t2], 2, ngb1_1, ngbi1_1);
    triangle_swap_neighbour(&d->triangles[ngb1_1], ngbi1_1, t2, 2);

    delaunay_log("Creating triangle %i: %i %i %i", t3, vt1_0, vt0_2, v);
    triangle_init(&d->triangles[t3], vt1_0, vt0_2, v);
    triangle_swap_neighbour(&d->triangles[t3], 0, t1, 1);
    triangle_swap_neighbour(&d->triangles[t3], 1, t2, 0);
    triangle_swap_neighbour(&d->triangles[t3], 2, ngb1_2, ngbi1_2);
    triangle_swap_neighbour(&d->triangles[ngb1_2], ngbi1_2, t3, 2);

    /* update the triangle-vertex connections */
    d->vertex_triangles[vt0_0] = t0;
    d->vertex_triangle_index[vt0_0] = 0;
    d->vertex_triangles[vt0_1] = t0;
    d->vertex_triangle_index[vt0_1] = 1;
    d->vertex_triangles[vt0_2] = t1;
    d->vertex_triangle_index[vt0_2] = 0;
    d->vertex_triangles[vt1_0] = t2;
    d->vertex_triangle_index[vt1_0] = 1;
    d->vertex_triangles[v] = t0;
    d->vertex_triangle_index[v] = 2;

    /* add the new triangles to the queue for checking */
    delaunay_enqueue(d, t0);
    delaunay_enqueue(d, t1);
    delaunay_enqueue(d, t2);
    delaunay_enqueue(d, t3);

    /* touch the last triangle; if nothing else happens, this triangle will be
       the initial guess for the next insertion */
    d->last_triangle = t3;
  }

  /* now process all triangles in the test queue */
  delaunay_check_triangles(d);
  delaunay_log("Post vertex %i check", v);
}

inline static void delaunay_add_local_vertex(struct delaunay* restrict d, int v,
                                             double x, double y, double z) {
  delaunay_assert(v < d->vertex_end && d->vertex_start <= v);
  delaunay_init_vertex(d, v, x, y);
  delaunay_log("Adding local vertex with position %g %g", x, y);
  delaunay_add_vertex(d, v);
}

inline static void delaunay_add_new_vertex(struct delaunay* restrict d,
                                           double x, double y) {
  int v = delaunay_new_vertex(d, x, y);
  delaunay_log("Created new vertex with position %g %g", x, y);
  delaunay_add_vertex(d, v);
}

/**
 * @brief Free up all memory associated with the Delaunay tessellation.
 *
 * @param d Delaunay tessellation.
 */
inline static void delaunay_destroy(struct delaunay* restrict d) {
  free(d->vertices);
#ifdef DELAUNAY_NONEXACT
  free(d->rescaled_vertices);
#endif
  free(d->integer_vertices);
  free(d->vertex_triangles);
  free(d->vertex_triangle_index);
  free(d->search_radii);
  free(d->triangles);
  free(d->queue);
  geometry2d_destroy(&d->geometry);
}

/**
 * @brief Get the radius of the circumcircle of the given triangle.
 *
 * @param d Delaunay tessellation.
 * @param t Triangle index.
 * @return Radius of the circumcircle of the triangle.
 */
inline static double delaunay_get_radius(const struct delaunay* restrict d,
                                         int t) {
  int v0 = d->triangles[t].vertices[0];
  int v1 = d->triangles[t].vertices[1];
  int v2 = d->triangles[t].vertices[2];

  double v0x = d->vertices[2 * v0];
  double v0y = d->vertices[2 * v0 + 1];
  double v1x = d->vertices[2 * v1];
  double v1y = d->vertices[2 * v1 + 1];
  double v2x = d->vertices[2 * v2];
  double v2y = d->vertices[2 * v2 + 1];

  double ax = v1x - v0x;
  double ay = v1y - v0y;
  double bx = v2x - v0x;
  double by = v2y - v0y;

  double D = 2. * (ax * by - ay * bx);
  double a2 = ax * ax + ay * ay;
  double b2 = bx * bx + by * by;
  double Rx = (by * a2 - ay * b2) / D;
  double Ry = (ax * b2 - bx * a2) / D;

  return sqrt(Rx * Rx + Ry * Ry);
}

/**
 * @brief Update the search radii of all vertices based on the given radius.
 *
 * If the current search radius of a vertex is larger than the given value,
 * the search radius is recomputed based on all the triangles that vertex is
 * part of (and set to twice the largest circumcircle radius among those
 * triangles). This function also counts the vertices for which this updated
 * radius is still larger than the given radius.
 *
 * This function is meant to be called after all ghost vertices with a distance
 * smaller than the given radius to all of the vertices have been added to the
 * tessellation.
 *
 * @param d Delaunay tessellation.
 * @param r Radius.
 * @return Number of vertices with a search radius larger than the given radius.
 */
inline static int delaunay_update_search_radii(struct delaunay* restrict d,
                                               double r) {
  int count = 0;
  for (int i = d->vertex_start; i < d->vertex_end; ++i) {
    if (d->search_radii[i] > r) {
      int t0 = d->vertex_triangles[i];
      int vi0 = d->vertex_triangle_index[i];
      int vi0p1 = (vi0 + 1) % 3;
      d->search_radii[i] = 2. * delaunay_get_radius(d, t0);
      int t1 = d->triangles[t0].neighbours[vi0p1];
      int vi1 = d->triangles[t0].index_in_neighbour[vi0p1];
      while (t1 != t0) {
        d->search_radii[i] =
            fmax(d->search_radii[i], 2. * delaunay_get_radius(d, t1));
        int vi1p2 = (vi1 + 2) % 3;
        vi1 = d->triangles[t1].index_in_neighbour[vi1p2];
        t1 = d->triangles[t1].neighbours[vi1p2];
      }
      if (d->search_radii[i] > r) {
        ++count;
      }
    }
  }
  return count;
}

/**
 * @brief Perform an (expensive) check on the tessellation to see that it is
 * still valid.
 *
 * This function will iterate over all triangles (except the 3 dummy triangles)
 * of the tessellation. For each triangle, it checks that all neighbour
 * relations for that triangle are set correctly (i.e. if triangle A is a
 * neighbour of B, B should always also be a neighbour of A; if triangle A
 * thinks it is the ith neighbour in triangle B, it should also be found in the
 * ith neighbour position in triangle B), and that none fo the vertices of
 * neighbouring triangles are within the circumcircle of the triangle, which
 * would violate the Delaunay criterion.
 *
 * Finally, this function also checks the vertex-triangle information by
 * making sure that all triangle indices stored in vertex_triangles and
 * vertex_triangle_index are correct.
 *
 * The function will abort with an error if any problems are found with the
 * tessellation.
 *
 * This function returns immediately if DELAUNAY_CHECKS in inactive. It adds
 * a significant extra runtime cost and should never be used in production runs.
 *
 * @param d Delaunay tessellation (note that this parameter cannot be const as
 * one might suspect, since the geometrical test variables are used to test
 * the Delaunay criterion and they cannot be read-only).
 */
inline static void delaunay_check_tessellation(struct delaunay* restrict d) {

#ifndef DELAUNAY_CHECKS
  /* return immediately if we do not want to run the expensive checks */
  return;
#endif

  /* Loop over all triangles. We skip the dummy triangles, since their
     neighbour relations are (on purpose) incorrect. */
  for (int i = 3; i < d->triangle_index; ++i) {
    int vt1_0 = d->triangles[i].vertices[0];
    int vt1_1 = d->triangles[i].vertices[1];
    int vt1_2 = d->triangles[i].vertices[2];

#ifdef DELAUNAY_NONEXACT
    double ax = d->rescaled_vertices[2 * vt1_0];
    double ay = d->rescaled_vertices[2 * vt1_0 + 1];

    double bx = d->rescaled_vertices[2 * vt1_1];
    double by = d->rescaled_vertices[2 * vt1_1 + 1];

    double cx = d->rescaled_vertices[2 * vt1_2];
    double cy = d->rescaled_vertices[2 * vt1_2 + 1];
#endif

    unsigned long int aix = d->integer_vertices[2 * vt1_0];
    unsigned long int aiy = d->integer_vertices[2 * vt1_0 + 1];

    unsigned long int bix = d->integer_vertices[2 * vt1_1];
    unsigned long int biy = d->integer_vertices[2 * vt1_1 + 1];

    unsigned long int cix = d->integer_vertices[2 * vt1_2];
    unsigned long int ciy = d->integer_vertices[2 * vt1_2 + 1];

    /* loop over the 3 neighbours of this triangle */
    for (int j = 0; j < 3; ++j) {
      int ngb = d->triangles[i].neighbours[j];
      /* skip potential dummy neighbours, since they have an invalid vertex
         that will break the Delaunay test. */
      if (ngb > 2) {
        int i0 = d->triangles[i].index_in_neighbour[j];

        /* check the mutual neighbour relations */
        if (d->triangles[ngb].neighbours[i0] != i) {
          fprintf(stderr, "Wrong neighbour!\n");
          fprintf(stderr, "Triangle %i: %i %i %i\n", i, vt1_0, vt1_1, vt1_2);
          fprintf(stderr, "Neighbours: %i %i %i\n",
                  d->triangles[i].neighbours[0], d->triangles[i].neighbours[1],
                  d->triangles[i].neighbours[2]);
          fprintf(stderr, "Index in neighbour: %i %i %i\n",
                  d->triangles[i].index_in_neighbour[0],
                  d->triangles[i].index_in_neighbour[1],
                  d->triangles[i].index_in_neighbour[2]);
          fprintf(stderr, "Neighbour triangle %i: %i %i %i\n", ngb,
                  d->triangles[ngb].vertices[0], d->triangles[ngb].vertices[1],
                  d->triangles[ngb].vertices[2]);
          fprintf(
              stderr, "Neighbours: %i %i %i\n", d->triangles[ngb].neighbours[0],
              d->triangles[ngb].neighbours[1], d->triangles[ngb].neighbours[2]);
          fprintf(stderr, "Index in neighbour: %i %i %i\n",
                  d->triangles[ngb].index_in_neighbour[0],
                  d->triangles[ngb].index_in_neighbour[1],
                  d->triangles[ngb].index_in_neighbour[2]);
          abort();
        }

        /* check the neighbour index information */
        if (d->triangles[ngb].index_in_neighbour[i0] != j) {
          fprintf(stderr, "Wrong neighbour!\n");
          fprintf(stderr, "Triangle %i: %i %i %i\n", i, vt1_0, vt1_1, vt1_2);
          fprintf(stderr, "Neighbours: %i %i %i\n",
                  d->triangles[i].neighbours[0], d->triangles[i].neighbours[1],
                  d->triangles[i].neighbours[2]);
          fprintf(stderr, "Index in neighbour: %i %i %i\n",
                  d->triangles[i].index_in_neighbour[0],
                  d->triangles[i].index_in_neighbour[1],
                  d->triangles[i].index_in_neighbour[2]);
          fprintf(stderr, "Neighbour triangle %i: %i %i %i\n", ngb,
                  d->triangles[ngb].vertices[0], d->triangles[ngb].vertices[1],
                  d->triangles[ngb].vertices[2]);
          fprintf(
              stderr, "Neighbours: %i %i %i\n", d->triangles[ngb].neighbours[0],
              d->triangles[ngb].neighbours[1], d->triangles[ngb].neighbours[2]);
          fprintf(stderr, "Index in neighbour: %i %i %i\n",
                  d->triangles[ngb].index_in_neighbour[0],
                  d->triangles[ngb].index_in_neighbour[1],
                  d->triangles[ngb].index_in_neighbour[2]);
          abort();
        }

        /* now get the vertex that is not shared between the triangle and this
           neighbour and make sure it is not in the triangle's circumcircle */
        int vt2_0 = d->triangles[ngb].vertices[i0];

#ifdef DELAUNAY_NONEXACT
        double dx = d->rescaled_vertices[2 * vt2_0];
        double dy = d->rescaled_vertices[2 * vt2_0 + 1];
        double test = geometry2d_in_sphere(ax, ay, bx, by, cx, cy, dx, dy);
#else
        double test = -1.;
#endif

        unsigned long dix = d->integer_vertices[2 * vt2_0];
        unsigned long diy = d->integer_vertices[2 * vt2_0 + 1];

        int testi = geometry2d_in_sphere_exact(&d->geometry, aix, aiy, bix, biy,
                                               cix, ciy, dix, diy);
        if (test > 0. || testi > 0) {
          fprintf(stderr, "Wrong triangle!\n");
          fprintf(stderr, "Triangle %i: %i (%g %g) %i (%g %g) %i (%g %g)\n", i,
                  vt1_0, d->vertices[2 * vt1_0], d->vertices[2 * vt1_0 + 1],
                  vt1_1, d->vertices[2 * vt1_1], d->vertices[2 * vt1_1 + 1],
                  vt1_2, d->vertices[2 * vt1_2], d->vertices[2 * vt1_2 + 1]);
          fprintf(stderr, "Opposite vertex: %i (%g %g)\n", vt2_0,
                  d->vertices[2 * vt2_0], d->vertices[2 * vt2_0 + 1]);
          fprintf(stderr, "Test result: %g\n", test);
#ifdef DELAUNAY_NONEXACT
          fprintf(stderr, "Orientation: %g\n",
                  geometry2d_orient(ax, ay, bx, by, cx, cy));
#endif
          abort();
        }
      }
    }
  }

  /* Loop over all vertex_triangle elements and check that they indeed link
     to triangles that contain their respective vertex */
  for (int i = 0; i < d->vertex_index; ++i) {
    int t = d->vertex_triangles[i];
    int vi = d->vertex_triangle_index[i];
    if (d->triangles[t].vertices[vi] != i) {
      fprintf(stderr, "Wrong vertex-triangle connection!");
      abort();
    }
  }
}

/**
 * @brief Consolidate the Delaunay tessellation. This signals the end of the
 * addition of normal vertices. All vertices added after this point are
 * considered to be ghost vertices.
 *
 * This function will also enable running delaunay_update_search_radii() and
 * will make it possible to construct a Voronoi grid based on the tessellation.
 *
 * @param d Delaunay tessellation.
 */
inline static void delaunay_consolidate(struct delaunay* restrict d) {
  /* Set ghost offset. Any vertices added from this point onward will be
   * considered ghost vertices. */
  d->ghost_offset = d->vertex_index;
  /* perform a consistency test if enabled */
  delaunay_check_tessellation(d);
}

/**
 * @brief Output the tessellation to a text file with the given name.
 *
 * The tessellation is output as follows:
 *  1. All the vertices are output in order, as individual lines with the
 *     format "V\tindex\tx\ty\n", where x and y are the original indices of
 *     the vertex in box coordinates.
 *  2. All triangles are output as individual lines with the format
 *     "T\tindex\tindex\tindex\n", where the 3 index values correspond to the
 *     indices of the 3 vertices of that triangle. Since all vertices are
 *     output, these indices should match the order written by 1.
 *
 * @param d Delaunay tessellation (read-only).
 * @param file_name Name of the output file.
 */
inline static void delaunay_print_tessellation(
    const struct delaunay* restrict d, const char* file_name) {

  FILE* file = fopen(file_name, "w");

  for (int i = 0; i < d->vertex_index; ++i) {
    fprintf(file, "V\t%i\t%g\t%g\n", i, d->vertices[2 * i],
            d->vertices[2 * i + 1]);
  }
  for (int i = 0; i < d->triangle_index; ++i) {
    fprintf(file, "T\t%i\t%i\t%i\n", d->triangles[i].vertices[0],
            d->triangles[i].vertices[1], d->triangles[i].vertices[2]);
  }

  fclose(file);
}

#endif /* CVORONOI_DELAUNAY2D_H */
