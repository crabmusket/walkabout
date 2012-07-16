//-----------------------------------------------------------------------------
// Torque
// Copyright GarageGames, LLC 2011
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/aiPlayer.h"

#include "console/consoleInternal.h"
#include "math/mMatrix.h"
#include "T3D/gameBase/moveManager.h"
#include "console/engineAPI.h"

IMPLEMENT_CO_NETOBJECT_V1(AIPlayer);

ConsoleDocClass( AIPlayer,
	"@brief A Player object not controlled by conventional input, but by an AI engine.\n\n"

	"The AIPlayer provides a Player object that may be controlled from script.  You control "
   "where the player moves and how fast.  You may also set where the AIPlayer is aiming at "
   "-- either a location or another game object.\n\n"

   "The AIPlayer class does not have a datablock of its own.  It makes use of the PlayerData "
   "datablock to define how it looks, etc.  As the AIPlayer is an extension of the Player class "
   "it can mount objects and fire weapons, or mount vehicles and drive them.\n\n"

   "While the PlayerData datablock is used, there are a number of additional callbacks that are "
   "implemented by AIPlayer on the datablock.  These are listed here:\n\n"

   "void onReachDestination(AIPlayer obj) \n"
   "Called when the player has reached its set destination using the setMoveDestination() method.  "
   "The actual point at which this callback is called is when the AIPlayer is within the mMoveTolerance "
   "of the defined destination.\n\n"

   "void onMoveStuck(AIPlayer obj) \n"
   "While in motion, if an AIPlayer has moved less than moveStuckTolerance within a single tick, this "
   "callback is called.  From here you could choose an alternate destination to get the AIPlayer moving "
   "again.\n\n"

   "void onTargetEnterLOS(AIPlayer obj) \n"
   "When an object is being aimed at (following a call to setAimObject()) and the targeted object enters "
   "the AIPlayer's line of sight, this callback is called.  The LOS test is a ray from the AIPlayer's eye "
   "position to the center of the target's bounding box.  The LOS ray test only checks against interiors, "
   "statis shapes, and terrain.\n\n"

   "void onTargetExitLOS(AIPlayer obj) \n"
   "When an object is being aimed at (following a call to setAimObject()) and the targeted object leaves "
   "the AIPlayer's line of sight, this callback is called.  The LOS test is a ray from the AIPlayer's eye "
   "position to the center of the target's bounding box.  The LOS ray test only checks against interiors, "
   "statis shapes, and terrain.\n\n"

	"@tsexample\n"
	"// Create the demo player object\n"
	"%player = new AiPlayer()\n"
	"{\n"
	"	dataBlock = DemoPlayer;\n"
	"	path = \"\";\n"
	"};\n"
	"@endtsexample\n\n"

	"@see Player for a list of all inherited functions, variables, and base description\n"

	"@ingroup AI\n"
	"@ingroup gameObjects\n");
/**
 * Constructor
 */
AIPlayer::AIPlayer()
{
   mMoveDestination.set( 0.0f, 0.0f, 0.0f );
   mMoveSpeed = 1.0f;
   mMoveTolerance = 0.25f;
   mMoveStuckTolerance = 0.01f;
   mMoveStuckTestDelay = 30;
   mMoveStuckTestCountdown = 0;
   mMoveSlowdown = true;
   mMoveState = ModeStop;

   mAimObject = 0;
   mAimLocationSet = false;
   mTargetInLOS = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);

   mJump = None;
   mNavSize = Regular;
   mLinkTypes.walk = true;
}

/**
 * Destructor
 */
AIPlayer::~AIPlayer()
{
}

