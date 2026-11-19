/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/NatronGitHub/openfx-supportext>,
 * (C) 2018-2021 The Natron Developers
 * (C) 2013-2018 INRIA
 *
 * openfx-supportext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-supportext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-supportext.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * OFX Matrix utilities.
 */

#ifndef openfx_supportext_ofxsMatrix2D_h
#define openfx_supportext_ofxsMatrix2D_h

#include <cmath>
#include <cfloat>
#include <cassert>
#include <algorithm> // std::fill

#include "ofxsMacros.h" // OFXS_PI

namespace OFX {
// NEVER define a variable/constant in a header (said it 100 times already)
// an inline function is OK
inline double
ofxsPi()
{
    return OFXS_PI;
}

/**
 * @brief A simple 3 * 3 matrix class layed out as such:
 *  a b c
 *  d e f
 *  g h i
 **/

inline double
ofxsToDegrees(double rad)
{
    rad = rad * 180.0 / ofxsPi();

    return rad;
}

inline double
ofxsToRadians(double deg)
{
    deg = deg * ofxsPi() / 180.0;

    return deg;
}

struct Point3D
{
    double x, y, z;

    Point3D()
        : x(0), y(0), z(0)
    {
    }

    Point3D(double x,
            double y,
            double z)
        : x(x), y(y), z(z)
    {
    }

    Point3D(const Point3D & p)
        : x(p.x), y(p.y), z(p.z)
    {
    }

    Point3D& operator=(const Point3D & p)
    {
        x = p.x;
        y = p.y;
        z = p.z;
        return *this;
    }

    bool operator==(const Point3D & other)
    {
        return x == other.x && y == other.y && z == other.z;
    }
};


/**
 * \brief Compute the cross-product of two vectors
 *
 */
inline OFX::Point3D
crossprod(const OFX::Point3D & a,
          const OFX::Point3D & b)
{
    OFX::Point3D c;

    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - a.x * b.z;
    c.z = a.x * b.y - a.y * b.x;

    return c;
}

struct Point4D
{
    double x, y, z, w;

    Point4D()
        : x(0), y(0), z(0), w(0)
    {
    }

    Point4D(double x,
            double y,
            double z,
            double w)
        : x(x), y(y), z(z), w(w)
    {
    }

    Point4D(const Point4D & o)
        : x(o.x), y(o.y), z(o.z), w(o.w)
    {
    }

    double & operator() (int i)
    {
        switch (i) {
        case 0:

            return x;
        case 1:

            return y;
        case 2:

            return z;
        case 3:

            return w;
        default:
            assert(false);

            return x;
        }
    }

    double operator() (int i) const
    {
        switch (i) {
        case 0:

            return x;
        case 1:

            return y;
        case 2:

            return z;
        case 3:

            return w;
        default:
            assert(false);

            return x;
        }
    }

    bool operator==(const Point4D & o)
    {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
};

// A double with a separate int exponent (value = m * 2^e, m in [0.5, 1) or 0), whose products
// and sums neither overflow nor underflow: used for the inverse of matrices whose cofactors or
// determinant may overflow in double.
struct OfxsWideDouble
{
    double m;
    int e;
};

inline OfxsWideDouble
ofxsWideNormalize(double m,
                  int e)
{
    OfxsWideDouble r;
    int de = 0;

    r.m = std::frexp(m, &de);
    // the exponent is unspecified for inf and NaN: keep it bounded (no int overflow)
    de = (std::min)( (std::max)(de, -1100), 1100 );
    r.e = (r.m == 0.) ? 0 : (std::min)( (std::max)(e + de, -1000000), 1000000 );

    return r;
}

inline OfxsWideDouble
ofxsWideFromDouble(double v)
{
    return ofxsWideNormalize(v, 0);
}

inline OfxsWideDouble
ofxsWideMul(const OfxsWideDouble &a,
            const OfxsWideDouble &b)
{
    return ofxsWideNormalize(a.m * b.m, a.e + b.e);
}

inline OfxsWideDouble
ofxsWideAdd(const OfxsWideDouble &a,
            const OfxsWideDouble &b)
{
    if (a.m == 0.) {
        return b;
    }
    if (b.m == 0.) {
        return a;
    }
    const OfxsWideDouble &big = (a.e >= b.e) ? a : b;
    const OfxsWideDouble &small = (a.e >= b.e) ? b : a;
    const int d = big.e - small.e;
    if (d > 1100) {
        return big; // small is below half an ulp of big
    }

    return ofxsWideNormalize(big.m + std::ldexp(small.m, -d), big.e);
}

// determinant of the k x k submatrix of the N x N matrix a made of the rows rows[0..k-1] and
// the columns cols[0..k-1] (Laplace expansion along its first row)
template <int N>
OfxsWideDouble
ofxsWideDeterminant(const OfxsWideDouble* a,
                    const int* rows,
                    const int* cols,
                    int k)
{
    if (k == 1) {
        return a[rows[0] * N + cols[0]];
    }
    OfxsWideDouble det = ofxsWideFromDouble(0.);
    int subCols[N];
    for (int j = 0; j < k; ++j) {
        const OfxsWideDouble &x = a[rows[0] * N + cols[j]];
        if (x.m == 0.) {
            continue;
        }
        int n = 0;
        for (int q = 0; q < k; ++q) {
            if (q != j) {
                subCols[n++] = cols[q];
            }
        }
        OfxsWideDouble t = ofxsWideMul( x, ofxsWideDeterminant<N>(a, rows + 1, subCols, k - 1) );
        if (j % 2) {
            t.m = -t.m;
        }
        det = ofxsWideAdd(det, t);
    }

    return det;
}

// Inverse of an N x N matrix whose cofactors or determinant may overflow in double (inf * 0 =
// NaN entries; see ofxsMatrixProductsMayOverflow, only tested beyond 2^300 for 3x3, 2^250 for
// 4x4): the same cofactors / determinant, computed with OfxsWideDouble so that no intermediate
// value overflows or underflows; only the final entries are rounded to double (and may
// overflow if the true inverse is out of range). False if the determinant is 0. Matrices whose
// direct inverse cannot overflow never come here, so their inverse is unchanged.
template <int N, class MAT>
bool
ofxsInverseWideExponent(const MAT &mat,
                        MAT* invOut)
{
    OfxsWideDouble a[N * N];

    for (int i = 0; i < N * N; ++i) {
        a[i] = ofxsWideFromDouble(mat.m[i]);
    }
    int all[N];
    for (int i = 0; i < N; ++i) {
        all[i] = i;
    }
    const OfxsWideDouble det = ofxsWideDeterminant<N>(a, all, all, N);
    if (det.m == 0.) {
        return false; // singular
    }
    int rows[N], cols[N];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            // inv[i][j] = (-1)^(i+j) det(minor without row j and column i) / det
            int n = 0;
            for (int q = 0; q < N; ++q) {
                if (q != j) {
                    rows[n++] = q;
                }
            }
            n = 0;
            for (int q = 0; q < N; ++q) {
                if (q != i) {
                    cols[n++] = q;
                }
            }
            const OfxsWideDouble cof = ofxsWideDeterminant<N>(a, rows, cols, N - 1);
            double v = std::ldexp( cof.m / det.m, (std::min)( (std::max)(cof.e - det.e, -3000), 3000 ) );
            invOut->m[i * N + j] = ( (i + j) % 2 ) ? -v : v;
        }
    }

