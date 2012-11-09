//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/groundPlane.h"

#include "renderInstance/renderPassManager.h"
#include "scene/sceneRenderState.h"
#include "materials/sceneData.h"
#include "materials/materialDefinition.h"
#include "materials/materialManager.h"
#include "materials/baseMatInstance.h"
#include "math/util/frustum.h"
#include "math/mPlane.h"
#include "console/consoleTypes.h"
#include "console/engineAPI.h"
#include "core/stream/bitStream.h"
#include "collision/boxConvex.h"
#include "collision/abstractPolyList.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsBody.h"
#include "T3D/physics/physicsCollision.h"


/// Minimum square size allowed.  This is a cheap way to limit the amount
/// of geometry possibly generated by the GroundPlane (vertex buffers have a
/// limit, too).  Dynamically clipping extents into range is a problem since the
/// location of the horizon depends on the camera orientation.  Just shifting
/// squareSize as needed also doesn't work as that causes different geometry to
/// be generated depending on the viewpoint and orientation which affects the
/// texturing.
static const F32 sMIN_SQUARE_SIZE = 16;


IMPLEMENT_CO_NETOBJECT_V1( GroundPlane );

ConsoleDocClass( GroundPlane,
   "@brief An infinite plane extending in all direction.\n\n"
   
   "%GroundPlane is useful for setting up simple testing scenes, or it can be "
   "placed under an existing scene to keep objects from falling into 'nothing'.\n\n"

   "%GroundPlane may not be moved or rotated, it is always at the world origin.\n\n"

   "@ingroup Terrain"   
);

GroundPlane::GroundPlane()
   : mSquareSize( 128.0f ),
     mScaleU( 1.0f ),
     mScaleV( 1.0f ),
     mMaterial( NULL ),
     mMin( 0.0f, 0.0f ),
     mMax( 0.0f, 0.0f ),
     mPhysicsRep( NULL )
{
   mTypeMask |= StaticObjectType | StaticShapeObjectType;
   mNetFlags.set( Ghostable | ScopeAlways );

   mConvexList = new Convex;
}

GroundPlane::~GroundPlane()
{
   if( mMaterial )
      SAFE_DELETE( mMaterial );

   mConvexList->nukeList();
   SAFE_DELETE( mConvexList );
}

void GroundPlane::initPersistFields()
{
   addGroup( "Plane" );

      addField( "squareSize",    TypeF32,          Offset( mSquareSize, GroundPlane ), "Square size in meters to which %GroundPlane subdivides its geometry." );
      addField( "scaleU",        TypeF32,          Offset( mScaleU, GroundPlane ), "Scale of texture repeat in the U direction." );
      addField( "scaleV",        TypeF32,          Offset( mScaleV, GroundPlane ), "Scale of texture repeat in the V direction." );
      addField( "material",      TypeMaterialName, Offset( mMaterialName, GroundPlane ), "Name of Material used to render %GroundPlane's surface." );

   endGroup( "Plane" );
   
   Parent::initPersistFields();

   removeField( "scale" );
   removeField( "position" );
   removeField( "rotation" );
}

bool GroundPlane::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   if( isClientObject() )
      _updateMaterial();
      
   if( mSquareSize < sMIN_SQUARE_SIZE )
   {
      Con::errorf( "GroundPlane - squareSize below threshold; re-setting to %.02f", sMIN_SQUARE_SIZE );
      mSquareSize = sMIN_SQUARE_SIZE;
   }

   Parent::setScale( VectorF( 1.0f, 1.0f, 1.0f ) );
   Parent::setTransform( MatrixF::Identity );
   setGlobalBounds();
   resetWorldBox();

   addToScene();

   if ( PHYSICSMGR )
   {
      PhysicsCollision *colShape = PHYSICSMGR->createCollision();
      colShape->addPlane( PlaneF( Point3F::Zero, Point3F( 0, 0, 1 ) ) ); 

      PhysicsWorld *world = PHYSICSMGR->getWorld( isServerObject() ? "server" : "client" );
      mPhysicsRep = PHYSICSMGR->createBody();
      mPhysicsRep->init( colShape, 0, 0, this, world );
   }

   return true;
}

void GroundPlane::onRemove()
{
   SAFE_DELETE( mPhysicsRep );

   removeFromScene();
   Parent::onRemove();
}

void GroundPlane::inspectPostApply()
{
   Parent::inspectPostApply();
   setMaskBits( U32( -1 ) );

   if( mSquareSize < sMIN_SQUARE_SIZE )
   {
      Con::errorf( "GroundPlane - squareSize below threshold; re-setting to %.02f", sMIN_SQUARE_SIZE );
      mSquareSize = sMIN_SQUARE_SIZE;
   }

   setScale( VectorF( 1.0f, 1.0f, 1.0f ) );
}