void AIPlayer::initPersistFields()
{
   addGroup( "AI" );

      addField( "mMoveTolerance", TypeF32, Offset( mMoveTolerance, AIPlayer ), 
         "@brief Distance from destination before stopping.\n\n"
         "When the AIPlayer is moving to a given destination it will move to within "
         "this distance of the destination and then stop.  By providing this tolerance "
         "it helps the AIPlayer from never reaching its destination due to minor obstacles, "
         "rounding errors on its position calculation, etc.  By default it is set to 0.25.\n");

      addField( "moveStuckTolerance", TypeF32, Offset( mMoveStuckTolerance, AIPlayer ), 
         "@brief Distance tolerance on stuck check.\n\n"
         "When the AIPlayer is moving to a given destination, if it ever moves less than "
         "this tolerance during a single tick, the AIPlayer is considered stuck.  At this point "
         "the onMoveStuck() callback is called on the datablock.\n");

      addField( "moveStuckTestDelay", TypeS32, Offset( mMoveStuckTestDelay, AIPlayer ), 
         "@brief The number of ticks to wait before testing if the AIPlayer is stuck.\n\n"
         "When the AIPlayer is asked to move, this property is the number of ticks to wait "
         "before the AIPlayer starts to check if it is stuck.  This delay allows the AIPlayer "
         "to accelerate to full speed without its initial slow start being considered as stuck.\n"
         "@note Set to zero to have the stuck test start immediately.\n");

   endGroup( "AI" );

   addGroup("Pathfinding");

   addField("allowWalk", TypeBool, Offset(mLinkTypes.walk, AIPlayer),
      "Allow the character to walk on dry land.");
   addField("allowJump", TypeBool, Offset(mLinkTypes.jump, AIPlayer),
      "Allow the character to use jump links.");
   addField("allowDrop", TypeBool, Offset(mLinkTypes.drop, AIPlayer),
      "Allow the character to use drop links.");
   addField("allowSwim", TypeBool, Offset(mLinkTypes.swim, AIPlayer),
      "Allow the character tomove in water.");
   addField("allowLedge", TypeBool, Offset(mLinkTypes.ledge, AIPlayer),
      "Allow the character to jump ledges.");
   addField("allowClimb", TypeBool, Offset(mLinkTypes.climb, AIPlayer),
      "Allow the character to use climb links.");
   addField("allowTeleport", TypeBool, Offset(mLinkTypes.teleport, AIPlayer),
      "Allow the character to use teleporters.");

   endGroup("Pathfinding");

   Parent::initPersistFields();
}

bool AIPlayer::onAdd()
{
   if (!Parent::onAdd())
      return false;

   // Use the eye as the current position (see getAIMove)
   MatrixF eye;
   getEyeTransform(&eye);
   mLastLocation = eye.getPosition();

   return true;
}

void AIPlayer::onRemove() 
{
   clearPath();
   clearCover();
   Parent::onRemove();
}

/**
 * Sets the speed at which this AI moves
 *
 * @param speed Speed to move, default player was 10
 */
void AIPlayer::setMoveSpeed( F32 speed )
{
   mMoveSpeed = getMax(0.0f, getMin( 1.0f, speed ));
}

/**
 * Stops movement for this AI
 */
void AIPlayer::stopMove()
{
   mMoveState = ModeStop;
}

/**
 * Sets how far away from the move location is considered
 * "on target"
 *
 * @param tolerance Movement tolerance for error
 */
void AIPlayer::setMoveTolerance( const F32 tolerance )
{
   mMoveTolerance = getMax( 0.1f, tolerance );
}

/**
 * Sets the location for the bot to run to
 *
 * @param location Point to run to
 */
void AIPlayer::setMoveDestination( const Point3F &location, bool slowdown )
{
   mMoveDestination = location;
   mMoveState = ModeMove;
   mMoveSlowdown = slowdown;
   mMoveStuckTestCountdown = mMoveStuckTestDelay;
}

/**
 * Sets the object the bot is targeting
 *
 * @param targetObject The object to target
 */
