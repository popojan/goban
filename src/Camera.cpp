#include <cmath>
#include <ctime>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

#include "Camera.h"

DDG::Quaternion& rst(DDG::Quaternion& q) {
	return q;
	/*	q.im().z = 0.0;
	q.normalize();
	return q;*/
}

static DDG::Quaternion toQuaternion(double pitch, double roll, double yaw)
{
	
	double t0 = std::cos(yaw * 0.5);
	double t1 = std::sin(yaw * 0.5);
	double t2 = std::cos(roll * 0.5);
	double t3 = std::sin(roll * 0.5);
	double t4 = std::cos(pitch * 0.5);
	double t5 = std::sin(pitch * 0.5);

	DDG::Quaternion q(
	t0 * t2 * t4 + t1 * t3 * t5,
	t0 * t3 * t4 - t1 * t2 * t5,
	t0 * t2 * t5 + t1 * t3 * t4,
	t1 * t2 * t4 - t0 * t3 * t5);
	return q;
}

static void toEulerianAngle(const DDG::Quaternion& q, double& roll, double& pitch, double& yaw)
{
	double ysqr = q.im().y * q.im().y;

	// roll (x-axis rotation)
	double t0 = +2.0 * (q.re() * q.im().x + q.im().y * q.im().z);
	double t1 = +1.0 - 2.0 * (q.im().x * q.im().x + ysqr);
	roll = std::atan2(t0, t1);

	// pitch (y-axis rotation)
	double t2 = +2.0 * (q.re() * q.im().y - q.im().z * q.im().x);
	t2 = ((t2 > 1.0) ? 1.0 : t2);
	t2 = ((t2 < -1.0) ? -1.0 : t2);
	pitch = std::asin(t2);

	// yaw (z-axis rotation)
	double t3 = +2.0 * (q.re() * q.im().z + q.im().x * q.im().y);
	double t4 = +1.0 - 2.0 * (ysqr + q.im().z * q.im().z);
	yaw = std::atan2(t3, t4);
}

namespace DDG
{ 
   Camera :: Camera(float x, float y, float z, float w)
   :   pClick( 1. ),
        pDrag( 1. ),
        pLast( 1. ),
        rLast( x, y, z, w),
     momentum( 1. ),
         zoom( 1. )
   { }

   Quaternion Camera :: clickToSphere( float x, float y, float w, float h )
   {

      float alphaW = 2.f * x / w - 1.f;
      float alphaH = 2.f * y / h - 1.f;

	  Quaternion p(0.0, -alphaW, alphaH, 0.0);

	  if( p.norm2() > 1. )
      {
		  p.normalize();
		  p.im().z = 0.;
      }
      else
      {
         p.im().z = sqrt( 1. - p.norm2() );
      }
	  return p;
   }

   Quaternion Camera :: currentRotation( void ) const
   {
	   auto ret = pDrag * pClick.conj() * rLast;
	   /*if (ret.im().norm2() > 0.0) {
		   double X, Y, Z;
		   toEulerianAngle(ret, X, Y, Z);
		   ret = toQuaternion(0.0, -Y, 0.0) * ret;// *ret;
	   }*/
	   return ret;
      /*if(pDrag.re() == 1.0) return rLast;
      Quaternion click = pClick;
        Quaternion drag = pDrag;
      Quaternion y1 = Quaternion(0, drag[1], 0, drag[3]);
      Quaternion y2 = Quaternion(0, click[1], 0, click[3]);
      Quaternion x1 = Quaternion(0, 0, drag[2], drag[3]);
      Quaternion x2 = Quaternion(0, 0, click[2], click[3]);
      y1.normalize();
      y2.normalize();
      x1.normalize();
      x2.normalize();
      Quaternion r1 = y1*y2.conj();
      Quaternion r2 = x1*x2.conj();
      return r2 * r1 * rLast;*/
   }