void GroundPlane::setTransform( const MatrixF &mat )
{
   // Ignore.
}

void GroundPlane::setScale( const Point3F& scale )
{
   // Ignore.
}

U32 GroundPlane::packUpdate( NetConnection* connection, U32 mask, BitStream* stream )
{
   U32 retMask = Parent::packUpdate( connection, mask, stream );

   stream->write( mSquareSize );
   stream->write( mScaleU );
   stream->write( mScaleV );
   stream->write( mMaterialName );

   return retMask;
}

void GroundPlane::unpackUpdate( NetConnection* connection, BitStream* stream )
{
   Parent::unpackUpdate( connection, stream );

   stream->read( &mSquareSize );
   stream->read( &mScaleU );
   stream->read( &mScaleV );
   stream->read( &mMaterialName );

   // If we're added then something possibly changed in 
   // the editor... do an update of the material and the
   // geometry.
   if ( isProperlyAdded() )
   {
      _updateMaterial();
      mVertexBuffer = NULL;
   }
}

void GroundPlane::_updateMaterial()
{
   if( mMaterialName.isEmpty() )
   {
      Con::warnf( "GroundPlane::_updateMaterial - no material set; defaulting to 'WarningMaterial'" );
      mMaterialName = "WarningMaterial";
   }

   // If the material name matches then don't 
   // bother updating it.
   if (  mMaterial && 
         mMaterialName.compare( mMaterial->getMaterial()->getName() ) == 0 )
      return;

   SAFE_DELETE( mMaterial );

   mMaterial = MATMGR->createMatInstance( mMaterialName, getGFXVertexFormat< VertexType >() );
   if ( !mMaterial )
      Con::errorf( "GroundPlane::_updateMaterial - no material called '%s'", mMaterialName.c_str() );
}

bool GroundPlane::castRay( const Point3F& start, const Point3F& end, RayInfo* info )
{
   PlaneF plane( Point3F( 0.0f, 0.0f, 0.0f ), Point3F( 0.0f, 0.0f, 1.0f ) );

   F32 t = plane.intersect( start, end );
   if( t >= 0.0 && t <= 1.0 )
   {
      info->t = t;
      info->setContactPoint( start, end );
      info->normal.set( 0, 0, 1 );
      info->material = mMaterial;
      info->object = this;
      info->distance = 0;
      info->faceDot = 0;
      info->texCoord.set( 0, 0 );
      return true;
   }

   return false;
}

void GroundPlane::buildConvex( const Box3F& box, Convex* convex )
{
   mConvexList->collectGarbage();

   Box3F planeBox = getPlaneBox();
   if ( !box.isOverlapped( planeBox ) )
      return;

   // See if we already have a convex in the working set.
   BoxConvex *boxConvex = NULL;
   CollisionWorkingList &wl = convex->getWorkingList();
   CollisionWorkingList *itr = wl.wLink.mNext;
   for ( ; itr != &wl; itr = itr->wLink.mNext )
   {
      if (  itr->mConvex->getType() == BoxConvexType &&
            itr->mConvex->getObject() == this )
      {
         boxConvex = (BoxConvex*)itr->mConvex;
         break;
      }
   }

   if ( !boxConvex )
   {
      boxConvex = new BoxConvex;
      mConvexList->registerObject( boxConvex );
      boxConvex->init( this );

      convex->addToWorkingList( boxConvex );
   }

   // Update our convex to best match the queried box
   if ( boxConvex )
   {
      Point3F queryCenter = box.getCenter();

      boxConvex->mCenter      = Point3F( queryCenter.x, queryCenter.y, -GROUND_PLANE_BOX_HEIGHT_HALF );
      boxConvex->mSize        = Point3F( box.getExtents().x,
                                         box.getExtents().y,
                                         GROUND_PLANE_BOX_HEIGHT_HALF );
   }
}

