//
// ObjPolyList class for T3D 1.1
// Copyright (c) Daniel Buckmaster 2012
//

#include "objPolyList.h"
#include "platform/platform.h"

#include "gfx/gfxDevice.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxStateBlock.h"
//#include "scene/sceneContainer.h"

ObjPolyList::ObjPolyList()
{
   nverts = 0;
   verts = NULL;
   vertcap = 0;

   ntris = 0;
   tris = NULL;
   tricap = 0;
}

ObjPolyList::~ObjPolyList()
{
   clear();
}

void ObjPolyList::clear()
{
   nverts = 0;
   delete[] verts;
   verts = NULL;
   vertcap = 0;

   ntris = 0;
   delete[] tris;
   tris = NULL;
   tricap = 0;
}

bool ObjPolyList::isEmpty() const
{
   return getTriCount() == 0;
}

U32 ObjPolyList::addPoint(const Point3F &p)
{
   // If we've reached the vertex cap, double the array size.
   if(nverts == vertcap)
   {
      // vertcap starts at 64, otherwise it doubles.
      if(vertcap == 0) vertcap = 16;
      else vertcap *= 2;
      // Allocate new vertex storage.
      F32 *newverts = new F32[vertcap*3];
      if(!newverts)
         return 0;
      dMemcpy(newverts, verts, nverts*3 * sizeof(F32));
      dFree(verts);
      verts = newverts;
   }
   Point3F v = p;
   mMatrix.mulP(v);
   // Insert the new vertex.
   verts[nverts*3] = v.x;
   verts[nverts*3+1] = v.z;
   verts[nverts*3+2] = -v.y;
   // Return nverts before incrementing it.
   return nverts++;
}

U32 ObjPolyList::addPlane(const PlaneF &plane)
{
   planes.increment();
   mPlaneTransformer.transform(plane, planes.last());
   return planes.size() - 1;
}

void ObjPolyList::begin(BaseMatInstance *material, U32 surfaceKey)
{
   vidx = 0;
   // If we've reached the tri cap, grow the array.
   if(ntris == tricap)
   {
      if(tricap == 0) tricap = 16;
      else tricap *= 2;
      // Allocate new vertex storage.
      S32 *newtris = new S32[tricap*3];
      if(!newtris)
         return;
      dMemcpy(newtris, tris, ntris*3 * sizeof(S32));
      dFree(tris);
      tris = newtris;
   }
}

void ObjPolyList::plane(U32 v1, U32 v2, U32 v3)
{
}

void ObjPolyList::plane(const PlaneF& p)
{
}

void ObjPolyList::plane(const U32 index)
{
}

void ObjPolyList::vertex(U32 vi)
{
   if(vidx == 3)
      return;
   tris[ntris*3+2-vidx] = vi;
   vidx++;
}

void ObjPolyList::end()
{
   ntris++;
}

U32 ObjPolyList::getVertCount() const
{
   return nverts;
}

const F32 *ObjPolyList::getVerts() const
{
   return verts;
}

U32 ObjPolyList::getTriCount() const
{
   return ntris;
}

const S32 *ObjPolyList::getTris() const
{
   return tris;
}

void ObjPolyList::renderWire() const
{
   GFXStateBlockDesc desc;
   desc.setCullMode(GFXCullNone);
   desc.setZReadWrite(false, false);
   //desc.setBlend(true);
   GFXStateBlockRef sb = GFX->createStateBlock(desc);
   GFX->setStateBlock(sb);

   PrimBuild::color3i(255, 0, 255);

   for(U32 t = 0; t < getTriCount(); t++)
   {
      PrimBuild::begin(GFXLineStrip, 4);

      PrimBuild::vertex3f(verts[tris[t*3]*3],   -verts[tris[t*3]*3+2],   verts[tris[t*3]*3+1]);
      PrimBuild::vertex3f(verts[tris[t*3+1]*3], -verts[tris[t*3+1]*3+2], verts[tris[t*3+1]*3+1]);
      PrimBuild::vertex3f(verts[tris[t*3+2]*3], -verts[tris[t*3+2]*3+2], verts[tris[t*3+2]*3+1]);
      PrimBuild::vertex3f(verts[tris[t*3]*3],   -verts[tris[t*3]*3+2],   verts[tris[t*3]*3+1]);

      PrimBuild::end();
   }
}
