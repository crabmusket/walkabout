//-----------------------------------------------------------------------------
// Copyright Daniel Buckmaster 2012
//-----------------------------------------------------------------------------

#ifndef _GUIFLEXIBLEARRAYCTRL_H_
#define _GUIFLEXIBLEARRAYCTRL_H_

#include "gui/core/guiControl.h"

class GuiFlexibleArrayControl : public GuiControl
{
   typedef GuiControl Parent;

public:

   GuiFlexibleArrayControl();
   virtual ~GuiFlexibleArrayControl();

   DECLARE_CONOBJECT(GuiFlexibleArrayControl);
   DECLARE_CATEGORY( "Gui Containers" );

   // ConsoleObject
   static void initPersistFields();

   // SimObject
   void inspectPostApply();

   // SimSet
   void addObject(SimObject *obj);

   // GuiControl
   bool resize(const Point2I &newPosition, const Point2I &newExtent);
   void childResized(GuiControl *child);

   // GuiFlexibleArrayCtrl
   void refresh();

protected:

   S32 mRows;
   S32 mRowSpacing;
   S32 mColSpacing;
   bool mResizing;
   bool mFrozen;

   RectSpacingI mPadding;
};

#endif // _GUIFLEXIBLEARRAYCTRL_H_