#ifdef TORQUE_WALKABOUT_ENABLED
bool GroundPlane::buildPolyList( PolyListContext context, AbstractPolyList* polyList, const Box3F& box, const SphereF& )
#else
bool GroundPlane::buildPolyList( PolyListContext context, AbstractPolyList* polyList, const Box3F&, const SphereF& )
#endif // TORQUE_WALKABOUT_ENABLED
{
   polyList->setObject( this );
   polyList->setTransform( &MatrixF::Identity, Point3F( 1.0f, 1.0f, 1.0f ) );

#ifdef TORQUE_WALKABOUT_ENABLED
   if(context == PLC_Navigation)
   {
      F32 z = getPosition().z;
      Point3F
         p0(box.minExtents.x, box.maxExtents.y, z),
         p1(box.maxExtents.x, box.maxExtents.y, z),
         p2(box.maxExtents.x, box.minExtents.y, z),
         p3(box.minExtents.x, box.minExtents.y, z);

      // Add vertices to poly list.
      U32 v0 = polyList->addPoint(p0);
      polyList->addPoint(p1);
      polyList->addPoint(p2);
      polyList->addPoint(p3);

      // Add plane between first three vertices.
      polyList->begin(0, 0);
      polyList->vertex(v0);
      polyList->vertex(v0+1);
      polyList->vertex(v0+2);
      polyList->plane(v0, v0+1, v0+2);
      polyList->end();

      // Add plane between last three vertices.
      polyList->begin(0, 1);
      polyList->vertex(v0+2);
      polyList->vertex(v0+3);
      polyList->vertex(v0);
      polyList->plane(v0+2, v0+3, v0);
      polyList->end();

      return true;
   }
#endif // TORQUE_WALKABOUT_ENABLED

   Box3F planeBox = getPlaneBox();
   polyList->addBox( planeBox, mMaterial );

   return true;
}

void GroundPlane::prepRenderImage( SceneRenderState* state )
{
   PROFILE_SCOPE( GroundPlane_prepRenderImage );
   
   // TODO: Should we skip rendering the ground plane into
   // the shadows?  Its not like you can ever get under it.

   if ( !mMaterial )
      return;

   // If we don't have a material instance after the override then 
   // we can skip rendering all together.
   BaseMatInstance *matInst = state->getOverrideMaterial( mMaterial );
   if ( !matInst )
      return;

   PROFILE_SCOPE( GroundPlane_prepRender );

   // Update the geometry.
   createGeometry( state->getFrustum() );
   if( mVertexBuffer.isNull() )
      return;

   // Add a render instance.

   RenderPassManager*   pass  = state->getRenderPass();
   MeshRenderInst*      ri    = pass->allocInst< MeshRenderInst >();

   ri->type                   = RenderPassManager::RIT_Mesh;
   ri->vertBuff               = &mVertexBuffer;
   ri->primBuff               = &mPrimitiveBuffer;
   ri->prim                   = &mPrimitive;
   ri->matInst                = matInst;
   ri->objectToWorld          = pass->allocUniqueXform( MatrixF::Identity );
   ri->worldToCamera          = pass->allocSharedXform( RenderPassManager::View );
   ri->projection             = pass->allocSharedXform( RenderPassManager::Projection );
   ri->visibility             = 1.0f;
   ri->translucentSort        = matInst->getMaterial()->isTranslucent();
   ri->defaultKey             = matInst->getStateHint();

   if( ri->translucentSort )
      ri->type = RenderPassManager::RIT_Translucent;

	// If we need lights then set them up.
   if ( matInst->isForwardLit() )
   {
      LightQuery query;
      query.init( getWorldSphere() );
		query.getLights( ri->lights, 8 );
   }

   pass->addInst( ri );
}

/// Generate a subset of the ground plane matching the given frustum.

void GroundPlane::createGeometry( const Frustum& frustum )
{
   PROFILE_SCOPE( GroundPlane_createGeometry );
   
   enum { MAX_WIDTH = 256, MAX_HEIGHT = 256 };
   
   // Project the frustum onto the XY grid.

   Point2F min;
   Point2F max;

   projectFrustum( frustum, mSquareSize, min, max );
   
   // Early out if the grid projection hasn't changed.

   if(   mVertexBuffer.isValid() && 
         min == mMin && 
         max == mMax )
      return;

   mMin = min;
   mMax = max;

   // Determine the grid extents and allocate the buffers.
   // Adjust square size permanently if with the given frustum,
   // we end up producing more than a certain limit of geometry.
   // This is to prevent this code from causing trouble with
   // long viewing distances.
   // This only affects the client object, of course, and thus
   // has no permanent effect.
   
   U32 width = mCeil( ( max.x - min.x ) / mSquareSize );
   if( width > MAX_WIDTH )
   {
      mSquareSize = mCeil( ( max.x - min.x ) / MAX_WIDTH );
      width = MAX_WIDTH;
   }
   else if( !width )
      width = 1;
   
   U32 height = mCeil( ( max.y - min.y ) / mSquareSize );
   if( height > MAX_HEIGHT )
   {
      mSquareSize = mCeil( ( max.y - min.y ) / MAX_HEIGHT );
      height = MAX_HEIGHT;
   }
   else if( !height )
      height = 1;

   const U32 numVertices   = ( width + 1 ) * ( height + 1 );
   const U32 numTriangles  = width * height * 2;

   // Only reallocate if the buffers are too small.
   if ( mVertexBuffer.isNull() || numVertices > mVertexBuffer->mNumVerts )
   {
#if defined(TORQUE_OS_XENON)
      mVertexBuffer.set( GFX, numVertices, GFXBufferTypeVolatile );
#else
      mVertexBuffer.set( GFX, numVertices, GFXBufferTypeDynamic );
#endif
   }
   if ( mPrimitiveBuffer.isNull() || numTriangles > mPrimitiveBuffer->mPrimitiveCount )
   {
#if defined(TORQUE_OS_XENON)
      mPrimitiveBuffer.set( GFX, numTriangles*3, numTriangles, GFXBufferTypeVolatile );
#else
      mPrimitiveBuffer.set( GFX, numTriangles*3, numTriangles, GFXBufferTypeDynamic );
#endif
   }

   // Generate the grid.

   generateGrid( width, height, mSquareSize, min, max, mVertexBuffer, mPrimitiveBuffer );

   // Set up GFX primitive.

   mPrimitive.type            = GFXTriangleList;
   mPrimitive.numPrimitives   = numTriangles;
   mPrimitive.numVertices     = numVertices;
}

