//-----------------------------------------------------------------------------
// Walkabout Navigation Toolkit
// Copyright (c) 2012 Daniel Buckmaster
// Permission is NOT granted to freely distribute the contents of this file.
//-----------------------------------------------------------------------------

#ifndef _NAVMESH_DRAW_H_
#define _NAVMESH_DRAW_H_

#include "nav.h"
#include "core/util/tVector.h"
#include "recast/duDebugDraw.h"
#include "gfx/gfxStateBlock.h"

/// @brief Implements the duDebugDraw interface in Torque.
class duDebugDrawTorque: public duDebugDraw {
public:
   duDebugDrawTorque();
   ~duDebugDrawTorque();

   /// Enable/disable Z read.
   void depthMask(bool state);

   /// Enable/disable texturing. Not used.
   void texture(bool state);

   /// Special colour overwrite for when I get picky about the colours Mikko chose.
   void overrideColor(unsigned int col);

   /// Stop the colour override.
   void cancelOverride();

   /// Begin drawing primitives.
   /// @param prim [in] primitive type to draw, one of rcDebugDrawPrimitives.
   /// @param size [in] size of a primitive, applies to point size and line width only.
   void begin(duDebugDrawPrimitives prim, float size = 1.0f);

   /// Submit a vertex
   /// @param pos [in] position of the verts.
   /// @param color [in] color of the verts.
   void vertex(const float* pos, unsigned int color);

   /// Submit a vertex
   /// @param x,y,z [in] position of the verts.
   /// @param color [in] color of the verts.
   void vertex(const float x, const float y, const float z, unsigned int color);

   /// Submit a vertex
   /// @param pos [in] position of the verts.
   /// @param color [in] color of the verts.
   void vertex(const float* pos, unsigned int color, const float* uv);

   /// Submit a vertex
   /// @param x,y,z [in] position of the verts.
   /// @param color [in] color of the verts.
   void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v);

   /// End drawing primitives.
   void end();

   /// Render buffered primitive.
   void render();

   /// Delete buffered primitive.
   void clear();
      
private:
   GFXStateBlockDesc mDesc;

   U32 mPrimType;
   bool mQuadsMode;

   U32 mVertCount;
   F32 mStore[2][3];

   struct Vertex {
      // Contain either a point or a color command.
      union {
         struct {
            U8 r, g, b, a;
         } color;
         struct {
            float x, y, z;
         } point;
         U32 primType;
      } data;
      // Which type of data do we store?
      enum {
         COLOR,
         POINT,
         PRIMTYPE,
      } type;
      // Construct as color instruction.
      Vertex(U8 r, U8 g, U8 b, U8 a) {
         type = COLOR;
         data.color.r = r;
         data.color.g = g;
         data.color.b = b;
         data.color.a = a;
      }
      // Construct as point.
      Vertex(float x, float y, float z) {
         type = POINT;
         data.point.x = x;
         data.point.y = y;
         data.point.z = z;
      }
      Vertex(U32 t = 0) {
         type = PRIMTYPE;
         data.primType = t;
      }
   };

   struct Buffer {
      Vector<Vertex> buffer;
      GFXPrimitiveType primType;
      Buffer(U32 type = 0) {
         primType = (GFXPrimitiveType)type;
      }
   };
   Vector<Buffer> mBuffers;

   U32 mCurrColor;
   U32 mOverrideColor;
   bool mOverride;

   void _vertex(const float x, const float y, const float z, unsigned int color);
};

#endif
