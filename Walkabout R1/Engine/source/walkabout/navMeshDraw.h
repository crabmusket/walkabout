//-----------------------------------------------------------------------------
// Walkabout Navigation Toolkit
// Copyright (c) 2012 Daniel Buckmaster
// Permission is NOT granted to freely distribute the contents of this file.
//-----------------------------------------------------------------------------

#ifndef _NAVMESH_DRAW_H_
#define _NAVMESH_DRAW_H_

#include "nav.h"
#include "recast/duDebugDraw.h"
#include "gfx/gfxStateBlock.h"

namespace Nav {
   /// @brief Implements the duDebugDraw interface in Torque.
   /// This class facilitates debug rendering of navmesh data
   /// in an abstracted way so that we don't have to worry
   /// about the rendering code!
   class duDebugDrawTorque: public duDebugDraw {

      GFXStateBlockDesc mDesc;

      bool mQuadsMode;
      U32 mVertCount;
      U32 mPrimType;

      F32 mStore[2][3];

      U32 mColour;
      bool mOverrideColour;
      unsigned char mAlpha;
      bool mOverrideAlpha;

   public:
      duDebugDrawTorque();
      ~duDebugDrawTorque();

      void depthMask(bool state);

      void texture(bool state);

      /// Special colour overwrite for when I get picky about the colours Mikko chose.
      void overrideColour(unsigned int col);

      /// Special alpha override.
      void overrideAlpha(unsigned char alpha);

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
   };
};

#endif