    return true;
}

// Can the direct inverse of the N x N matrix m overflow? Its cofactor and determinant terms
// (and their partial products, whatever the evaluation order) are products of entries in
// distinct rows and distinct columns: each is below 2^(sum of the entries' frexp exponents).
// True if such a sum exceeds 1018, so that a sum of up to 24 terms may not stay below
// DBL_MAX. Integer exponents only (clamped for inf and NaN, whose exponent is unspecified).
template <int N>
bool
ofxsMatrixProductsMayOverflow(const double* m)
{
    const int kZero = -100000; // a zero entry: its products are 0
    int ex[N * N];

    for (int i = 0; i < N * N; ++i) {
        ex[i] = kZero;
        if (m[i] != 0.) {
            int e = 0;
            std::frexp(m[i], &e);
            ex[i] = (std::min)( (std::max)(e, -1100), 1100 );
        }
    }
    // depth-first over the rows: each row contributes no entry, or one in an unused column
    int col[N + 1]; // col[r]: the column used by row r, -1 if none; col[N]: unused
    int sum[N + 1]; // sum[r]: the exponent sum of rows 0..r-1
    int count[N + 1]; // count[r]: the number of entries in rows 0..r-1
    int r = 0;
    col[0] = -2;
    sum[0] = 0;
    count[0] = 0;
    while (r >= 0) {
        // next choice for row r: -1 (no entry), then the columns 0..N-1
        int c = col[r] + 1;
        for (; c < N; ++c) {
            bool used = false;
            for (int q = 0; q < r; ++q) {
                if (col[q] == c) {
                    used = true;
                }
            }
            if ( (c == -1) || ( !used && (ex[r * N + c] != kZero) ) ) {
                break;
            }
        }
        if (c >= N) {
            --r; // no more choices for row r: backtrack
            continue;
        }
        col[r] = c;
        sum[r + 1] = sum[r] + ( (c >= 0) ? ex[r * N + c] : 0 );
        count[r + 1] = count[r] + ( (c >= 0) ? 1 : 0 );
        if ( (count[r + 1] >= 2) && (sum[r + 1] > 1018) ) {
            return true;
        }
        if (r + 1 < N) {
            ++r;
            col[r] = -2;
        }
    }

    return false;
}

// true if some entry of the N*N array m is above 2^e (NaN entries are ignored)
inline bool
ofxsMatrixHasEntryAbove(const double* m,
                        int n,
                        int e)
{
    const double big = std::ldexp(1., e);

    for (int i = 0; i < n; ++i) {
        if (std::fabs(m[i]) > big) {
            return true;
        }
    }

    return false;
}

struct Matrix3x3
{
    double m[3*3];

    Matrix3x3()
    {
        std::fill(m, m + 3*3, 0.);
    }

    Matrix3x3(double a,
              double b,
              double c,
              double d,
              double e,
              double f,
              double g,
              double h,
              double i)
    {
        m[0] = a; m[1] = b; m[2] = c;
        m[3] = d; m[4] = e; m[5] = f;
        m[6] = g; m[7] = h; m[8] = i;
    }

    Matrix3x3(const Matrix3x3 & other)
    {
        std::copy(other.m, other.m + 3 * 3, m);
    }

