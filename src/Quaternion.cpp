#include <cmath>
#include <iostream>
using namespace std;

#include "Quaternion.h"

namespace DDG
{
   // CONSTRUCTORS ----------------------------------------------------------
   
   Quaternion :: Quaternion( )
   // initializes all components to zero
   : s( 0. ),
     v( 0., 0., 0. )
   {}

    // initializes from existing quaternion
   Quaternion :: Quaternion( const Quaternion& q ) = default;

   Quaternion :: Quaternion( double s_, double vi, double vj, double vk )
   // initializes with specified double (s) and imaginary (v) components
   : s( s_ ),
     v( vi, vj, vk )
   {}
   
   Quaternion :: Quaternion( double s_, const Vector& v_ )
   // initializes with specified double(s) and imaginary (v) components
   : s( s_ ),
     v( v_ )
   {}
   
   Quaternion :: Quaternion( double s_ )
   // initializes purely real quaternion with specified real (s) component (imaginary part is zero)
   : s( s_ ),
     v( 0., 0., 0. )
   {}
   
   // ASSIGNMENT OPERATORS --------------------------------------------------
   
   Quaternion& Quaternion :: operator=( double _s )
   // assigns a purely real quaternion with real value s
   {
      s = _s;
      v = Vector( 0., 0., 0. );
   
      return *this;
   }
   
   Quaternion& Quaternion :: operator=( const Vector& _v )
   // assigns a purely real quaternion with imaginary value v
   {
      s = 0.;
      v = _v;
   
      return *this;
   }
   
   
   // ACCESSORS -------------------------------------------------------------
   
   double& Quaternion::operator[]( int index )
   // returns reference to the specified component (0-based indexing: double, i, j, k)
   {
      return ( &s )[ index ];
   }
   
   const double& Quaternion::operator[]( int index ) const
   // returns const reference to the specified component (0-based indexing: double, i, j, k)
   {
      return ( &s )[ index ];
   }

   void Quaternion::toMatrix( double Q[4][4] ) const
   // returns 4x4 matrix representation
   {
      Q[0][0] =   s; Q[0][1] = -v.x; Q[0][2] = -v.y; Q[0][3] = -v.z;
      Q[1][0] = v.x; Q[1][1] =    s; Q[1][2] = -v.z; Q[1][3] =  v.y;
      Q[2][0] = v.y; Q[2][1] =  v.z; Q[2][2] =    s; Q[2][3] = -v.x;
      Q[3][0] = v.z; Q[3][1] = -v.y; Q[3][2] =  v.x; Q[3][3] =    s;
   }
   
   double& Quaternion::re( )
   // returns reference to double part
   {
      return s;
   }
   
   const double& Quaternion::re( ) const
   // returns const reference to double part
   {
      return s;
   }
   
   Vector& Quaternion::im( )
   // returns reference to imaginary part
   {
      return v;
   }
   
   const Vector& Quaternion::im( ) const
   // returns const reference to imaginary part
   {
      return v;
   }
   
   
   // VECTOR SPACE OPERATIONS -----------------------------------------------
   
   Quaternion Quaternion::operator+( const Quaternion& q ) const
   // addition
   {
      return { s+q.s, v+q.v };
   }
   
   Quaternion Quaternion::operator-( const Quaternion& q ) const
   // subtraction
   {
      return { s-q.s, v-q.v };
   }
   
   Quaternion Quaternion::operator-( ) const
   // negation
   {
      return { -s, -v };
   }
   
   Quaternion Quaternion::operator*( double c ) const
   // scalar multiplication
   {
      return { s*c, v*c };
   }
   
   Quaternion operator*( double c, const Quaternion& q )
   // scalar multiplication
   {
      return q*c;
   }
   
   Quaternion Quaternion::operator/( double c ) const
   // scalar division
   {
      return { s/c, v/c };
   }
   
   void Quaternion::operator+=( const Quaternion& q )
   // addition / assignment
   {
      s += q.s;
      v += q.v;
   }
   
   void Quaternion::operator+=( double c )
   // addition / assignment of pure real
   {
      s += c;
   }
   
   void Quaternion::operator-=( const Quaternion& q )
   // subtraction / assignment
   {
      s -= q.s;
      v -= q.v;
   }
   
   void Quaternion::operator-=( double c )
   // subtraction / assignment of pure real
   {
      s -= c;
   }
   
   void Quaternion::operator*=( double c )
   // scalar multiplication / assignment
   {
      s *= c;
      v *= c;
   }
   
   void Quaternion::operator/=( double c )
   // scalar division / assignment
   {
      s /= c;
      v /= c;
   }
   
   
   // ALGEBRAIC OPERATIONS --------------------------------------------------
   
   Quaternion Quaternion::operator*( const Quaternion& q ) const
   // Hamilton product
   {
      const double& s1( s );
      const double& s2( q.s );
      const Vector& v1( v );
      const Vector& v2( q.v );
   
      return { s1*s2 - dot(v1,v2), s1*v2 + s2*v1 + cross(v1,v2) };
   }
   
   void Quaternion::operator*=( const Quaternion& q )
   // Hamilton product / assignment
   {
      *this = ( *this * q );
   }
   
   Quaternion Quaternion::conj( ) const
   // conjugation
   {
      return { s, -v };
   }
   
   Quaternion Quaternion::inv( ) const
   {
      return ( this->conj() ) / this->norm2();
   }
   
   
   // NORMS -----------------------------------------------------------------
   
   double Quaternion::norm( ) const
   // returns Euclidean length
   {
      return sqrt( s*s + v.x*v.x + v.y*v.y + v.z*v.z );
   }
   
   double Quaternion::norm2( ) const
   // returns Euclidean length squared
   {
      return s*s + dot(v,v);
   }
   
   Quaternion Quaternion::unit( ) const
   // returns unit quaternion
   {
      return *this / norm();
   }
   
   void Quaternion::normalize( )
   // divides by Euclidean length
   {
      *this /= norm();
   }

   // GEOMETRIC OPERATIONS --------------------------------------------------
   
   Quaternion slerp( const Quaternion& q0, const Quaternion& q1, double t )
   // spherical-linear interpolation
   {
      // interpolate length
      double m0 = q0.norm();
      double m1 = q1.norm();
      double m = (1-t)*m0 + t*m1;
   
      // interpolate direction
      Quaternion p0 = q0 / m0;
      Quaternion p1 = q1 / m1;
      double theta = acos(( p0.conj()*p1 ).re() );
      Quaternion p = ( sin((1-t)*theta)*p0 + sin(t*theta)*p1 )/sin(theta);
   
      return m*p;
   }
   
   
   // I/O -------------------------------------------------------------------------
   
   std::ostream& operator<<( std::ostream& os, const Quaternion& q )
   // prints components
   {
      os << "( " << q.re() << ", " << q.im() << " )";
   
      return os;
   }
}

