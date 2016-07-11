// This file is part of the ACTS project.
//
// Copyright (C) 2016 ACTS project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// TriangleBounds.h, ACTS project
///////////////////////////////////////////////////////////////////

#ifndef ACTS_SURFACESTRIANGLEBOUNDS_H
#define ACTS_SURFACESTRIANGLEBOUNDS_H

#include <utility>
#include "ACTS/Surfaces/PlanarBounds.hpp"
#include "ACTS/Utilities/Definitions.hpp"
#include "ACTS/Utilities/ParameterDefinitions.hpp"

namespace Acts {

 /// @class TriangleBounds
 /// 
 /// Bounds for a triangular, planar surface.
 /// 
 /// @image html TriangularBounds.gif

class TriangleBounds : public PlanarBounds
{
public:
  // @enum BoundValues for readability 
  enum BoundValues {
    bv_x1     = 0,
    bv_y1     = 1,
    bv_x2     = 2,
    bv_y2     = 3,
    bv_x3     = 4,
    bv_y3     = 5,
    bv_length = 6
  };

  /// Default Constructor - deleted
  TriangleBounds() = delete;

  /// Constructor with coordinates of vertices
  /// @param vertices is the vector of vertices
  TriangleBounds(const std::vector<Vector2D>& vertices );

  /// Copy constructor
  /// @param tribo are the source bounds for assignment
  TriangleBounds(const TriangleBounds& tribo) : PlanarBounds(tribo) {}

  /// Destructor 
  virtual ~TriangleBounds();

  /// Assignment Operator
  /// @param tribo are the source bounds for assignment
  TriangleBounds&
  operator=(const TriangleBounds& tribo);

  /// Virtual constructor
  virtual TriangleBounds*
  clone() const override;

  /// Return the type of the bounds for persistency 
  virtual BoundsType
  type() const override
  {
    return SurfaceBounds::Triangle;
  }

  /// This method checks if the provided local coordinates are inside the surface bounds
  /// @param lpos local position in 2D local carthesian frame
  /// @param bchk is the boundary check directive
  virtual bool
  inside(const Vector2D& lpos, const BoundaryCheck& bchk) const override;

  /// This method checks if the provided local coordinate 1 is inside the surface bounds
  /// @param lpos local position in 2D local carthesian frame
  /// @param bchk is the boundary check directive
  virtual bool
  insideLoc0(const Vector2D& lpos, double tol0 = 0.) const override;

  /// This method checks if the provided local coordinate 2 is inside the surface bounds
  /// @param lpos local position in 2D local carthesian frame
  /// @param bchk is the boundary check directive
  virtual bool
  insideLoc1(const Vector2D& lpos, double tol1 = 0.) const override;

  /// Minimal distance to boundary 
  /// @param lpos is the local position in 2D local carthesian frame
  /// @return the distance ( > 0 if outside and <=0 if inside) 
  virtual double
  minDistance(const Vector2D& lpos) const override;

  /// This method returns the coordinates of vertices
  const std::vector<Vector2D>
  vertices() const override;

