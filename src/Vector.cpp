#include "Vector.h"
#include <cmath>

namespace DDG
{
   Vector :: Vector( )
   : x( 0. ),
     y( 0. ),
     z( 0. )
   {}
   
   Vector :: Vector( double x0,
                     double y0,
                     double z0 )
   : x( x0 ),
     y( y0 ),
     z( z0 )
   {}
   
   Vector :: Vector( const Vector& v ) = default;

   double& Vector :: operator[]( const int& index )
   {
      return ( &x )[ index ];
   }
   
   const double& Vector :: operator[]( const int& index ) const
   {
      return ( &x )[ index ];
   }
   
   Vector Vector :: operator+( const Vector& v ) const
   {
      return { x + v.x,
               y + v.y,
               z + v.z };
   }
   
   Vector Vector :: operator-( const Vector& v ) const
   {
      return { x - v.x,
               y - v.y,
               z - v.z };
   }
   
   Vector Vector :: operator-( ) const
   {
      return { -x,
               -y,
               -z };
   }
   
   Vector Vector :: operator*( const double& c ) const
   {
      return { x*c,
               y*c,
               z*c };
   }
   
   Vector operator*( const double& c, const Vector& v )
   {
      return v*c;
   }
   
   Vector Vector :: operator/( const double& c ) const
   {
      return (*this) * ( 1./c );
   }
   
   void Vector :: operator+=( const Vector& v )
   {
      x += v.x;
      y += v.y;
      z += v.z;
   }
   
   void Vector :: operator-=( const Vector& v )
   {
      x -= v.x;
      y -= v.y;
      z -= v.z;
   }
   
   void Vector :: operator*=( const double& c )
   {
      x *= c;
      y *= c;
      z *= c;
   }
   
   void Vector :: operator/=( const double& c )
   {
      (*this) *= ( 1./c );
   }
   
   double Vector :: norm( ) const
   {
      return std::sqrt( norm2());
   }
   
   double Vector :: norm2( ) const
   {
      return dot( *this, *this );
   }
   
   void Vector :: normalize( )
   {
      (*this) /= norm();
   }
   
   Vector Vector :: unit( ) const
   {
      return (*this) / norm();
   }
   
   Vector Vector :: abs( ) const
   {
      return { std::abs( x ),
               std::abs( y ),
               std::abs( z ) };
   }

   double dot( const Vector& u, const Vector& v )
   {
      return u.x*v.x +
             u.y*v.y +
             u.z*v.z ;
   }
   
   Vector cross( const Vector& u, const Vector& v )
   {
      return { u.y*v.z - u.z*v.y,
               u.z*v.x - u.x*v.z,
               u.x*v.y - u.y*v.x };
   }
   
   std::ostream& operator << (std::ostream& os, const Vector& o)
   {
      os << "[ "
         << o.x << " "
         << o.y << " "
         << o.z
         << " ]";
   
      return os;
   }
}