    Matrix3x3& operator=(const Matrix3x3& other)
    {
        if (this != &other) { // not a self-assignment
            std::copy(other.m, other.m + 3 * 3, m);
        }
        return *this;
    }

    /// Contruct from columns
    Matrix3x3(const OFX::Point3D &m0,
              const OFX::Point3D &m1,
              const OFX::Point3D &m2)
    {
        m[0] = m0.x; m[1] = m1.x; m[2] = m2.x;
        m[3] = m0.y; m[4] = m1.y; m[5] = m2.y;
        m[6] = m0.z; m[7] = m1.z; m[8] = m2.z;
    }

    bool isIdentity() const
    {
        return m[0] == 1 && m[1] == 0 && m[2] == 0 &&
               m[3] == 0 && m[4] == 1 && m[5] == 0 &&
               m[6] == 0 && m[7] == 0 && m[8] == 1;
    }

    bool operator==(const Matrix3x3 & o) const
    {
        return m[0] == o.m[0] && m[1] == o.m[1] && m[2] == o.m[2] &&
               m[3] == o.m[3] && m[4] == o.m[4] && m[5] == o.m[5] &&
               m[6] == o.m[6] && m[7] == o.m[7] && m[8] == o.m[8];
    }

    bool operator!=(const Matrix3x3 & o) const
    {
        return !(*this == o);
    }

    /// Set to the degenerate transform that maps every point to the origin
    /// (all coefficients are zero, except m(2,2) = 1).
    void setNullTransform()
    {
        std::fill(m, m + 3*3, 0.);
        m[8] = 1.;
    }

    double & operator()(int row,
                        int col)
    {
        assert(row >= 0 && row < 3 && col >= 0 && col < 3);

        return m[row * 3 + col];
    }

    double operator()(int row,
                      int col) const
    {
        assert(row >= 0 && row < 3 && col >= 0 && col < 3);

        return m[row * 3 + col];
    }

    Matrix3x3 operator*(const Matrix3x3 & m2) const
    {
        return Matrix3x3(m[0] * m2.m[0] + m[1] * m2.m[3] + m[2] * m2.m[6],
                         m[0] * m2.m[1] + m[1] * m2.m[4] + m[2] * m2.m[7],
                         m[0] * m2.m[2] + m[1] * m2.m[5] + m[2] * m2.m[8],
                         m[3] * m2.m[0] + m[4] * m2.m[3] + m[5] * m2.m[6],
                         m[3] * m2.m[1] + m[4] * m2.m[4] + m[5] * m2.m[7],
                         m[3] * m2.m[2] + m[4] * m2.m[5] + m[5] * m2.m[8],
                         m[6] * m2.m[0] + m[7] * m2.m[3] + m[8] * m2.m[6],
                         m[6] * m2.m[1] + m[7] * m2.m[4] + m[8] * m2.m[7],
                         m[6] * m2.m[2] + m[7] * m2.m[5] + m[8] * m2.m[8]);
    }

    Point3D operator*(const Point3D & p) const
    {
        Point3D ret;

        ret.x = m[0] * p.x + m[1] * p.y + m[2] * p.z;
        ret.y = m[3] * p.x + m[4] * p.y + m[5] * p.z;
        ret.z = m[6] * p.x + m[7] * p.y + m[8] * p.z;

        return ret;
    }

    Matrix3x3 operator *(double s)
    {
        return Matrix3x3(m[0] * s,
                         m[1] * s,
                         m[2] * s,
                         m[3] * s,
                         m[4] * s,
                         m[5] * s,
                         m[6] * s,
                         m[7] * s,
                         m[8] * s);
    }

    Matrix3x3& operator *=(double s)
    {
        for (int i=0; i < 9; ++i) {
            m[i] *= s;
        }
        return *this;
    }


    double determinant() const
    {
        return m[0] * (m[4] * m[8] - m[7] * m[5])
               - m[1] * (m[3] * m[8] - m[6] * m[5])
               + m[2] * (m[3] * m[7] - m[6] * m[4]);
    }

    Matrix3x3 scaledAdjoint(double s) const
    {
        Matrix3x3 ret;

        ret.m[0] = (s) * (m[4] * m[8] - m[7] * m[5]);
        ret.m[3] = (s) * (m[5] * m[6] - m[3] * m[8]);
        ret.m[6] = (s) * (m[3] * m[7] - m[4] * m[6]);

        ret.m[1] = (s) * (m[2] * m[7] - m[1] * m[8]);
        ret.m[4] = (s) * (m[0] * m[8] - m[2] * m[6]);
        ret.m[7] = (s) * (m[1] * m[6] - m[0] * m[7]);

        ret.m[2] = (s) * (m[1] * m[5] - m[2] * m[4]);
        ret.m[5] = (s) * (m[2] * m[3] - m[0] * m[5]);
        ret.m[8] = (s) * (m[0] * m[4] - m[1] * m[3]);

        return ret;
    }

    bool inverse(Matrix3x3* invOut) const
    {
        // entries beyond 2^300: the cofactors or the determinant may overflow
        // (the product test is only run for large entries, to keep the common case fast)
        if ( ofxsMatrixHasEntryAbove(m, 9, 300) && ofxsMatrixProductsMayOverflow<3>(m) ) {
            return ofxsInverseWideExponent<3>(*this, invOut);
        }

        return inverseUnscaled(invOut);
    }