/// Project the given frustum onto the ground plane and return the XY bounds in world space.

void GroundPlane::projectFrustum( const Frustum& frustum, F32 squareSize, Point2F& outMin, Point2F& outMax )
{
   // Get the frustum's min and max XY coordinates.

   const Box3F bounds = frustum.getBounds();

   Point2F minPt( bounds.minExtents.x, bounds.minExtents.y );
   Point2F maxPt( bounds.maxExtents.x, bounds.maxExtents.y );

   // Round the min and max coordinates so they align on the grid.

   minPt.x -= mFmod( minPt.x, squareSize );
   minPt.y -= mFmod( minPt.y, squareSize );

   F32 maxDeltaX = mFmod( maxPt.x, squareSize );
   F32 maxDeltaY = mFmod( maxPt.y, squareSize );

   if( maxDeltaX != 0.0f )
      maxPt.x += ( squareSize - maxDeltaX );
   if( maxDeltaY != 0.0f )
      maxPt.y += ( squareSize - maxDeltaY );

   // Add a safezone, so we don't touch the clipping planes.

   minPt.x -= squareSize; minPt.y -= squareSize;
   maxPt.x += squareSize; maxPt.y += squareSize;

   outMin = minPt;
   outMax = maxPt;
}

/// Generate a triangulated grid spanning the given bounds into the given buffers.

void GroundPlane::generateGrid( U32 width, U32 height, F32 squareSize,
                                const Point2F& min, const Point2F& max,
                                GFXVertexBufferHandle< VertexType >& outVertices,
                                GFXPrimitiveBufferHandle& outPrimitives )
{
   // Generate the vertices.

   VertexType* vertices = outVertices.lock();
   for( F32 y = min.y; y <= max.y; y += squareSize )
      for( F32 x = min.x; x <= max.x; x += squareSize )
      {
         vertices->point.x = x;
         vertices->point.y = y;
         vertices->point.z = 0.0;

         vertices->texCoord.x = ( x / squareSize ) * mScaleU;
         vertices->texCoord.y = ( y / squareSize ) * -mScaleV;

         vertices->normal.x = 0.0f;
         vertices->normal.y = 0.0f;
         vertices->normal.z = 1.0f;

         vertices->tangent.x = 1.0f;
         vertices->tangent.y = 0.0f;
         vertices->tangent.z = 0.0f;

         vertices->binormal.x = 0.0f;
         vertices->binormal.y = 1.0f;
         vertices->binormal.z = 0.0f;

         vertices++;
      }
   outVertices.unlock();

   // Generate the indices.

   U16* indices;
   outPrimitives.lock( &indices );
   
   U16 corner1 = 0;
   U16 corner2 = 1;
   U16 corner3 = width + 1;
   U16 corner4 = width + 2;
   
   for( U32 y = 0; y < height; ++ y )
   {
      for( U32 x = 0; x < width; ++ x )
      {
         indices[ 0 ] = corner3;
         indices[ 1 ] = corner2;
         indices[ 2 ] = corner1;

         indices += 3;

         indices[ 0 ] = corner3;
         indices[ 1 ] = corner4;
         indices[ 2 ] = corner2;

         indices += 3;

         corner1 ++;
         corner2 ++;
         corner3 ++;
         corner4 ++;
      }

      corner1 ++;
      corner2 ++;
      corner3 ++;
      corner4 ++;
   }

   outPrimitives.unlock();
}

DefineEngineMethod( GroundPlane, postApply, void, (),,
                   "Intended as a helper to developers and editor scripts.\n"
                   "Force trigger an inspectPostApply. This will transmit "
                   "material and other fields to client objects."
                   )
{
	object->inspectPostApply();
}