  /// Output Method for std::ostream 
  virtual std::ostream&
  dump(std::ostream& sl) const override;
  
private:
  /// private helper method for inside check
  bool inside(const Vector2D& lpos, double tol0, double tol2) const ; 

};

inline TriangleBounds*
TriangleBounds::clone() const
{
  return new TriangleBounds(*this);
}

inline bool
TriangleBounds::inside(const Vector2D& lpos, double tol0, double tol1) const
{
  std::pair<double, double> locB(m_valueStore.at(TriangleBounds::bv_x2)
                                     - m_valueStore.at(TriangleBounds::bv_x1),
                                 m_valueStore.at(TriangleBounds::bv_y2)
                                     - m_valueStore.at(TriangleBounds::bv_y1));
  std::pair<double, double> locT(
      m_valueStore.at(TriangleBounds::bv_x3) - lpos[0],
      m_valueStore.at(TriangleBounds::bv_y3) - lpos[1]);
  std::pair<double, double> locV(
      m_valueStore.at(TriangleBounds::bv_x1) - lpos[0],
      m_valueStore.at(TriangleBounds::bv_y1) - lpos[1]);

  // special case :: third vertex ?
  if (locT.first * locT.first + locT.second * locT.second < tol0 * tol0)
    return true;

  // special case : lies on base ?
  double db = locB.first * locV.second - locB.second * locV.first;
  if (fabs(db) < tol0) {
    double a = (locB.first != 0) ? -locV.first / locB.first
                                 : -locV.second / locB.second;
    if (a > -tol1 && a - 1. < tol1) return true;
    return false;
  }

  double dn = locB.first * locT.second - locB.second * locT.first;

  if (fabs(dn) > fabs(tol0)) {
    double t = (locB.first * locV.second - locB.second * locV.first) / dn;
    if (t > 0.) return false;

    double a = (locB.first != 0.)
        ? (t * locT.first - locV.first) / locB.first
        : (t * locT.second - locV.second) / locB.second;
    if (a < -tol1 || a - 1. > tol1) return false;
  } else {
    return false;
  }
  return true;
}

inline bool
TriangleBounds::inside(const Vector2D& lpos, const BoundaryCheck& bchk) const
{
  if (bchk.bcType == 0)
    return TriangleBounds::inside(
        lpos, bchk.toleranceLoc0, bchk.toleranceLoc1);

  /// @TODO check for quick limit test  
  ///double max_ell = bchk.lCovariance(0, 0) > bchk.lCovariance(1, 1)
  ///    ? bchk.lCovariance(0, 0)
  ///    : bchk.lCovariance(1, 1);
  /// a fast FALSE
  /// double fabsR = sqrt(lpos[Acts::eLOC_X] * lpos[Acts::eLOC_X]
  ///                    + lpos[Acts::eLOC_Y] * lpos[Acts::eLOC_Y]);
  /// double limit = bchk.nSigmas * sqrt(max_ell);
  ///double r_max = TriangleBounds::r();
  ///if (fabsR > (r_max + limit)) return false;
  // compute KDOP and axes for surface polygon
  std::vector<KDOP>     elementKDOP(3);
  std::vector<Vector2D> elementP(3);
  double                theta = (bchk.lCovariance(1, 0) != 0
                 && (bchk.lCovariance(1, 1) - bchk.lCovariance(0, 0)) != 0)
      ? .5
          * bchk.FastArcTan(2 * bchk.lCovariance(1, 0)
                            / (bchk.lCovariance(1, 1) - bchk.lCovariance(0, 0)))
      : 0.;
  sincosCache scResult = bchk.FastSinCos(theta);
  ActsMatrixD<2, 2> rotMatrix;
  rotMatrix << scResult.cosC, scResult.sinC, -scResult.sinC, scResult.cosC;
  ActsMatrixD<2, 2> normal;
  normal << 0, -1, 1, 0;
  // ellipse is always at (0,0), surface is moved to ellipse position and then
  // rotated
  Vector2D p;
  p << m_valueStore.at(TriangleBounds::bv_x1),
      m_valueStore.at(TriangleBounds::bv_y1);
  elementP.at(0) = (rotMatrix * (p - lpos));
  p << m_valueStore.at(TriangleBounds::bv_x2),
      m_valueStore.at(TriangleBounds::bv_y2);
  elementP.at(1) = (rotMatrix * (p - lpos));
  p << m_valueStore.at(TriangleBounds::bv_x3),
      m_valueStore.at(TriangleBounds::bv_y3);
  elementP.at(2)             = (rotMatrix * (p - lpos));
  std::vector<Vector2D> axis = {normal * (elementP.at(1) - elementP.at(0)),
                                normal * (elementP.at(2) - elementP.at(1)),
                                normal * (elementP.at(2) - elementP.at(0))};
  bchk.ComputeKDOP(elementP, axis, elementKDOP);
  // compute KDOP for error ellipse
  std::vector<KDOP> errelipseKDOP(3);
  bchk.ComputeKDOP(bchk.EllipseToPoly(3), axis, errelipseKDOP);
  // check if KDOPs overlap and return result
  return bchk.TestKDOPKDOP(elementKDOP, errelipseKDOP);
}

inline bool
TriangleBounds::insideLoc0(const Vector2D& lpos, double tol0) const
{
  return inside(lpos, tol0, tol0);
}

inline bool
TriangleBounds::insideLoc1(const Vector2D& lpos, double tol1) const
{
  return inside(lpos, tol1, tol1);
}

inline const std::vector<Vector2D>
TriangleBounds::vertices() const
{
  std::vector<Vector2D> vertices;
  vertices.resize(3);
  for (size_t iv = 0; iv < 3; iv++)
    vertices.push_back(
        Vector2D(m_valueStore.at(2 * iv), m_valueStore.at(2 * iv + 1)));
  return vertices;
}

}  // end of namespace

#endif  // ACTS_SURFACESRECTANGLEBOUNDS_H