    /// the inverse, computed directly (see inverse())
    bool inverseUnscaled(Matrix3x3* invOut) const
    {
        double inv[9], det;
        int i;

        inv[0] = (m[4] * m[8] - m[7] * m[5]);
        inv[3] = (m[5] * m[6] - m[3] * m[8]);
        inv[6] = (m[3] * m[7] - m[4] * m[6]);

        det = m[0] * inv[0] + m[1] * inv[3] + m[2] * inv[6];

        if (std::fabs(det) < DBL_MIN) {
            // singular, or subnormal: 1/det would be huge or overflow to inf (then 0*inf = NaN below)
            return false;
        }

        inv[1] = (m[2] * m[7] - m[1] * m[8]);
        inv[4] = (m[0] * m[8] - m[2] * m[6]);
        inv[7] = (m[1] * m[6] - m[0] * m[7]);

        inv[2] = (m[1] * m[5] - m[2] * m[4]);
        inv[5] = (m[2] * m[3] - m[0] * m[5]);
        inv[8] = (m[0] * m[4] - m[1] * m[3]);

        det = 1.0 / det;

        for (i = 0; i < 9; ++i) {
            invOut->m[i] = inv[i] * det;
        }
        return true;

        /* old version
        double det = determinant();
        if (det == 0) {
            return false;
        }
        *invOut = scaledAdjoint( 1. / det );
        return true;
         */
    }

    void setIdentity()
    {
        m[0] = 1; m[1] = 0; m[2] = 0;
        m[3] = 0; m[4] = 1; m[5] = 0;
        m[6] = 0; m[7] = 0; m[8] = 1;
    }

    /**
     * \brief Compute a homography from 4 points correspondences
     * \param p1 source point
     * \param p2 source point
     * \param p3 source point
     * \param p4 source point
     * \param q1 target point
     * \param q2 target point
     * \param q3 target point
     * \param q4 target point
     * \return the homography matrix that maps pi's to qi's
     *
       Using four point-correspondences pi ↔ pi^, we can set up an equation system to solve for the homography matrix H.
       An algorithm to obtain these parameters requiring only the inversion of a 3 × 3 equation system is as follows.
       From the four point-correspondences pi ↔ pi^ with (i ∈ {1, 2, 3, 4}),
       compute h1 = (p1 × p2 ) × (p3 × p4 ), h2 = (p1 × p3 ) × (p2 × p4 ), h3 = (p1 × p4 ) × (p2 × p3 ).
       Also compute h1^ , h2^ , h3^ using the same principle from the points pi^.
       Now, the homography matrix H can be obtained easily from
       H · [h1 h2 h3] = [h1^ h2^ h3^],
       which only requires the inversion of the matrix [h1 h2 h3].

       Algo from:
       http://www.dirk-farin.net/publications/phd/text/AB_EfficientComputationOfHomographiesFromFourCorrespondences.pdf
     */
    bool setHomographyFromFourPoints(const OFX::Point3D &p1,
                                     const OFX::Point3D &p2,
                                     const OFX::Point3D &p3,
                                     const OFX::Point3D &p4,
                                     const OFX::Point3D &q1,
                                     const OFX::Point3D &q2,
                                     const OFX::Point3D &q3,
                                     const OFX::Point3D &q4)
    {
        OFX::Matrix3x3 invHp;
        OFX::Matrix3x3 Hp( crossprod( crossprod(p1, p2), crossprod(p3, p4) ),
                           crossprod( crossprod(p1, p3), crossprod(p2, p4) ),
                           crossprod( crossprod(p1, p4), crossprod(p2, p3) ) );

        if ( !Hp.inverse(&invHp) ) {
            return false;
        }
        OFX::Matrix3x3 Hq( crossprod( crossprod(q1, q2), crossprod(q3, q4) ),
                           crossprod( crossprod(q1, q3), crossprod(q2, q4) ),
                           crossprod( crossprod(q1, q4), crossprod(q2, q3) ) );
        double detHq = Hq.determinant();
        if (detHq == 0.) {
            return false;
        }
        *this = Hq * invHp;

        return true;
    }

    bool setAffineFromThreePoints(const OFX::Point3D &p1,
                                  const OFX::Point3D &p2,
                                  const OFX::Point3D &p3,
                                  const OFX::Point3D &q1,
                                  const OFX::Point3D &q2,
                                  const OFX::Point3D &q3)
    {
        OFX::Matrix3x3 invHp;
        OFX::Matrix3x3 Hp(p1, p2, p3);
        if ( !Hp.inverse(&invHp) ) {
            return false;
        }
        OFX::Matrix3x3 Hq(q1, q2, q3);
        double detHq = Hq.determinant();
        if (detHq == 0.) {
            return false;
        }
        *this = Hq * invHp;

        return true;
    }

