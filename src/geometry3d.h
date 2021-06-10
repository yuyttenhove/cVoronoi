//
// Created by yuyttenh on 09/06/2021.
//

#ifndef CVORONOI_GEOMETRY3D_H
#define CVORONOI_GEOMETRY3D_H

#include <gmp.h>

/**
 * @brief Auxiliary variables used by the arbirary exact tests. Since allocating
 * and deallocating these variables poses a significant overhead, they are best
 * reused.
 */
struct geometry {
  /*! @brief Arbitrary exact vertex coordinates */
  mpz_t aix, aiy, aiz, bix, biy, biz, cix, ciy, ciz, dix, diy, diz, eix, eiy,
      eiz;

  /*! @brief Temporary variables used to store relative vertex coordinates. */
  mpz_t s1x, s1y, s1z, s2x, s2y, s2z, s3x, s3y, s3z, s4x, s4y, s4z;

  /*! @brief Temporary variables used to store intermediate results. */
  mpz_t tmp1, tmp2, ab, bc, cd, da, ac, bd;

  /*! @brief Temporary variable used to store final exact results, before their
   *  sign is evaluated and returned as a finite precision integer. */
  mpz_t result;
};

/**
 * @brief Initialize the geometry object.
 *
 * This allocates and initialises the auxiliary arbitrary precision variables.
 *
 * @param g Geometry object.
 */
inline static void geometry_init(struct geometry* restrict g) {
  mpz_inits(g->aix, g->aiy, g->aiz, g->bix, g->biy, g->biz, g->cix, g->ciy,
            g->ciz, g->dix, g->diy, g->diz, g->eix, g->eiy, g->eiz, g->s1x,
            g->s1y, g->s1z, g->s2x, g->s2y, g->s2z, g->s3x, g->s3y, g->s3z,
            g->s4x, g->s4y, g->s4z, g->tmp1, g->tmp2, g->ab, g->bc, g->cd,
            g->da, g->ac, g->bd, g->result, NULL);
}

/**
 * @brief Deallocate all memory occupied by the geometry object.
 *
 * @param g Geometry object.
 */
inline static void geometry_destroy(struct geometry* restrict g) {
  mpz_clears(g->aix, g->aiy, g->aiz, g->bix, g->biy, g->biz, g->cix, g->ciy,
             g->ciz, g->dix, g->diy, g->diz, g->eix, g->eiy, g->eiz, g->s1x,
             g->s1y, g->s1z, g->s2x, g->s2y, g->s2z, g->s3x, g->s3y, g->s3z,
             g->s4x, g->s4y, g->s4z, g->tmp1, g->tmp2, g->ab, g->bc, g->cd,
             g->da, g->ac, g->bd, g->result, NULL);
}

inline static double geometry_orient() {
  // TODO
  return -1.;
}

/**
 * @brief Test the orientation of the tetrahedron that has the four given
 * points as vertices.
 *
 * The test returns a positive result if the fourth vertex is below the plane
 * through the three other vertices, with above the direction from which the
 * three points are ordered counterclockwise.
 *
 * E.g. if the four points are (0, 0, 0), (0, 0, 1), (0, 1, 0), and (1, 0, 0),
 * then this function returns 1.
 *
 * If the four points are exactly coplanar, then this function returns 0.
 *
 * @param a First vertex.
 * @param b Second vertex.
 * @param c Third vertex.
 * @param d Fourth vertex.
 * @return -1, 0, or 1, depending on the orientation of the tetrahedron.
 */
inline static int geometry_orient_exact(
    struct geometry* g, const unsigned long ax, const unsigned long ay,
    const unsigned long az, const unsigned long bx, const unsigned long by,
    const unsigned long bz, const unsigned long cx, const unsigned long cy,
    const unsigned long cz, const unsigned long dx, const unsigned long dy,
    const unsigned long dz) {

  /* store the input coordinates into the temporary large integer variables */
  mpz_set_ui(g->aix, ax);
  mpz_set_ui(g->aiy, ay);
  mpz_set_ui(g->aiz, az);

  mpz_set_ui(g->bix, bx);
  mpz_set_ui(g->biy, by);
  mpz_set_ui(g->biz, bz);

  mpz_set_ui(g->cix, cx);
  mpz_set_ui(g->ciy, cy);
  mpz_set_ui(g->ciz, cz);

  mpz_set_ui(g->dix, dx);
  mpz_set_ui(g->diy, dy);
  mpz_set_ui(g->diz, dz);

  /* compute large integer relative coordinates */
  mpz_sub(g->s1x, g->aix, g->dix);
  mpz_sub(g->s1y, g->aiy, g->diy);
  mpz_sub(g->s1z, g->aiz, g->diz);

  mpz_sub(g->s2x, g->bix, g->dix);
  mpz_sub(g->s2y, g->biy, g->diy);
  mpz_sub(g->s2z, g->biz, g->diz);

  mpz_sub(g->s3x, g->cix, g->dix);
  mpz_sub(g->s3y, g->ciy, g->diy);
  mpz_sub(g->s3z, g->ciz, g->diz);

  /* Compute the result in 3 steps */
  mpz_set_ui(g->result, 0);

  mpz_mul(g->tmp1, g->s2x, g->s3y);
  mpz_submul(g->tmp1, g->s3x, g->s2y);
  mpz_addmul(g->result, g->s1z, g->tmp1);

  mpz_mul(g->tmp1, g->s3x, g->s1y);
  mpz_submul(g->tmp1, g->s1x, g->s3y);
  mpz_addmul(g->result, g->s2z, g->tmp1);

  mpz_mul(g->tmp1, g->s1x, g->s2y);
  mpz_submul(g->tmp1, g->s2x, g->s1y);
  mpz_addmul(g->result, g->s3z, g->tmp1);

  return mpz_sgn(g->result);
}

