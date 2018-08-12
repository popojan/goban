#ifndef DDG_COMPLEX_H
#define DDG_COMPLEX_H

#include <iosfwd>

namespace DDG
{
   class MyComplex
   {
      public:
         MyComplex( double a=0., double b=0. );
         // constructs number a+bi

         void operator+=( const MyComplex& z );
         // add z

         void operator-=( const MyComplex& z );
         // subtract z

         void operator*=( const MyComplex& z );
         // MyComplex multiply by z

         void operator*=( double r );
         // scalar multiply by r

         void operator/=( double r );
         // scalar divide by r

         void operator/=( const MyComplex& z );
         // complex divide by r

         MyComplex operator-( void ) const;
         // returns the additive inverse

         MyComplex conj( void ) const;
         // returns MyComplex conjugate

         MyComplex inv( void ) const;
         // returns inverse

         double arg( void ) const;
         // returns argument

         double norm( void ) const;
         // returns norm

         double norm2( void ) const;
         // returns norm squared

         MyComplex unit( void ) const;
         // returns complex number with unit norm and same modulus

         MyComplex exponential( void ) const;
         // complex exponentiation

         double re;
         // real part

         double im;
         // imaginary part
   };

   MyComplex operator+( const MyComplex& z1, const MyComplex& z2 );
   // binary addition

   MyComplex operator-( const MyComplex& z1, const MyComplex& z2 );
   // binary subtraction
   
   MyComplex operator*( const MyComplex& z1, const MyComplex& z2 );
   // binary MyComplex multiplication

   MyComplex operator*( const MyComplex& z, double r );
   // right scalar multiplication

   MyComplex operator*( double r, const MyComplex& z );
   // left scalar multiplication
   
   MyComplex operator/( const MyComplex& z, double r );
   // scalar division

   MyComplex operator/( const MyComplex& z1, const MyComplex& z2 );
   // complex division

   double dot( const MyComplex& z1, const MyComplex& z2 );
   // inner product

   double cross( const MyComplex& z1, const MyComplex& z2 );
   // cross product

   std::ostream& operator<<( std::ostream& os, const MyComplex& o );
   // prints components
}

#endif