    bool setSimilarityFromTwoPoints(const OFX::Point3D &p1,
                                    const OFX::Point3D &p2,
                                    const OFX::Point3D &q1,
                                    const OFX::Point3D &q2)
    {
        // Generate a third point so that p1p3 is orthogonal to p1p2, and compute the affine transform
        OFX::Point3D p3, q3;

        p3.x = p1.x - (p2.y - p1.y);
        p3.y = p1.y + (p2.x - p1.x);
        p3.z = 1.;
        q3.x = q1.x - (q2.y - q1.y);
        q3.y = q1.y + (q2.x - q1.x);
        q3.z = 1.;

        return setAffineFromThreePoints(p1, p2, p3, q1, q2, q3);
        /*
           there is probably a better solution.
           we have to solve for H in
           [x1 x2]
           [ h1 -h2 h3] [y1 y2]   [x1' x2']
           [ h2  h1 h4] [ 1  1] = [y1' y2']

           which is equivalent to
           [x1 -y1 1 0] [h1]   [x1']
           [x2 -y2 1 0] [h2]   [x2']
           [y1  x1 0 1] [h3] = [y1']
           [y2  x2 0 1] [h4]   [y2']
           The 4x4 matrix should be easily invertible

           with(linalg);
           M := Matrix([[x1, -y1, 1, 0], [x2, -y2, 1, 0], [y1, x1, 0, 1], [y2, x2, 0, 1]]);
           inverse(M);
         */
        /*
           double det = p1.x*p1.x - 2*p2.x*p1.x + p2.x*p2.x +p1.y*p1.y -2*p1.y*p2.y +p2.y*p2.y;
           if (det == 0.) {
           return false;
           }
           double h1 = (p1.x-p2.x)*(q1.x-q2.x) + (p1.y-p2.y)*(q1.y-q2.y);
           double h2 = (p1.x-p2.x)*(q1.y-q2.y) - (p1.y-p2.y)*(q1.x-q2.x);
           double h3 =
           todo...
         */
    }

    bool setTranslationFromOnePoint(const OFX::Point3D &p1,
                                    const OFX::Point3D &q1)
    {
        m[0] = 1.;
        m[1] = 0.;
        m[2] = q1.x - p1.x;
        m[3] = 0.;
        m[4] = 1.;
        m[5] = q1.y - p1.y;
        m[6] = 0.;
        m[7] = 0.;
        m[8] = 1.;

        return true;
    }
};

inline Matrix3x3 ofxsMatRotation(double rads);
inline Matrix3x3 ofxsMatRotationAroundPoint(double rads, double pointX, double pointY);
inline Matrix3x3 ofxsMatTranslation(double translateX, double translateY);
inline Matrix3x3 ofxsMatScale(double scaleX, double scaleY);
inline Matrix3x3 ofxsMatScale(double scale);
inline Matrix3x3 ofxsMatScaleAroundPoint(double scaleX, double scaleY, double pointX, double pointY);
inline Matrix3x3 ofxsMatSkewXY(double skewX, double skewY, bool skewOrderYX);

// matrix transform from destination to source, in canonical coordinates
inline Matrix3x3 ofxsMatInverseTransformCanonical(double translateX, double translateY, double scaleX, double scaleY, double skewX, double skewY, bool skewOrderYX, double rads, double centerX, double centerY);

// matrix transform from source to destination in canonical coordinates
inline Matrix3x3 ofxsMatTransformCanonical(double translateX, double translateY, double scaleX, double scaleY, double skewX, double skewY, bool skewOrderYX, double rads, double centerX, double centerY);

/// transform from pixel coordinates to canonical coordinates
inline Matrix3x3 ofxsMatPixelToCanonical(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                                         double renderscaleX, //!< 0.5 for a half-resolution image
                                         double renderscaleY,
                                         bool fielded); //!< true if the image property kOfxImagePropField is kOfxImageFieldLower or kOfxImageFieldUpper (apply 0.5 field scale in Y
/// transform from canonical coordinates to pixel coordinates
inline Matrix3x3 ofxsMatCanonicalToPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                                         double renderscaleX, //!< 0.5 for a half-resolution image
                                         double renderscaleY,
                                         bool fielded); //!< true if the image property kOfxImagePropField is kOfxImageFieldLower or kOfxImageFieldUpper (apply 0.5 field scale in Y

// matrix transform from destination to source, in pixel coordinates
inline Matrix3x3 ofxsMatInverseTransformPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                                              double renderscaleX, //!< 0.5 for a half-resolution image
                                              double renderscaleY,
                                              bool fielded,
                                              double translateX, double translateY,
                                              double scaleX, double scaleY,
                                              double skewX,
                                              double skewY,
                                              bool skewOrderYX,
                                              double rads,
                                              double centerX, double centerY);

// matrix transform from source to destination in pixel coordinates
inline Matrix3x3 ofxsMatTransformPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                                       double renderscaleX, //!< 0.5 for a half-resolution image
                                       double renderscaleY,
                                       bool fielded,
                                       double translateX, double translateY,
                                       double scaleX, double scaleY,
                                       double skewX,
                                       double skewY,
                                       bool skewOrderYX,
                                       double rads,
                                       double centerX, double centerY);
struct Matrix4x4
{
    double m[16];

    Matrix4x4()
    {
        std::fill(m, m + 16, 0.);
    }

    Matrix4x4(const double d[16])
    {
        std::copy(d, d + 16, m);
    }

    Matrix4x4(const Matrix4x4 & other)
    {
        std::copy(other.m, other.m + 16, m);
    }

    Matrix4x4& operator=(const Matrix4x4& other)
    {
        if (this != &other) { // not a self-assignment
            std::copy(other.m, other.m + 16, m);
        }
        return *this;
    }

    double & operator()(int row,
                        int col)
    {
        assert(row >= 0 && row < 4 && col >= 0 && col < 4);

        return m[row * 4 + col];
    }

