#include "Complex.h"
#include <iostream>
#include <cmath>

using namespace std;

namespace DDG
{
   MyComplex::MyComplex( double a, double b )
   // constructs number a+bi
   : re( a ), im( b )
   {}

   void MyComplex::operator+=( const MyComplex& z )
   // add z
   {
      re += z.re;
      im += z.im;
   }

   void MyComplex::operator-=( const MyComplex& z )
   // subtract z
   {
      re -= z.re;
      im -= z.im;
   }

   void MyComplex::operator*=( const MyComplex& z )
   // MyComplex multiply by z
   {
      double a = re;
      double b = im;
      double c = z.re;
      double d = z.im;

      re = a*c-b*d;
      im = a*d+b*c;
   }

   void MyComplex::operator*=( double r )
   // scalar multiply by r
   {
      re *= r;
      im *= r;
   }

   void MyComplex::operator/=( double r )
   // scalar divide by r
   {
      re /= r;
      im /= r;
   }

   void MyComplex::operator/=( const MyComplex& z )
   // scalar divide by r
   {
      *this *= z.inv();
   }

   MyComplex MyComplex::operator-( void ) const
   {
      return MyComplex( -re, -im );
   }

   MyComplex MyComplex::conj( void ) const
   // returns MyComplex conjugate
   {
      return MyComplex( re, -im );
   }

   MyComplex MyComplex::inv( void ) const
   // returns inverse
   {
      return this->conj() / this->norm2();
   }

   double MyComplex::arg( void ) const
   // returns argument
   {
      return atan2( im, re );
   }

   double MyComplex::norm( void ) const
   // returns norm
   {
      return sqrt( re*re + im*im );
   }

   double MyComplex::norm2( void ) const
   // returns norm squared
   {
      return re*re + im*im;
   }

   MyComplex MyComplex::unit( void ) const
   // returns complex number with unit norm and same modulus
   {
      return *this / this->norm();
   }

   MyComplex MyComplex::exponential( void ) const
   // complex exponentiation
   {
      return exp( re ) * MyComplex( cos( im ), sin( im ));
   }

   MyComplex operator+( const MyComplex& z1, const MyComplex& z2 )
   // binary addition
   {
      MyComplex z = z1;
      z += z2;
      return z;
   }

   MyComplex operator-( const MyComplex& z1, const MyComplex& z2 )
   // binary subtraction
   {
      MyComplex z = z1;
      z -= z2;
      return z;
   }

   MyComplex operator*( const MyComplex& z1, const MyComplex& z2 )
   // binary MyComplex multiplication
   {
      MyComplex z = z1;
      z *= z2;
      return z;
   }

   MyComplex operator*( const MyComplex& z, double r )
   // right scalar multiplication
   {
      MyComplex zr = z;
      zr *= r;
      return zr;
   }

   MyComplex operator*( double r, const MyComplex& z )
   // left scalar multiplication
   {
      return z*r;
   }

   MyComplex operator/( const MyComplex& z, double r )
   // scalar division
   {
      MyComplex zr = z;
      zr /= r;
      return zr;
   }

   MyComplex operator/( const MyComplex& z1, const MyComplex& z2 )
   // complex division
   {
      MyComplex z = z1;
      z /= z2;
      return z;
   }

   double dot( const MyComplex& z1, const MyComplex& z2 )
   {
      return z1.re*z2.re + z1.im*z2.im;
   }

   double cross( const MyComplex& z1, const MyComplex& z2 )
   {
      return z1.re*z2.im - z1.im*z2.re;
   }

   std::ostream& operator<<( std::ostream& os, const MyComplex& z )
   // prints components
   {
      if( z.im > 0 )
      {
         os << z.re << " + " << z.im << "i";
      }
      else if( z.im < 0 )
      {
         os << z.re << " - " << -z.im << "i";
      }
      else
      {
         os << z.re;
      }

      return os;
   }
}

