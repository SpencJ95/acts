// This file is part of the Acts project.
//
// Copyright (C) 2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// CylinderSurface.ipp, Acts project
///////////////////////////////////////////////////////////////////

inline const Vector3D CylinderSurface::rotSymmetryAxis(
    const GeometryContext& gctx) const {
  // fast access via tranform matrix (and not rotation())
  return transform(gctx).matrix().block<3, 1>(0, 2);
}

inline detail::RealQuadraticEquation CylinderSurface::intersectionSolver(
    const GeometryContext& gctx, const Vector3D& position,
    const Vector3D& direction) const {
  // Solve for radius R
  double R = bounds().r();

  // Get the transformation matrtix
  const auto& tMatrix = transform(gctx).matrix();
  Vector3D caxis = tMatrix.block<3, 1>(0, 2).transpose();
  Vector3D ccenter = tMatrix.block<3, 1>(0, 3).transpose();

  // Check documentation for explanation
  Vector3D pc = position - ccenter;
  Vector3D pcXcd = pc.cross(caxis);
  Vector3D ldXcd = direction.cross(caxis);
  double a = ldXcd.dot(ldXcd);
  double b = 2. * (ldXcd.dot(pcXcd));
  double c = pcXcd.dot(pcXcd) - (R * R);
  // And solve the qaudratic equation
  return detail::RealQuadraticEquation(a, b, c);
}

inline Intersection CylinderSurface::intersectionEstimate(
    const GeometryContext& gctx, const Vector3D& position,
    const Vector3D& direction, const BoundaryCheck& bcheck) const {
  // Solve the quadratic euation
  auto qe = intersectionSolver(gctx, position, direction);

  // If no valid solution return a non-valid intersection
  if (qe.solutions == 0) {
    return Intersection();
  }

  // Absolute smallest solution
  double path =
      qe.first * qe.first < qe.second * qe.second ? qe.first : qe.second;
  Vector3D solution = position + path * direction;
  Intersection::Status status = Intersection::Status::reachable;

  // Boundary check necessary
  if (bcheck and not isOnSurface(gctx, solution, direction, bcheck)) {
    status = Intersection::Status::missed;
  }

  // Now return the solution
  return Intersection(solution, path, status);
}

inline SurfaceIntersection CylinderSurface::surfaceIntersectionEstimate(
    const GeometryContext& gctx, const Vector3D& position,
    const Vector3D& direction, const BoundaryCheck& bcheck) const {
  // Solve the quadratic euation
  auto qe = intersectionSolver(gctx, position, direction);

  // If no valid solution return a non-valid surfaceIntersection
  if (qe.solutions == 0) {
    return SurfaceIntersection();
  }

  // Check the validity of the first solution
  Vector3D solution1 = position + qe.first * direction;
  Intersection::Status status1 = Intersection::Status::reachable;
  if (bcheck and not isOnSurface(gctx, solution1, direction, bcheck)) {
    status1 = Intersection::Status::missed;
  }

  // Check the validity of second the solution
  Vector3D solution2 = position + qe.first * direction;
  Intersection::Status status2 = Intersection::Status::reachable;
  if (bcheck and not isOnSurface(gctx, solution2, direction, bcheck)) {
    status2 = Intersection::Status::missed;
  }
  // Set the intersection
  Intersection first(solution1, qe.first, status1);
  Intersection second(solution2, qe.second, status2);
  SurfaceIntersection cIntersection(first, this);
  // Check one if its valid or neither is valid
  bool check1 = status1 != Intersection::Status::missed or
                (status1 == Intersection::Status::missed and
                 status2 == Intersection::Status::missed);
  // Check and (eventually) go with the first solution
  if ((check1 and qe.first * qe.first < qe.second * qe.second) or
      status2 == Intersection::Status::missed) {
    // And add the alternative
    if (qe.solutions > 1) {
      cIntersection.alternatives = {second};
    }
  } else {
    // And add the alternative
    if (qe.solutions > 1) {
      cIntersection.alternatives = {first};
    }
    cIntersection.intersection = second;
  }
  return cIntersection;
}
