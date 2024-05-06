// -----------------------------------------------------------------------------
// libDDG -- Quaternion.h
// -----------------------------------------------------------------------------
//
// Quaternion represents an element of the quaternions, along with all the usual
// vectors space operations (addition, multiplication by scalars, etc.).  The
// Hamilton product is expressed using the * operator:
//
//    Quaternion p, q, r;
//    r = q * p;
//
// and conjugation is expressed using the method Quaternion::conj():
//
//    Quaternion q;
//    double normQSquared = -q.conj()*q;
//
// Individual components can be accessed in several ways: the real and imaginary
// parts can be accessed using the methods Quaternion::re() and Quaternion::im():
//
//   Quaternion q;
//   double a = q.re();
//   Vector b = q.im();
//
// or by index:
//
//   Quaternion q;
//   double a  = q[0];
//   double bi = q[1];
//   double bj = q[2];
//   double bk = q[3];
//

#ifndef DDG_QUATERNION_H
#define DDG_QUATERNION_H

#include "Vector.h"
#include <ostream>

namespace DDG
{
   class Quaternion
   {
      public:
         Quaternion( );
         // initializes all components to zero

         Quaternion( const Quaternion& q );
         // initializes from existing quaternion

         Quaternion( double s, double vi, double vj, double vk );
         // initializes with specified real (s) and imaginary (v) components

         Quaternion( double s, const Vector& v );
         // initializes with specified real (s) and imaginary (v) components

         explicit Quaternion( double s );
         // initializes purely real quaternion with specified real (s) component (imaginary part is zero)

         Quaternion& operator=( double s );
         // assigns a purely real quaternion with real value s

         Quaternion& operator=( const Vector& v );
         // assigns a purely real quaternion with imaginary value v

         double& operator[]( int index );
         // returns reference to the specified component (0-based indexing: r, i, j, k)

         const double& operator[]( int index ) const;
         // returns const reference to the specified component (0-based indexing: r, i, j, k)

         void toMatrix( double Q[4][4] ) const;
         // builds 4x4 matrix Q representing (left) quaternion multiplication

         double& re( );
         // returns reference to double part

         [[nodiscard]] const double& re( ) const;
         // returns const reference to double part

         Vector& im( );
         // returns reference to imaginary part

         [[nodiscard]] const Vector& im( ) const;
         // returns const reference to imaginary part

         Quaternion operator+( const Quaternion& q ) const;
         // addition

         Quaternion operator-( const Quaternion& q ) const;
         // subtraction

         Quaternion operator-( ) const;
         // negation

         Quaternion operator*( double c ) const;
         // right scalar multiplication

         Quaternion operator/( double c ) const;
         // scalar division

         void operator+=( const Quaternion& q );
         // addition / assignment

         void operator+=( double c );
         // addition / assignment of pure real

         void operator-=( const Quaternion& q );
         // subtraction / assignment

         void operator-=( double c );
         // subtraction / assignment of pure real

         void operator*=( double c );
         // scalar multiplication / assignment

         void operator/=( double c );
         // scalar division / assignment

         Quaternion operator*( const Quaternion& q ) const;
         // Hamilton product

         void operator*=( const Quaternion& q );
         // Hamilton product / assignment

         Quaternion conj( void ) const;
         // conjugation

         Quaternion inv( void ) const;
         // inverse
         
         double norm( void ) const;
         // returns Euclidean length

         double norm2( void ) const;
         // returns Euclidean length squared

         Quaternion unit( void ) const;
         // returns unit quaternion

         void normalize( void );
         // divides by Euclidean length
   
      protected:
         double s;
         // scalar (double) part

         Vector v;
         // vector (imaginary) part
   };
   
   Quaternion operator*( double c, const Quaternion& q );
   // left scalar multiplication
   
   std::ostream& operator<<( std::ostream& os, const Quaternion& q );
   // prints components
}

#endif

