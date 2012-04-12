#define DEBUG_ROOT_COCOA

#define NDEBUG

#ifdef DEBUG_ROOT_COCOA
#include <algorithm>
#endif

#include <stdexcept>
#include <cassert>

#include <Cocoa/Cocoa.h>

#include "CocoaPrivate.h"
#include "QuartzWindow.h"
#include "CocoaUtils.h"

namespace ROOT {
namespace MacOSX {
namespace Details {

//______________________________________________________________________________
CocoaPrivate::CocoaPrivate()
               : fCurrentDrawableID(GetRootWindowID() + 1)//Any real window has id > rootID.
                                                        //0 is also used by some X11 functions as None.
{
   //Init NSApplication, if it was not done yet.
   Util::AutoreleasePool pool;
   [NSApplication sharedApplication];
}

//______________________________________________________________________________
CocoaPrivate::~CocoaPrivate()
{
}

//______________________________________________________________________________
int CocoaPrivate::GetRootWindowID()const
{
   //First I had root ID == 0, but this is None in X11 and
   //it can be used by ROOT, for example, I had trouble with
   //gClient able to found TGWindow for None - crash!
   return 1;
}

//______________________________________________________________________________
bool CocoaPrivate::IsRootWindow(int wid)const
{
   return wid == GetRootWindowID();
}

//______________________________________________________________________________
unsigned CocoaPrivate::RegisterDrawable(NSObject *nsObj)
{
   //Return integer identifier for a new "drawable" (like in X11)
   unsigned newID = fCurrentDrawableID;

   if (fFreeDrawableIDs.size()) {
      newID = fFreeDrawableIDs.back();
      fFreeDrawableIDs.pop_back();
   } else
      fCurrentDrawableID++;

   assert(fDrawables.find(newID) == fDrawables.end() && "RegisterDrawable, id for new drawable is still in use");

   fDrawables[newID] = nsObj;

   return newID;
}

//______________________________________________________________________________
NSObject<X11Drawable> *CocoaPrivate::GetDrawable(unsigned drawableID)const
{
   auto drawableIter = fDrawables.find(drawableID);
#ifdef DEBUG_ROOT_COCOA
   if (drawableIter == fDrawables.end()) {
      NSLog(@"Fatal error: requested non-existing drawable %u", drawableID);
      //We do not care about efficiency, ROOT's gonna die on assert :)
      auto deletedDrawable = std::find(fFreeDrawableIDs.begin(), fFreeDrawableIDs.end(), drawableID);
      if (deletedDrawable != fFreeDrawableIDs.end()) {
         NSLog(@"This drawable was deleted already");
      } else {
         NSLog(@"This drawable not found among allocated/deleted drawables");
      }
   }
#endif
   assert(drawableIter != fDrawables.end() && "GetDrawable, non-existing drawable requested");
   return drawableIter->second.Get();
}

//______________________________________________________________________________
void CocoaPrivate::DeleteDrawable(unsigned drawableID)
{
   auto drawableIter = fDrawables.find(drawableID);

   assert(drawableIter != fDrawables.end() && "DeleteDrawable, non existing drawableID");
   
   //Probably, I'll need some additional cleanup here later. Now just delete NSObject and
   //reuse its id.
   NSObject *base = drawableIter->second.Get();
   if ([base isKindOfClass : [QuartzView class]])
      [(QuartzView *)base removeFromSuperview];
   else if ([base isKindOfClass : [QuartzWindow class]]) {
      QuartzWindow *qw = (QuartzWindow *)base;
      [qw.fContentView removeFromSuperview];
      qw.contentView = nil;
   }

   fFreeDrawableIDs.push_back(drawableID);
   fDrawables.erase(drawableIter);//StrongReference should do work here.
}

//______________________________________________________________________________
void CocoaPrivate::ReplaceDrawable(unsigned drawableID, NSObject *nsObj)
{
   auto drawableIter = fDrawables.find(drawableID);
   assert(drawableIter != fDrawables.end() && "ReplaceDrawable, can not replace non existing drawable");
   drawableIter->second.Reset(nsObj);
}

}
}
}