    double operator()(int row,
                      int col) const
    {
        assert(row >= 0 && row < 4 && col >= 0 && col < 4);

        return m[row * 4 + col];
    }

    double determinant() const
    {
        double inv0 = (m[5]  * m[10] * m[15] -
                       m[5]  * m[11] * m[14] -
                       m[9]  * m[6]  * m[15] +
                       m[9]  * m[7]  * m[14] +
                       m[13] * m[6]  * m[11] -
                       m[13] * m[7]  * m[10]);

        double inv4 = (-m[4]  * m[10] * m[15] +
                       m[4]  * m[11] * m[14] +
                       m[8]  * m[6]  * m[15] -
                       m[8]  * m[7]  * m[14] -
                       m[12] * m[6]  * m[11] +
                       m[12] * m[7]  * m[10]);

        double inv8 = (m[4]  * m[9] * m[15] -
                       m[4]  * m[11] * m[13] -
                       m[8]  * m[5] * m[15] +
                       m[8]  * m[7] * m[13] +
                       m[12] * m[5] * m[11] -
                       m[12] * m[7] * m[9]);

        double inv12 = (-m[4]  * m[9] * m[14] +
                        m[4]  * m[10] * m[13] +
                        m[8]  * m[5] * m[14] -
                        m[8]  * m[6] * m[13] -
                        m[12] * m[5] * m[10] +
                        m[12] * m[6] * m[9]);

        return m[0] * inv0 + m[1] * inv4 + m[2] * inv8 + m[3] * inv12;
    }

    bool inverse(Matrix4x4* invOut) const
    {
        // entries beyond 2^250: the cofactors or the determinant may overflow
        // (the product test is only run for large entries, to keep the common case fast)
        if ( ofxsMatrixHasEntryAbove(m, 16, 250) && ofxsMatrixProductsMayOverflow<4>(m) ) {
            return ofxsInverseWideExponent<4>(*this, invOut);
        }

        return inverseUnscaled(invOut);
    }

    /// the inverse, computed directly (see inverse())
    bool inverseUnscaled(Matrix4x4* invOut) const
    {
        double inv[16], det;
        int i;

        inv[0] = (m[5]  * m[10] * m[15] -
                  m[5]  * m[11] * m[14] -
                  m[9]  * m[6]  * m[15] +
                  m[9]  * m[7]  * m[14] +
                  m[13] * m[6]  * m[11] -
                  m[13] * m[7]  * m[10]);

        inv[4] = (-m[4]  * m[10] * m[15] +
                  m[4]  * m[11] * m[14] +
                  m[8]  * m[6]  * m[15] -
                  m[8]  * m[7]  * m[14] -
                  m[12] * m[6]  * m[11] +
                  m[12] * m[7]  * m[10]);

        inv[8] = (m[4]  * m[9] * m[15] -
                  m[4]  * m[11] * m[13] -
                  m[8]  * m[5] * m[15] +
                  m[8]  * m[7] * m[13] +
                  m[12] * m[5] * m[11] -
                  m[12] * m[7] * m[9]);

        inv[12] = (-m[4]  * m[9] * m[14] +
                   m[4]  * m[10] * m[13] +
                   m[8]  * m[5] * m[14] -
                   m[8]  * m[6] * m[13] -
                   m[12] * m[5] * m[10] +
                   m[12] * m[6] * m[9]);

        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (std::fabs(det) < DBL_MIN) {
            // singular, or subnormal: 1/det would be huge or overflow to inf (then 0*inf = NaN below)
            return false;
        }

        inv[1] = (-m[1]  * m[10] * m[15] +
                  m[1]  * m[11] * m[14] +
                  m[9]  * m[2] * m[15] -
                  m[9]  * m[3] * m[14] -
                  m[13] * m[2] * m[11] +
                  m[13] * m[3] * m[10]);

        inv[5] = (m[0]  * m[10] * m[15] -
                  m[0]  * m[11] * m[14] -
                  m[8]  * m[2] * m[15] +
                  m[8]  * m[3] * m[14] +
                  m[12] * m[2] * m[11] -
                  m[12] * m[3] * m[10]);

        inv[9] = (-m[0]  * m[9] * m[15] +
                  m[0]  * m[11] * m[13] +
                  m[8]  * m[1] * m[15] -
                  m[8]  * m[3] * m[13] -
                  m[12] * m[1] * m[11] +
                  m[12] * m[3] * m[9]);

        inv[13] = (m[0]  * m[9] * m[14] -
                   m[0]  * m[10] * m[13] -
                   m[8]  * m[1] * m[14] +
                   m[8]  * m[2] * m[13] +
                   m[12] * m[1] * m[10] -
                   m[12] * m[2] * m[9]);

        inv[2] = (m[1]  * m[6] * m[15] -
                  m[1]  * m[7] * m[14] -
                  m[5]  * m[2] * m[15] +
                  m[5]  * m[3] * m[14] +
                  m[13] * m[2] * m[7] -
                  m[13] * m[3] * m[6]);

        inv[6] = (-m[0]  * m[6] * m[15] +
                  m[0]  * m[7] * m[14] +
                  m[4]  * m[2] * m[15] -
                  m[4]  * m[3] * m[14] -
                  m[12] * m[2] * m[7] +
                  m[12] * m[3] * m[6]);

        inv[10] = (m[0]  * m[5] * m[15] -
                   m[0]  * m[7] * m[13] -
                   m[4]  * m[1] * m[15] +
                   m[4]  * m[3] * m[13] +
                   m[12] * m[1] * m[7] -
                   m[12] * m[3] * m[5]);

        inv[14] = (-m[0]  * m[5] * m[14] +
                   m[0]  * m[6] * m[13] +
                   m[4]  * m[1] * m[14] -
                   m[4]  * m[2] * m[13] -
                   m[12] * m[1] * m[6] +
                   m[12] * m[2] * m[5]);

        inv[3] = (-m[1] * m[6] * m[11] +
                  m[1] * m[7] * m[10] +
                  m[5] * m[2] * m[11] -
                  m[5] * m[3] * m[10] -
                  m[9] * m[2] * m[7] +
                  m[9] * m[3] * m[6]);

        inv[7] = (m[0] * m[6] * m[11] -
                  m[0] * m[7] * m[10] -
                  m[4] * m[2] * m[11] +
                  m[4] * m[3] * m[10] +
                  m[8] * m[2] * m[7] -
                  m[8] * m[3] * m[6]);

        inv[11] = (-m[0] * m[5] * m[11] +
                   m[0] * m[7] * m[9] +
                   m[4] * m[1] * m[11] -
                   m[4] * m[3] * m[9] -
                   m[8] * m[1] * m[7] +
                   m[8] * m[3] * m[5]);

        inv[15] = (m[0] * m[5] * m[10] -
                   m[0] * m[6] * m[9] -
                   m[4] * m[1] * m[10] +
                   m[4] * m[2] * m[9] +
                   m[8] * m[1] * m[6] -
                   m[8] * m[2] * m[5]);

        det = 1.0 / det;

        for (i = 0; i < 16; i++) {
            invOut->m[i] = inv[i] * det;
        }

        return true;
    }
};

