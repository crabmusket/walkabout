//
// ObjPolyList class for T3D 1.1
// Copyright (c) Daniel Buckmaster 2012
//

#ifndef _OBJPOLYLIST_H_
#define _OBJPOLYLIST_H_

#include "abstractPolyList.h"
#include "core/util/tVector.h"
//#include "math/mBox.h"

/// Represents polygons in the same manner as the .obj file format. Handy for
/// padding data to Recast, since it expects this data format. At the moment,
/// this class only accepts triangles.
/// @see AbstractPolyList
class ObjPolyList : public AbstractPolyList {
public:
   /// @name AbstractPolyList
   /// @{

   bool isEmpty() const;

   U32 addPoint(const Point3F &p);
   U32 addPlane(const PlaneF &plane);

   void begin(BaseMatInstance *material, U32 surfaceKey);

   void plane(U32 v1, U32 v2, U32 v3);
   void plane(const PlaneF& p);
   void plane(const U32 index);

   void vertex(U32 vi);

   void end();

   /// @}

   /// @name Data interface
   /// @{

   U32 getVertCount() const;
   const F32 *getVerts() const;

   U32 getTriCount() const;
   const S32 *getTris() const;

   //static void parse(ObjPolyList &list, Box3F box, U32 mask);

   void clear();

   void renderWire() const;

   /// @}

   /// Default constructor.
   ObjPolyList();
   /// Default destructor.
   ~ObjPolyList();

protected:
   /// Number of vertices defined.
   U32 nverts;
   /// Array of vertex coordinates. Size nverts*3
   F32 *verts;
   /// Size of vertex array.
   U32 vertcap;

   /// Number of triangles defined.
   U32 ntris;
   /// Array of triangle vertex indices. Size ntris*3
   S32 *tris;
   /// Size of triangle array.
   U32 tricap;

   /// Index of vertex we're adding to the current triangle.
   U8 vidx;

   /// Store a list of planes - not actually used.
   Vector<PlaneF> planes;
   /// Another inherited utility function.
   const PlaneF& getIndexedPlane(const U32 index) { return planes[index]; }

private:
};

#endif
