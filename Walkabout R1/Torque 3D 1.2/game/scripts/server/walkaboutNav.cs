//-----------------------------------------------------------------------------
// Walkabout
// Copyright Daniel Buckmaster 2012 dan.buckmaster@gmail.com
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Walkabout namespace
// Basically, provides a utility with hooks in onAdd and onRemove to update all
// NavMeshes. handy if you have lots of types of shapes you want to be 'dynamic'
// with respect to navigation.
//-----------------------------------------------------------------------------

// Called when an object is added to the scene.
function WalkaboutDynamic::onAdd(%this, %obj)
{
   // Simple - update all NavMeshes with this object's world box!
   WalkaboutUpdateAll(%obj);
   Parent::onAdd(%this, %obj);
}

// Called just before an object is removed from the scene.
function WalkaboutDynamic::onRemove(%this, %obj)
{
   // This time, notify the meshes that we are about to remove the object.
   WalkaboutUpdateAll(%obj, true);
   Parent::onRemove(%this, %obj);
}

//-----------------------------------------------------------------------------
// Walkabout helper functions
//-----------------------------------------------------------------------------