   glm::mat4 Camera :: getView(bool reverse) const {
	   Quaternion r = reverse ? rLast.conj() : rLast;

	   float w = (float)r[0];
      float x = (float)r[1];
      float y = (float)r[2];
	  float z = (float)r[3];

	  return /*glm::rotate(glm::mat4x4(), (float)(-Z), glm::vec3(0.0, 0.0, 1.0)) */ glm::mat4x4(
         1.f-2.f*y*y-2.f*z*z, 2.f*x*y+2.f*w*z, 2.f*x*z-2.f*w*y, 0.f,
         2.f*x*y-2.f*w*z, 1.f-2.f*x*x-2.f*z*z, 2.f*y*z+2.f*w*x, 0.f,
         2.f*x*z+2.f*w*y, 2.f*y*z-2.f*w*x, 1.f-2.f*x*x-2.f*y*y, 0.f,
         0.f, 0.f, 0.f, 1.f
		 );

    }
   glm::mat4 Camera :: setView( ) const
   {
      Quaternion r = currentRotation().conj(); //( pDrag * pClick.conj() ) * rLast;

	  float w = (float)r[0];
      float x = (float)r[1];
      float y = (float)r[2];
	  float z = (float)r[3];

	  return /*glm::rotate(glm::mat4x4(), (float)(-Z), glm::vec3(0.0, 0.0, 1.0)) * */glm::mat4x4(
         1.f-2.f*y*y-2.f*z*z, 2.f*x*y+2.f*w*z, 2.f*x*z-2.f*w*y, 0.f,
         2.f*x*y-2.f*w*z, 1.f-2.f*x*x-2.f*z*z, 2.f*y*z+2.f*w*x, 0.f,
         2.f*x*z+2.f*w*y, 2.f*y*z-2.f*w*x, 1.f-2.f*x*x-2.f*y*y, 0.f,
         0.f, 0.f, 0.f, 1.f
      );
   }

   void Camera :: mouse( int state, float x, float y, float w, float h )
   {
      if( state == 1 )
      {
		  double a = std::tan(3.1415 / 4 );
		  double b = (y - h/2)/ (x -w/2);
		  double c = std::tan(3.1415 / 2 - 3.1415 / 4);
		  if (b < a && b > -a) {
			  axis = X_AXIS;
			  pClick = pDrag = DDG::Quaternion(1.0);
				pLast = DDG::Quaternion(x, 0.0, 0.0, 0.0); // clickToSphere(x, h, w, h);
		  }
		  else if (b > c || b < -c) {
			  axis = Y_AXIS;
			  pClick = pDrag = pLast = clickToSphere(w / 2, y, w, h);
		  }
		  else {
			  pClick = pDrag = pLast = clickToSphere(x, y, w, h);
		  }
			  
         momentum = 1.;
      }
      if( state == 0 )
      {
         double timeSinceDrag = ( clock() - tLast ) / (double) CLOCKS_PER_SEC;

         if( timeSinceDrag < .1 )
         {
            momentum = pDrag * pLast.conj();
            momentum = ( .03 * momentum + .97 ).unit();
         }
         else
         {
            momentum = 1.;
         }

		 rLast = currentRotation(); //pDrag * pClick.conj() * rLast;
         pClick = pDrag = 1.;
      }
   }

   void Camera :: motion( float x, float y, float w, float h )
   {
      tLast = clock();
      //pLast = pDrag;
	  if (axis == Y_AXIS) {
		  pDrag = clickToSphere(w / 2, y, w, h);
	  }
	  else if (axis == X_AXIS)	{
		  pDrag = (rLast*toQuaternion(3 * (x - pLast.re()) / w, 0, 0)*rLast.conj()).conj();//
	  }
	  else {
		  pDrag = clickToSphere(x, y, w, h);
	  }
   }

   void Camera :: idle( void )
   {
      // get time since last idle event
      static int t0 = clock();
      int t1 = clock();
      double dt = (t1-t0) / (double) CLOCKS_PER_SEC;

      rLast = momentum * rLast;
      momentum = ( (1.-.5*dt) * momentum + .5*dt ).unit();

      zoom += vZoom*dt;
      vZoom *= max( 0., 1.-5.*dt );

      t0 = t1;
   }

   void Camera :: zoomIn( void )
   {
      vZoom -= 0.5;
   }

   void Camera :: zoomOut( void )
   {
      vZoom += 0.5;
   }
}