inline Matrix4x4
operator*(const Matrix4x4 & m1,
          const Matrix4x4 & m2)
{
    Matrix4x4 ret;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int x = 0; x < 4; ++x) {
                ret(i, j) += m1(i, x) * m2(x, j);
            }
        }
    }

    return ret;
}

inline Point4D
operator*(const Matrix4x4 & m,
          const Point4D & p)
{
    Point4D ret;

    for (int i = 0; i < 4; ++i) {
        ret(i) = 0.;
        for (int j = 0; j < 4; ++j) {
            ret(i) += m(i, j) * p(j);
        }
    }

    return ret;
}

inline Matrix4x4
matrix4x4FromMatrix3x3(const Matrix3x3 & m)
{
    Matrix4x4 ret;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            ret(i,j) = m(i,j);
        }
    }
    ret(0, 3) = 0.;
    ret(1, 3) = 0.;
    ret(2, 3) = 0.;
    ret(3, 0) = 0.;  ret(3, 1) = 0.;  ret(3, 2) = 0.;  ret(3, 3) = 1.;

    return ret;
}

////////////////////
// IMPLEMENTATION //
////////////////////


inline Matrix3x3
ofxsMatRotation(double rads)
{
    double c = std::cos(rads);
    double s = std::sin(rads);

    return Matrix3x3(c, s, 0, -s, c, 0, 0, 0, 1);
}

inline Matrix3x3
ofxsMatRotationAroundPoint(double rads,
                           double px,
                           double py)
{
    return ofxsMatTranslation(px, py) * ( ofxsMatRotation(rads) * ofxsMatTranslation(-px, -py) );
}

inline Matrix3x3
ofxsMatTranslation(double x,
                   double y)
{
    return Matrix3x3(1., 0., x,
                     0., 1., y,
                     0., 0., 1.);
}

inline Matrix3x3
ofxsMatScale(double x,
             double y)
{
    return Matrix3x3(x,  0., 0.,
                     0., y,  0.,
                     0., 0., 1.);
}

inline Matrix3x3
ofxsMatScale(double s)
{
    return ofxsMatScale(s, s);
}

inline Matrix3x3
ofxsMatScaleAroundPoint(double scaleX,
                        double scaleY,
                        double px,
                        double py)
{
    return ofxsMatTranslation(px, py) * ( ofxsMatScale(scaleX, scaleY) * ofxsMatTranslation(-px, -py) );
}

inline Matrix3x3
ofxsMatSkewXY(double skewX,
              double skewY,
              bool skewOrderYX)
{
    return Matrix3x3(skewOrderYX ? 1. : (1. + skewX * skewY), skewX, 0.,
                     skewY, skewOrderYX ? (1. + skewX * skewY) : 1, 0.,
                     0., 0., 1.);
}

