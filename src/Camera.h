// -----------------------------------------------------------------------------
// libDDG -- Camera.h
// -----------------------------------------------------------------------------
//
// Camera is used by Viewer to keep track of the view state; it also
// handles mouse input related to camera manipulation.
//

#ifndef DDG_CAMERA_H
#define DDG_CAMERA_H

#include "Quaternion.h"
#include "glm/mat4x4.hpp"


namespace DDG
{
   class Camera
   {
   public:
         Camera(float x, float y, float z, float w);
         // constructor

         Quaternion clickToSphere( float x, float y, float w, float h );
         // projects a mouse click onto the unit sphere

         glm::mat4 getView( bool reverse = false) const;
         glm::mat4 setView(  ) const;
         // applies the camera transformation to the OpenGL modelview stack

         void mouse( int state, float x, float y, float w, float h );
         // handles mouse clicks

         void motion( float x, float y, float w, float h );
         // handles mouse drags

         void idle( void );
         // handles camera momentum

         void zoomIn( void );
         // moves viewer toward object

         void zoomOut( void );
         // moves viewer away from object

         Quaternion currentRotation( void ) const;
         // returns the rotation corresponding to the current mouse state

         Quaternion pClick;
         // mouse coordinates of current click

         Quaternion pDrag;
         // mouse coordinates of current drag

         Quaternion pLast;
         // mouse coordinates of previous drag

         Quaternion rLast;
         // previous camera rotation

         Quaternion momentum;
         // camera momentum

         int tLast = 0;
         // time of previous drag

         double zoom, vZoom;

		 enum { BOTH_AXES, X_AXIS, Y_AXIS} axis;
         // zoom and zoom velocity
   };
}

#endif