inline static double geometry_in_sphere() {
  // TODO
  return -1.;
}

inline static int geometry_in_sphere_exact(
    struct geometry* restrict g, const unsigned long ax, const unsigned long ay,
    const unsigned long az, const unsigned long bx, const unsigned long by,
    const unsigned long bz, const unsigned long cx, const unsigned long cy,
    const unsigned long cz, const unsigned long dx, const unsigned long dy,
    const unsigned long dz, const unsigned long ex, const unsigned long ey,
    const unsigned long ez) {
  /* store the input coordinates into the temporary large integer variables */
  mpz_set_ui(g->aix, ax);
  mpz_set_ui(g->aiy, ay);
  mpz_set_ui(g->aiz, az);

  mpz_set_ui(g->bix, bx);
  mpz_set_ui(g->biy, by);
  mpz_set_ui(g->biz, bz);

  mpz_set_ui(g->cix, cx);
  mpz_set_ui(g->ciy, cy);
  mpz_set_ui(g->ciz, cz);

  mpz_set_ui(g->dix, dx);
  mpz_set_ui(g->diy, dy);
  mpz_set_ui(g->diz, dz);

  mpz_set_ui(g->eix, ex);
  mpz_set_ui(g->eiy, ey);
  mpz_set_ui(g->eiz, ez);

  /* compute large integer relative coordinates */
  mpz_sub(g->s1x, g->aix, g->eix);
  mpz_sub(g->s1y, g->aiy, g->eiy);
  mpz_sub(g->s1z, g->aiz, g->eiz);

  mpz_sub(g->s2x, g->bix, g->eix);
  mpz_sub(g->s2y, g->biy, g->eiy);
  mpz_sub(g->s2z, g->biz, g->eiz);

  mpz_sub(g->s3x, g->cix, g->eix);
  mpz_sub(g->s3y, g->ciy, g->eiy);
  mpz_sub(g->s3z, g->ciz, g->eiz);

  mpz_sub(g->s4x, g->dix, g->eix);
  mpz_sub(g->s4y, g->diy, g->eiy);
  mpz_sub(g->s4z, g->diz, g->eiz);

  /* compute intermediate values */
  mpz_mul(g->ab, g->s1x, g->s2y);
  mpz_submul(g->ab, g->s2x, g->s1y);

  mpz_mul(g->bc, g->s2x, g->s3y);
  mpz_submul(g->bc, g->s3x, g->s2y);

  mpz_mul(g->cd, g->s3x, g->s4y);
  mpz_submul(g->cd, g->s4x, g->s3y);

  mpz_mul(g->da, g->s4x, g->s1y);
  mpz_submul(g->da, g->s1x, g->s4y);

  mpz_mul(g->ac, g->s1x, g->s3y);
  mpz_submul(g->ac, g->s3x, g->s1y);

  mpz_mul(g->bd, g->s2x, g->s4y);
  mpz_submul(g->bd, g->s4x, g->s2y);


  /* compute the result in 4 steps */
  mpz_set_ui(g->result, 0);

  mpz_mul(g->tmp1, g->s4x, g->s4x);
  mpz_addmul(g->tmp1, g->s4y, g->s4y);
  mpz_addmul(g->tmp1, g->s4z, g->s4z);
  mpz_mul(g->tmp2, g->s1z, g->bc);
  mpz_submul(g->tmp2, g->s2z, g->ac);
  mpz_addmul(g->tmp2, g->s3z, g->ab);
  mpz_addmul(g->result, g->tmp1, g->tmp2);

  mpz_mul(g->tmp1, g->s3x, g->s3x);
  mpz_addmul(g->tmp1, g->s3y, g->s3y);
  mpz_addmul(g->tmp1, g->s3z, g->s3z);
  mpz_mul(g->tmp2, g->s4z, g->ab);
  mpz_addmul(g->tmp2, g->s1z, g->bd);
  mpz_addmul(g->tmp2, g->s2z, g->da);
  mpz_submul(g->result, g->tmp1, g->tmp2);

  mpz_mul(g->tmp1, g->s2x, g->s2x);
  mpz_addmul(g->tmp1, g->s2y, g->s2y);
  mpz_addmul(g->tmp1, g->s2z, g->s2z);
  mpz_mul(g->tmp2, g->s3z, g->da);
  mpz_addmul(g->tmp2, g->s4z, g->ac);
  mpz_addmul(g->tmp2, g->s1z, g->cd);
  mpz_addmul(g->result, g->tmp1, g->tmp2);

  mpz_mul(g->tmp1, g->s1x, g->s1x);
  mpz_addmul(g->tmp1, g->s1y, g->s1y);
  mpz_addmul(g->tmp1, g->s1z, g->s1z);
  mpz_mul(g->tmp2, g->s2z, g->cd);
  mpz_submul(g->tmp2, g->s3z, g->bd);
  mpz_addmul(g->tmp2, g->s4z, g->bc);
  mpz_submul(g->result, g->tmp1, g->tmp2);

  return mpz_sgn(g->result);
}

#endif  // CVORONOI_GEOMETRY3D_H