// matrix transform from destination to source
inline Matrix3x3
ofxsMatInverseTransformCanonical(double translateX,
                                 double translateY,
                                 double scaleX,
                                 double scaleY,
                                 double skewX,
                                 double skewY,
                                 bool skewOrderYX,
                                 double rads,
                                 double centerX,
                                 double centerY)
{
    ///1) We translate to the center of the transform.
    ///2) We scale
    ///3) We apply skewX and skewY in the right order
    ///4) We rotate
    ///5) We apply the global translation
    ///5) We translate back to the origin

    // avoid division by zero (use FLT_MIN instead of DBL_MIN in case there are further casts to float)
    if (std::fabs(scaleX) < FLT_MIN ) {
      scaleX = scaleX > 0 ? FLT_MIN : -FLT_MIN;
    }
    if (std::fabs(scaleY) < FLT_MIN ) {
      scaleY = scaleY > 0 ? FLT_MIN : -FLT_MIN;
    }
    // since this is the inverse, oerations are in reverse order
    return ofxsMatTranslation(centerX, centerY) *
           ofxsMatScale(1. / scaleX, 1. / scaleY) *
           ofxsMatSkewXY(-skewX, -skewY, !skewOrderYX) *
           ofxsMatRotation(rads) *
           ofxsMatTranslation(-translateX, -translateY) *
           ofxsMatTranslation(-centerX, -centerY);
}

// matrix transform from source to destination
inline Matrix3x3
ofxsMatTransformCanonical(double translateX,
                          double translateY,
                          double scaleX,
                          double scaleY,
                          double skewX,
                          double skewY,
                          bool skewOrderYX,
                          double rads,
                          double centerX,
                          double centerY)
{
    ///1) We translate to the center of the transform.
    ///2) We scale
    ///3) We apply skewX and skewY in the right order
    ///4) We rotate
    ///5) We apply the global translation
    ///5) We translate back to the origin

    return ofxsMatTranslation(centerX, centerY) *
           ofxsMatTranslation(translateX, translateY) *
           ofxsMatRotation(-rads) *
           ofxsMatSkewXY(skewX, skewY, skewOrderYX) *
           ofxsMatScale(scaleX, scaleY) *
           ofxsMatTranslation(-centerX, -centerY);
}

// The transforms between pixel and canonical coordinated
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#MappingCoordinates

/// transform from pixel coordinates to canonical coordinates
inline Matrix3x3
ofxsMatPixelToCanonical(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                        double renderscaleX, //!< 0.5 for a half-resolution image
                        double renderscaleY,
                        bool fielded) //!< true if the image property kOfxImagePropField is kOfxImageFieldLower or kOfxImageFieldUpper (apply 0.5 field scale in Y
{
    /*
       To map an X and Y coordinates from Pixel coordinates to Canonical coordinates, we perform the following multiplications...

       X' = (X * PAR)/SX
       Y' = Y/(SY * FS)
     */

    // FIXME: when it's the Upper field, showuldn't the first pixel start at canonical coordinate (0,0.5) ?
    return ofxsMatScale( pixelaspectratio / renderscaleX, 1. / ( renderscaleY * (fielded ? 0.5 : 1.0) ) );
}

/// transform from canonical coordinates to pixel coordinates
inline Matrix3x3
ofxsMatCanonicalToPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                        double renderscaleX, //!< 0.5 for a half-resolution image
                        double renderscaleY,
                        bool fielded) //!< true if the image property kOfxImagePropField is kOfxImageFieldLower or kOfxImageFieldUpper (apply 0.5 field scale in Y
{
    /*
       To map an X and Y coordinates from Canonical coordinates to Pixel coordinates, we perform the following multiplications...

       X' = (X * SX)/PAR
       Y' = Y * SY * FS
     */

    // FIXME: when it's the Upper field, showuldn't the first pixel start at canonical coordinate (0,0.5) ?
    return ofxsMatScale( renderscaleX / pixelaspectratio, renderscaleY * (fielded ? 0.5 : 1.0) );
}

// matrix transform from destination to source
inline Matrix3x3
ofxsMatInverseTransformPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                             double renderscaleX, //!< 0.5 for a half-resolution image
                             double renderscaleY,
                             bool fielded,
                             double translateX,
                             double translateY,
                             double scaleX,
                             double scaleY,
                             double skewX,
                             double skewY,
                             bool skewOrderYX,
                             double rads,
                             double centerX,
                             double centerY)
{
    ///1) We go from pixel to canonical
    ///2) we apply transform
    ///3) We go back to pixels

    return ofxsMatCanonicalToPixel(pixelaspectratio, renderscaleX, renderscaleY, fielded) *
           ofxsMatInverseTransformCanonical(translateX, translateY, scaleX, scaleY, skewX, skewY, skewOrderYX, rads, centerX, centerY) *
           ofxsMatPixelToCanonical(pixelaspectratio, renderscaleX, renderscaleY, fielded);
}

// matrix transform from source to destination
inline Matrix3x3
ofxsMatTransformPixel(double pixelaspectratio, //!< 1.067 for PAL, where 720x576 pixels occupy 768x576 in canonical coords
                      double renderscaleX, //!< 0.5 for a half-resolution image
                      double renderscaleY,
                      bool fielded,
                      double translateX,
                      double translateY,
                      double scaleX,
                      double scaleY,
                      double skewX,
                      double skewY,
                      bool skewOrderYX,
                      double rads,
                      double centerX,
                      double centerY)
{
    ///1) We go from pixel to canonical
    ///2) we apply transform
    ///3) We go back to pixels

    return ofxsMatCanonicalToPixel(pixelaspectratio, renderscaleX, renderscaleY, fielded) *
           ofxsMatTransformCanonical(translateX, translateY, scaleX, scaleY, skewX, skewY, skewOrderYX, rads, centerX, centerY) *
           ofxsMatPixelToCanonical(pixelaspectratio, renderscaleX, renderscaleY, fielded);
}
};

#endif // openfx_supportext_ofxsMatrix2D_h