void AIPlayer::setAimObject( GameBase *targetObject )
{
   mAimObject = targetObject;
   mTargetInLOS = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * Sets the object the bot is targeting and an offset to add to target location
 *
 * @param targetObject The object to target
 * @param offset       The offest from the target location to aim at
 */
void AIPlayer::setAimObject( GameBase *targetObject, Point3F offset )
{
   mAimObject = targetObject;
   mTargetInLOS = false;
   mAimOffset = offset;
}

/**
 * Sets the location for the bot to aim at
 *
 * @param location Point to aim at
 */
void AIPlayer::setAimLocation( const Point3F &location )
{
   mAimObject = 0;
   mAimLocationSet = true;
   mAimLocation = location;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * Clears the aim location and sets it to the bot's
 * current destination so he looks where he's going
 */
void AIPlayer::clearAim()
{
   mAimObject = 0;
   mAimLocationSet = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * This method calculates the moves for the AI player
 *
 * @param movePtr Pointer to move the move list into
 */
bool AIPlayer::getAIMove(Move *movePtr)
{
   *movePtr = NullMove;

   // Use the eye as the current position.
   MatrixF eye;
   getEyeTransform(&eye);
   Point3F location = eye.getPosition();
   Point3F rotation = getRotation();

   updateNavMesh();

   // Orient towards the aim point, aim object, or towards
   // our destination.
   if (mAimObject || mAimLocationSet || mMoveState != ModeStop) {

      // Update the aim position if we're aiming for an object
      if (mAimObject)
         mAimLocation = mAimObject->getPosition() + mAimOffset;
      else
         if (!mAimLocationSet)
            mAimLocation = mMoveDestination;

      F32 xDiff = mAimLocation.x - location.x;
      F32 yDiff = mAimLocation.y - location.y;
      if (!mIsZero(xDiff) || !mIsZero(yDiff)) {

         // First do Yaw
         // use the cur yaw between -Pi and Pi
         F32 curYaw = rotation.z;
         while (curYaw > M_2PI_F)
            curYaw -= M_2PI_F;
         while (curYaw < -M_2PI_F)
            curYaw += M_2PI_F;

         // find the yaw offset
         F32 newYaw = mAtan2( xDiff, yDiff );
         F32 yawDiff = newYaw - curYaw;

         // make it between 0 and 2PI
         if( yawDiff < 0.0f )
            yawDiff += M_2PI_F;
         else if( yawDiff >= M_2PI_F )
            yawDiff -= M_2PI_F;

         // now make sure we take the short way around the circle
         if( yawDiff > M_PI_F )
            yawDiff -= M_2PI_F;
         else if( yawDiff < -M_PI_F )
            yawDiff += M_2PI_F;

         movePtr->yaw = yawDiff;

         // Next do pitch.
         if (!mAimObject && !mAimLocationSet) {
            // Level out if were just looking at our next way point.
            Point3F headRotation = getHeadRotation();
            movePtr->pitch = -headRotation.x;
         }
         else {
            // This should be adjusted to run from the
            // eye point to the object's center position. Though this
            // works well enough for now.
            F32 vertDist = mAimLocation.z - location.z;
            F32 horzDist = mSqrt(xDiff * xDiff + yDiff * yDiff);
            F32 newPitch = mAtan2( horzDist, vertDist ) - ( M_PI_F / 2.0f );
            if (mFabs(newPitch) > 0.01f) {
               Point3F headRotation = getHeadRotation();
               movePtr->pitch = newPitch - headRotation.x;
            }
         }
      }
   }
   else {
      // Level out if we're not doing anything else
      Point3F headRotation = getHeadRotation();
      movePtr->pitch = -headRotation.x;
   }

   // Move towards the destination
   if (mMoveState != ModeStop) {
      F32 xDiff = mMoveDestination.x - location.x;
      F32 yDiff = mMoveDestination.y - location.y;

      // Check if we should mMove, or if we are 'close enough'
      if (mFabs(xDiff) < mMoveTolerance && mFabs(yDiff) < mMoveTolerance) {
         mMoveState = ModeStop;
         onReachDestination();
      }
      else {
         // Build move direction in world space
         if (mIsZero(xDiff))
            movePtr->y = (location.y > mMoveDestination.y) ? -1.0f : 1.0f;
         else
            if (mIsZero(yDiff))
               movePtr->x = (location.x > mMoveDestination.x) ? -1.0f : 1.0f;
            else
               if (mFabs(xDiff) > mFabs(yDiff)) {
                  F32 value = mFabs(yDiff / xDiff);
                  movePtr->y = (location.y > mMoveDestination.y) ? -value : value;
                  movePtr->x = (location.x > mMoveDestination.x) ? -1.0f : 1.0f;
               }
               else {
                  F32 value = mFabs(xDiff / yDiff);
                  movePtr->x = (location.x > mMoveDestination.x) ? -value : value;
                  movePtr->y = (location.y > mMoveDestination.y) ? -1.0f : 1.0f;
               }

         // Rotate the move into object space (this really only needs
         // a 2D matrix)
         Point3F newMove;
         MatrixF moveMatrix;
         moveMatrix.set(EulerF(0.0f, 0.0f, -(rotation.z + movePtr->yaw)));
         moveMatrix.mulV( Point3F( movePtr->x, movePtr->y, 0.0f ), &newMove );
         movePtr->x = newMove.x;
         movePtr->y = newMove.y;

         // Set movement speed.  We'll slow down once we get close
         // to try and stop on the spot...
         if (mMoveSlowdown) {
            F32 speed = mMoveSpeed;
            F32 dist = mSqrt(xDiff*xDiff + yDiff*yDiff);
            F32 maxDist = 5.0f;
            if (dist < maxDist)
               speed *= dist / maxDist;
            movePtr->x *= speed;
            movePtr->y *= speed;
         }
         else {
            movePtr->x *= mMoveSpeed;
            movePtr->y *= mMoveSpeed;
         }

         // Reset a previous stuck mode
         mMoveState = ModeMove;

         if (mMoveStuckTestCountdown > 0)
         {
            --mMoveStuckTestCountdown;
         }
         else
         {
            // We should check to see if we are stuck...
            F32 locationDelta = (location - mLastLocation).len();
            if (locationDelta < mMoveStuckTolerance) {
               mMoveState = ModeStuck;
               onStuck();
            }
         }
      }
   }

   // Test for target location in sight if it's an object. The LOS is
   // run from the eye position to the center of the object's bounding,
   // which is not very accurate.
   if (mAimObject) {
      MatrixF eyeMat;
      getEyeTransform(&eyeMat);
      eyeMat.getColumn(3,&location);
      Point3F targetLoc = mAimObject->getBoxCenter();

      // This ray ignores non-static shapes. Cast Ray returns true
      // if it hit something.
      RayInfo dummy;
      if (getContainer()->castRay( location, targetLoc,
            InteriorObjectType | StaticShapeObjectType | StaticObjectType |
            TerrainObjectType, &dummy)) {
         if (mTargetInLOS) {
            throwCallback( "onTargetExitLOS" );
            mTargetInLOS = false;
         }
      }
      else
         if (!mTargetInLOS) {
            throwCallback( "onTargetEnterLOS" );
            mTargetInLOS = true;
         }
   }

   // Replicate the trigger state into the move so that
   // triggers can be controlled from scripts.
   for( int i = 0; i < MaxTriggerKeys; i++ )
      movePtr->trigger[i] = getImageTriggerState(i);

   if(mJump == Now)
   {
      movePtr->trigger[2] = true;
      mJump = None;
   }
   else if(mJump == Ledge)
   {
      // If we're not touching the ground, jump!
      RayInfo info;
      if(!getContainer()->castRay(getPosition(), getPosition() - Point3F(0, 0, 0.4f), StaticShapeObjectType, &info))
      {
         movePtr->trigger[2] = true;
         mJump = None;
      }
   }

   mLastLocation = location;

   return true;
}

/**
 * Utility function to throw callbacks. Callbacks always occure
 * on the datablock class.
 *
 * @param name Name of script function to call
 */
void AIPlayer::throwCallback( const char *name )
{
   Con::executef(getDataBlock(), name, getIdString());
}

/**
 * Called when we get within mMoveTolerance of our destination set using
 * setMoveDestination(). Only fires the script callback if we are at the end
 * of a pathfinding path, or have no pathfinding path.
 */
void AIPlayer::onReachDestination()
{
   if(!mPathData.path.isNull())
   {
      if(mPathData.index == mPathData.path->size() - 1)
      {
         // Handle looping paths.
         if(mPathData.path->mIsLooping)
            moveToNode(0);
         // Otherwise end path.
         else
         {
            clearPath();
            throwCallback("onReachDestination");
         }
      }
      else
      {
         moveToNode(mPathData.index + 1);
         // Throw callback every time if we're on a looping path.
         //if(mPathData.path->mIsLooping)
            //throwCallback("onReachDestination");
      }
   }
   else
      throwCallback("onReachDestination");
}

/**
 * Called when we move less than mMoveStuckTolerance in a tick, signalling
 * that some obstacle is preventing us from getting where we need to go.
 */
void AIPlayer::onStuck()
{
   if(!mPathData.path.isNull())
      repath();
   else
      throwCallback("onMoveStuck");
}

// --------------------------------------------------------------------------------------------
// Pathfinding
// --------------------------------------------------------------------------------------------

void AIPlayer::clearPath()
{
   // Only delete if we own the path.
   if(!mPathData.path.isNull() && mPathData.owned)
      mPathData.path->deleteObject();
   // Reset path data.
   mPathData = PathData();
   // Stop moving.
   stopMove();
}

void AIPlayer::clearCover()
{
   // Notify cover that we are no longer on our way.
   if(!mCoverData.cover.isNull())
      mCoverData.cover->setOccupied(false);
   mCoverData = CoverData();
}

void AIPlayer::moveToNode(S32 node)
{
   if(mPathData.path.isNull())
      return;

   // -1 is shorthand for 'last path node'.
   if(node == -1)
      node = mPathData.path->size() - 1;

   // Consider slowing down on the last path node.
   setMoveDestination(mPathData.path->getNode(node), false);

   // Check flags for this segment.
   if(mPathData.index)
   {
      U16 flags = mPathData.path->getFlags(node - 1);
      // Jump if we must.
      if(flags & Nav::LedgeFlag)
         mJump = Ledge;
      else if(flags & Nav::JumpFlag)
         mJump = Now;
      else
         // Catch pathing errors.
         mJump = None;
   }

   // Store current index.
   mPathData.index = node;
}

void AIPlayer::setPathDestination(const Point3F &pos)
{
   // Pathfinding only happens on the server.
   if(!isServerObject())
      return;

   if(!getNavMesh())
      updateNavMesh();
   // If we can't find a mesh, just move regularly.
   //if(!getNavMesh())
   //{
   //   setMoveDestination(pos);
   //   return;
   //}

   // Create a new path.
   Nav::NavPath *path = new Nav::NavPath();
   if(path)
   {
      path->mMesh = getNavMesh();
      path->mFrom = getPosition();
      path->mTo = pos;
      path->mFromSet = path->mToSet = true;
      path->mAlwaysRender = true;
      path->mLinkTypes = mLinkTypes;
      // Paths plan automatically upon being registered.
      if(!path->registerObject())
      {
         delete path;
         return;
      }
   }
   else
      return;

   if(path->success())
   {
      // Clear any current path we might have.
      clearPath();
      clearCover();
      // Store new path.
      mPathData.path = path;
      mPathData.owned = true;
      // Skip node 0, which we are currently standing on.
      moveToNode(1);
   }
   else
   {
      // Just move normally if we can't path.
      //setMoveDestination(pos, true);
      //return;
      throwCallback("onPathFailed");
      path->deleteObject();
   }
}

DefineEngineMethod(AIPlayer, setPathDestination, void, (Point3F goal),,
   "@brief Tells the AI to find a path to the location provided\n\n"

   "@param goal Coordinates in world space representing location to move to.\n\n"

   "@see getPathDestination()\n"
   "@see setMoveDestination()\n")
{
   object->setPathDestination(goal);
}

Point3F AIPlayer::getPathDestination() const
{
   if(!mPathData.path.isNull())
      return mPathData.path->mTo;
   return Point3F(0, 0, 0);
}

DefineEngineMethod(AIPlayer, getPathDestination, Point3F, (),,
   "@brief Get the AIPlayer's current pathfinding destination.\n\n"

   "@return Returns a point containing the \"x y z\" position "
   "of the AIPlayer's current path destination. If no path destination "
   "has yet been set, this returns \"0 0 0\"."

   "@see setPathDestination()\n")
{
	return object->getPathDestination();
}

void AIPlayer::followNavPath(Nav::NavPath *path)
{
   if(!isServerObject())
      return;

   // Get rid of our current path.
   clearPath();
   clearCover();

   // Follow new path.
   mPathData.path = path;
   mPathData.owned = false;
   // Start from 0 since we might not already be there.
   moveToNode(0);
}

DefineEngineMethod(AIPlayer, followNavPath, void, (SimObjectId obj),,
   "@brief Tell the AIPlayer to follow a path.\n\n"

   "@param obj ID of a NavPath object for the character to follow.")
{
   Nav::NavPath *path;
   if(Sim::findObject(obj, path))
      object->followNavPath(path);
}

void AIPlayer::followObject(SceneObject *obj)
{
}

void AIPlayer::repath()
{
   // Ineffectual if we don't have a path, or are using someone else's.
   if(mPathData.path.isNull() || !mPathData.owned)
      return;

   // Update from position and replan.
   mPathData.path->mFrom = getPosition();
   mPathData.path->plan();
   // Move to first node (skip start pos).
   moveToNode(1);
}

DefineEngineMethod(AIPlayer, repath, void, (),,
   "@brief Tells the AI to re-plan its path. Does nothing if the character "
   "has no path, or if it is following a mission path.\n\n")
{
   object->repath();
}

struct CoverSearch
{
   Point3F loc;
   Point3F from;
   F32 dist;
   F32 best;
   CoverPoint *point;
   CoverSearch() : loc(0, 0, 0), from(0, 0, 0)
   {
      best = -FLT_MAX;
      point = NULL;
      dist = FLT_MAX;
   }
};

static void findCoverCallback(SceneObject *obj, void *key)
{
   CoverPoint *p = dynamic_cast<CoverPoint*>(obj);
   if(!p || p->isOccupied())
      return;
   CoverSearch *s = static_cast<CoverSearch*>(key);
   Point3F dir = s->from - p->getPosition();
   dir.normalizeSafe();
   // Score first based on angle of cover point to enemy.
   F32 score = mDot(p->getNormal(), dir);
   // Score also based on distance from seeker.
   score -= (p->getPosition() - s->loc).len() / s->dist;
   // Finally, consider cover size.
   score += (p->getSize() + 1) / CoverPoint::NumSizes;
   score *= p->getQuality();
   if(score > s->best)
   {
      s->best = score;
      s->point = p;
   }
}

void AIPlayer::findCover(const Point3F &from, F32 radius)
{
   if(radius <= 0)
      return;

   // Create a search state.
   CoverSearch s;
   s.loc = getPosition();
   s.dist = radius;
   // Direction we seek cover FROM.
   s.from = from;

   // Find cover points.
   Box3F box(radius * 2.0f);
   box.setCenter(getPosition());
   getContainer()->findObjects(box, MarkerObjectType, findCoverCallback, &s);

   // Go to cover!
   if(s.point)
   {
      setPathDestination(s.point->getPosition());
      clearCover();
      mCoverData.cover = s.point;
      s.point->setOccupied(true);
   }
}

DefineEngineMethod(AIPlayer, findCover, void, (Point3F from, F32 radius),,
   "@brief Tells the AI to find cover nearby.\n\n"

   "@param from   Location to find cover from (i.e., enemy position).\n"
   "@param radius Distance to search for cover.\n\n")
{
   object->findCover(from, radius);
}

Nav::NavMesh *AIPlayer::findNavMesh() const
{
   // Search for NavMeshes that contain us entirely with the smallest possible
   // volume.
   Nav::NavMesh *mesh = NULL;
   SimSet *set = Nav::NavMesh::getServerSet();
   for(U32 i = 0; i < set->size(); i++)
   {
      Nav::NavMesh *m = static_cast<Nav::NavMesh*>(set->at(i));
      if(m->getWorldBox().isContained(getWorldBox()))
      {
         // Check that mesh size is appropriate.
         if(mMount.object) // Should use isMounted() but it's not const. Grr.
         {
            if(!m->mVehicles)
               continue;
         }
         else
         {
            if(getNavSize() == Small && !m->mSmallCharacters ||
               getNavSize() == Regular && !m->mRegularCharacters ||
               getNavSize() == Large && !m->mLargeCharacters)
               continue;
         }
         if(!mesh || m->getWorldBox().getVolume() < mesh->getWorldBox().getVolume())
            mesh = m;
      }
   }
   return mesh;
}

DefineEngineMethod(AIPlayer, findNavMesh, S32, (),,
   "@brief Get the NavMesh object this AIPlayer is currently using.\n\n"

   "@return The ID of the NavPath object this character is using for "
   "pathfinding. This is determined by the character's location, "
   "navigation type and other factors. Returns -1 if no NavMesh is "
   "found.")
{
   Nav::NavMesh *mesh = object->getNavMesh();
   return mesh ? mesh->getId() : -1;
}

void AIPlayer::updateNavMesh()
{
   Nav::NavMesh *old = mNavMesh;
   if(mNavMesh.isNull())
      mNavMesh = findNavMesh();
   else
   {
      if(!mNavMesh->getWorldBox().isContained(getWorldBox()))
         mNavMesh = findNavMesh();
   }
   // See if we need to update our path.
   if(mNavMesh != old && !mPathData.path.isNull())
   {
      setPathDestination(mPathData.path->mTo);
   }
}

DefineEngineMethod(AIPlayer, getNavMesh, S32, (),,
   "@brief Return the NavMesh this AIPlayer is using to navigate.\n\n")
{
   Nav::NavMesh *m = object->getNavMesh();
   return m ? m->getId() : 0;
}

DefineEngineMethod(AIPlayer, setNavSize, void, (const char *size),,
   "@brief Set the size of NavMesh this character uses. One of \"Small\", \"Regular\" or \"Large\".")
{
   if(!dStrcmp(size, "Small"))
      object->setNavSize(AIPlayer::Small);
   else if(!dStrcmp(size, "Regular"))
      object->setNavSize(AIPlayer::Regular);
   else if(!dStrcmp(size, "Large"))
      object->setNavSize(AIPlayer::Large);
   else
      Con::errorf("AIPlayer::setNavSize: no such size '%s'.", size);
}

DefineEngineMethod(AIPlayer, getNavSize, const char*, (),,
   "@brief Return the size of NavMesh this character uses for pathfinding.")
{
   switch(object->getNavSize())
   {
   case AIPlayer::Small:
      return "Small";
   case AIPlayer::Regular:
      return "Regular";
   case AIPlayer::Large:
      return "Large";
   }
   return "";
}

// --------------------------------------------------------------------------------------------
// Console Functions
// --------------------------------------------------------------------------------------------

DefineEngineMethod( AIPlayer, stop, void, ( ),,
   "@brief Tells the AIPlayer to stop moving.\n\n")
{
   object->stopMove();
}

DefineEngineMethod( AIPlayer, clearAim, void, ( ),,
   "@brief Use this to stop aiming at an object or a point.\n\n"
   
   "@see setAimLocation()\n"
   "@see setAimObject()\n")
{
   object->clearAim();
}

DefineEngineMethod( AIPlayer, setMoveSpeed, void, ( F32 speed ),,
   "@brief Sets the move speed for an AI object.\n\n"

   "@param speed A speed multiplier between 0.0 and 1.0.  "
   "This is multiplied by the AIPlayer's base movement rates (as defined in "
   "its PlayerData datablock)\n\n"
   
   "@see getMoveDestination()\n")
{
	object->setMoveSpeed(speed);
}

DefineEngineMethod( AIPlayer, setMoveDestination, void, ( Point3F goal, bool slowDown ), ( true ),
   "@brief Tells the AI to move to the location provided\n\n"

   "@param goal Coordinates in world space representing location to move to.\n"
   "@param slowDown A boolean value. If set to true, the bot will slow down "
   "when it gets within 5-meters of its move destination. If false, the bot "
   "will stop abruptly when it reaches the move destination. By default, this is true.\n\n"

   "@note Upon reaching a move destination, the bot will clear its move destination and "
   "calls to getMoveDestination will return \"0 0 0\"."
   
   "@see getMoveDestination()\n")
{
   object->setMoveDestination( goal, slowDown);
}

DefineEngineMethod( AIPlayer, getMoveDestination, Point3F, (),,
   "@brief Get the AIPlayer's current destination.\n\n"

   "@return Returns a point containing the \"x y z\" position "
   "of the AIPlayer's current move destination. If no move destination "
   "has yet been set, this returns \"0 0 0\"."
   
   "@see setMoveDestination()\n")
{
	return object->getMoveDestination();
}

DefineEngineMethod( AIPlayer, setAimLocation, void, ( Point3F target ),,
   "@brief Tells the AIPlayer to aim at the location provided.\n\n"

   "@param target An \"x y z\" position in the game world to target.\n\n"
   
   "@see getAimLocation()\n")
{
	object->setAimLocation(target);
}

DefineEngineMethod( AIPlayer, getAimLocation, Point3F, (),,
   "@brief Returns the point the AIPlayer is aiming at.\n\n"

   "This will reflect the position set by setAimLocation(), "
   "or the position of the object that the bot is now aiming at.  "
   "If the bot is not aiming at anything, this value will "
   "change to whatever point the bot's current line-of-sight intercepts."

   "@return World space coordinates of the object AI is aiming at. Formatted as \"X Y Z\".\n\n"
   
   "@see setAimLocation()\n"
   "@see setAimObject()\n")
{
	return object->getAimLocation();
}

ConsoleDocFragment _setAimObject(
   "@brief Sets the AIPlayer's target object.  May optionally set an offset from target location\n\n"

   "@param targetObject The object to target\n"
   "@param offset Optional three-element offset vector which will be added to the position of the aim object.\n\n"

   "@tsexample\n"
   "// Without an offset\n"
   "%ai.setAimObject(%target);\n\n"
   "// With an offset\n"
   "// Cause our AI object to aim at the target\n"
   "// offset (0, 0, 1) so you don't aim at the target's feet\n"
   "%ai.setAimObject(%target, \"0 0 1\");\n"
   "@endtsexample\n\n"
   
   "@see getAimLocation()\n"
   "@see getAimObject()\n"
   "@see clearAim()\n",

   "AIPlayer",
   "void setAimObject(GameBase targetObject, Point3F offset);"
);
ConsoleMethod( AIPlayer, setAimObject, void, 3, 4, "( GameBase obj, [Point3F offset] )"
              "Sets the bot's target object. Optionally set an offset from target location."
			  "@hide")
{
   Point3F off( 0.0f, 0.0f, 0.0f );

   // Find the target
   GameBase *targetObject;
   if( Sim::findObject( argv[2], targetObject ) )
   {
      if (argc == 4)
         dSscanf( argv[3], "%g %g %g", &off.x, &off.y, &off.z );

      object->setAimObject( targetObject, off );
   }
   else
      object->setAimObject( 0, off );
}

DefineEngineMethod( AIPlayer, getAimObject, S32, (),,
   "@brief Gets the object the AIPlayer is targeting.\n\n"

   "@return Returns -1 if no object is being aimed at, "
   "or the SimObjectID of the object the AIPlayer is aiming at.\n\n"
   
   "@see setAimObject()\n")
{
	GameBase* obj = object->getAimObject();
   return obj? obj->getId(): -1;
}
