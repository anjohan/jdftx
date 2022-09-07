/*-------------------------------------------------------------------
Copyright 2011 Ravishankar Sundararaman

This file is part of JDFTx.

JDFTx is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

JDFTx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with JDFTx.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------*/

#ifndef JDFTX_CORE_TENSOR3_H
#define JDFTX_CORE_TENSOR3_H

//! @addtogroup DataStructures
//! @{

//! @file tensor3.h Symmetric traceless tensor with CPU and GPU operators

#include <core/matrix3.h>

#define LOOP5(code) { for(int k=0; k<5; k++) { code } }

//! Symmetric traceless rank-2 tensor in 3D
template<typename scalar=double> class tensor3
{
	scalar v[5];
public:
	//Accessors
	__hostanddev__ scalar& operator[](int k) { return v[k]; }
	__hostanddev__ const scalar& operator[](int k) const { return v[k]; }
	__hostanddev__ scalar& xy() { return v[0]; }
	__hostanddev__ scalar& yz() { return v[1]; }
	__hostanddev__ scalar& zx() { return v[2]; }
	__hostanddev__ scalar& xxr() { return v[3]; } //!< xxr = x^2 - r^2/3
	__hostanddev__ scalar& yyr() { return v[4]; } //!< yyr = y^2-r^2/3
	__hostanddev__ const scalar& xy() const { return v[0]; }
	__hostanddev__ const scalar& yz() const { return v[1]; }
	__hostanddev__ const scalar& zx() const { return v[2]; }
	__hostanddev__ const scalar& xxr() const { return v[3]; }
	__hostanddev__ const scalar& yyr() const { return v[4]; }

	explicit __hostanddev__ tensor3(scalar a=0, scalar b=0, scalar c=0, scalar d=0, scalar e=0) { v[0]=a; v[1]=b; v[2]=c; v[3]=d; v[4]=e; }
	tensor3(std::vector<scalar> a) { LOOP5( v[k]=a[k]; ) }
	
	//! Extract from full matrix
	explicit __hostanddev__ tensor3(const matrix3<scalar>& m)
	{	xy() = 0.5*(m(0, 1) + m(1, 0));
		yz() = 0.5*(m(1, 2) + m(2, 1));
		zx() = 0.5*(m(2, 0) + m(0, 2));
		scalar traceTerm = (1./3) * trace(m);
		xxr() = m(0, 0) - traceTerm;
		yyr() = m(1, 1) - traceTerm;
	}

	//! Convert to full matrix
	explicit __hostanddev__ operator matrix3<scalar>() const
	{	matrix3<scalar> m;
		m(0, 1) = (m(1, 0) = xy());
		m(1, 2) = (m(2, 1) = yz());
		m(2, 0) = (m(0, 2) = zx());
		m(0, 0) = xxr();
		m(1, 1) = yyr();
		m(2, 2) = -(xxr() + yyr());
		return m;
	}
	
	//Arithmetic:
	__hostanddev__ tensor3 operator+(const tensor3 &a) const { return tensor3(v[0]+a[0], v[1]+a[1], v[2]+a[2], v[3]+a[3], v[4]+a[4]); }
	__hostanddev__ tensor3 operator+=(const tensor3 &a) { LOOP5( v[k]+=a[k]; ) return *this; }
	__hostanddev__ tensor3 operator-(const tensor3 &a) const { return tensor3(v[0]-a[0], v[1]-a[1], v[2]-a[2], v[3]-a[3], v[4]-a[4]); }
	__hostanddev__ tensor3 operator-=(const tensor3 &a) { LOOP5( v[k]-=a[k]; ) return *this; }
};

template<typename scalar> __hostanddev__  tensor3<scalar> operator*(scalar s, const tensor3<scalar> &a) { tensor3<scalar> v; LOOP5(v[k]=a[k]*s;) return v; }
template<typename scalar> __hostanddev__  tensor3<scalar> operator*(const tensor3<scalar> &a, scalar s) { tensor3<scalar> v; LOOP5(v[k]=a[k]*s;) return v; }

//! Load tensor from a constant tensor field
template<typename scalar> __hostanddev__ tensor3<scalar> loadTensor(const tensor3<const scalar*>& tArr, int i)
{	return tensor3<scalar>( tArr[0][i], tArr[1][i], tArr[2][i], tArr[3][i], tArr[4][i] );
}
//! Load tensor from a tensor field
template<typename scalar> __hostanddev__ tensor3<scalar> loadTensor(const tensor3<scalar*>& tArr, int i)
{	return tensor3<scalar>( tArr[0][i], tArr[1][i], tArr[2][i], tArr[3][i], tArr[4][i] );
}
//! Store tensor to a tensor field
template<typename scalar> __hostanddev__ void storeTensor(const tensor3<scalar>& t, tensor3<scalar*>& tArr, int i)
{	LOOP5( tArr[k][i] = t[k]; )
}
//! Accumulate tensor onto a tensor field
template<typename scalar> __hostanddev__ void accumTensor(const tensor3<scalar>& t, tensor3<scalar*>& tArr, int i)
{	LOOP5( tArr[k][i] += t[k]; )
}

//! @}
#undef LOOP5
#endif // JDFTX_CORE_TENSOR3_H
