//////////////////////////////// -*- C++ -*- //////////////////////////////
//
// AUTHOR
//    Peter Shirley, Cornell University, shirley@graphics.cornell.edu 
//
// COPYRIGHT
//    Copyright (c) 1995  Peter Shirley. All rights reserved.
//
//    Permission to use, copy, modify, and distribute this software for any
//    purpose without fee is hereby granted, provided that this entire
//    notice is included in all copies of any software which is or includes
//    a copy or modification of this software and in all copies of the
//    a copy or modification of this software and in all copies of the
//    supporting documentation for such software.
//
// DISCLAIMER
//    Neither the Peter Shirley nor Cornell nor any of their
//    employees, makes any warranty, express or implied, or assumes any
//    liability or responsibility for the accuracy, completeness, or
//    usefulness of any information, apparatus, product, or process
//    disclosed, or represents that its use would not infringe
//    privately-owned rights.  
//
///////////////////////////////////////////////////////////////////////////



#ifndef MRXCYLINDER_H
#define MRXCYLINDER_H


#include <mrSurface.h>
#include <ggGeometry.h>

// an axis-aligned cylinder parallel to the X axis

class mrXCylinder : public mrSurface {
public:
  virtual ggBoolean print(ostream& ) const;

  virtual ggBoolean shadowHit(        
             const ggRay3& r,    // ray being sent
             double time,     
             double tmin, // minimum hit parameter to be searched for
             double tmax, // maximum hit parameter to be searched for
             double& t,
             ggVector3& N,  
             ggBoolean& emits,
             ggSpectrum& kEmit
             ) const;

  virtual ggBoolean viewingHit(        
             const ggRay3& r,    // ray being sent
             double time,     // time ray is sent
             double tmin, // minimum hit parameter to be searched for
             double tmax, // maximum hit parameter to be searched for
             mrViewingHitRecord&,
             ggMaterialRecord& rec
             ) const;

  //  returns true if bounding box is found
  virtual ggBoolean boundingBox(
             double time1, // low end of time range
             double time2, // high end of time range
             ggBox3& bbox)
             const;



 mrXCylinder(double xmin,  double xmax,
             double yval, double zval, double rval) 
                       { xMin = xmin;  xMax = xmax; r = rval; 
                         yCenter = yval; zCenter = zval; }

  protected:
     mrXCylinder() { }
// range of points: [xMin,xMax]X[yCenter-r, yCenter+r]X[zCenter-r,zCenter+r]
// normal vector points inside if "plus" is 0, and points outside otherwise

     float xMin, xMax, r, yCenter, zCenter;
};


#endif
