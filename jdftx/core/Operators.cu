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

#include <core/GpuKernelUtils.h>
#include <core/LoopMacros.h>
#include <core/Operators_internal.h>

__global__
void RealG_kernel(int zBlock, const vector3<int> S, const complex* vFull, complex* vHalf, double scaleFac)
{	COMPUTE_halfGindices
	RealG_calc(i, iG, S, vFull, vHalf, scaleFac);
}
void RealG_gpu(const vector3<int> S, const complex* vFull, complex* vHalf, double scaleFac)
{	GpuLaunchConfigHalf3D glc(RealG_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		RealG_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, vFull, vHalf, scaleFac);
	gpuErrorCheck();
}

__global__
void ImagG_kernel(int zBlock, const vector3<int> S, const complex* vFull, complex* vHalf, double scaleFac)
{	COMPUTE_halfGindices
	ImagG_calc(i, iG, S, vFull, vHalf, scaleFac);
}
void ImagG_gpu(const vector3<int> S, const complex* vFull, complex* vHalf, double scaleFac)
{	GpuLaunchConfigHalf3D glc(ImagG_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		ImagG_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, vFull, vHalf, scaleFac);
	gpuErrorCheck();
}

__global__
void ComplexG_kernel(int zBlock, const vector3<int> S, const complex* vHalf, complex *vFull, double scaleFac)
{	COMPUTE_halfGindices
	ComplexG_calc(i, iG, S, vHalf, vFull, scaleFac);
}
void ComplexG_gpu(const vector3<int> S, const complex* vHalf, complex *vFull, double scaleFac)
{	GpuLaunchConfigHalf3D glc(ComplexG_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		ComplexG_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, vHalf, vFull, scaleFac);
	gpuErrorCheck();
}



__global__
void L_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* v)
{	COMPUTE_halfGindices
	v[i] *= GGT.metric_length_squared(iG);
}
void L_gpu(const vector3<int> S, const matrix3<> GGT, complex* v)
{	GpuLaunchConfigHalf3D glc(L_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		L_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, v);
	gpuErrorCheck();
}

__global__
void Linv_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* v)
{	COMPUTE_halfGindices
	v[i] *= i ? 1.0/GGT.metric_length_squared(iG) : 0.0;
}
void Linv_gpu(const vector3<int> S, const matrix3<> GGT, complex* v)
{	GpuLaunchConfigHalf3D glc(Linv_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		Linv_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, v);
	gpuErrorCheck();
}

__global__
void fullL_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* v)
{	COMPUTE_fullGindices
	v[i] *= GGT.metric_length_squared(iG);
}
void fullL_gpu(const vector3<int> S, const matrix3<> GGT, complex* v)
{	GpuLaunchConfig3D glc(fullL_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		fullL_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, v);
	gpuErrorCheck();
}

__global__
void fullLinv_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* v)
{	COMPUTE_fullGindices
	v[i] *= i ? 1.0/GGT.metric_length_squared(iG) : 0.0;
}
void fullLinv_gpu(const vector3<int> S, const matrix3<> GGT, complex* v)
{	GpuLaunchConfig3D glc(fullLinv_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		fullLinv_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, v);
	gpuErrorCheck();
}

__global__
void Lstress_kernel(int zBlock, vector3<int> S, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	COMPUTE_halfGindices
	double weight = ((iG[2]==0) or (2*iG[2]==S[2])) ? 1 : 2; //weight factor for points in reduced reciprocal space of real scalar fields
	grad_RRT[i] = (weight * real(X[i].conj() * Y[i])) * outer(vector3<>(iG));
}

void Lstress_gpu(vector3<int> S, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	GpuLaunchConfigHalf3D glc(Lstress_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		Lstress_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, X, Y, grad_RRT);
}

__global__
void LinvStress_kernel(int zBlock, vector3<int> S, const matrix3<> GGT, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	COMPUTE_halfGindices
	double weight = ((iG[2]==0) or (2*iG[2]==S[2])) ? 1 : 2; //weight factor for points in reduced reciprocal space of real scalar fields
	double Gsq = GGT.metric_length_squared(iG);
	double GsqInv = Gsq ? 1./Gsq : 0;
	grad_RRT[i] = (weight * real(X[i].conj() * Y[i]) * (-GsqInv*GsqInv)) * outer(vector3<>(iG));
}

void LinvStress_gpu(vector3<int> S, const matrix3<>& GGT, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	GpuLaunchConfigHalf3D glc(LinvStress_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		LinvStress_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, X, Y, grad_RRT);
}


__global__
void D_kernel(int zBlock, const vector3<int> S, const complex* in, complex* out, vector3<> Ge)
{	COMPUTE_halfGindices
	D_calc(i, iG, in, out, Ge);
}
void D_gpu(const vector3<int> S, const complex* in, complex* out, vector3<> Ge)
{	GpuLaunchConfigHalf3D glc(D_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		D_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, in, out, Ge);
	gpuErrorCheck();
}

__global__
void DD_kernel(int zBlock, const vector3<int> S, const complex* in, complex* out, vector3<> Ge1, vector3<> Ge2)
{	COMPUTE_halfGindices
	DD_calc(i, iG, in, out, Ge1, Ge2);
}
void DD_gpu(const vector3<int> S, const complex* in, complex* out, vector3<> Ge1, vector3<> Ge2)
{	GpuLaunchConfigHalf3D glc(DD_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		DD_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, in, out, Ge1, Ge2);
	gpuErrorCheck();
}

template<int l> __global__
void lGradient_kernel(int zBlock, const vector3<int> S, const complex lPhase, const complex* in, array<complex*, 2*l+1> out, const matrix3<> G)
{	COMPUTE_halfGindices
	lGradient_calc<l>(i, iG, IS_NYQUIST, lPhase, in, out, G);
}
template<int l> void lGradient_gpu(const vector3<int>& S, const complex* in, array<complex*, 2*l+1> out, const matrix3<>& G)
{	const complex lPhase = cis(l*0.5*M_PI);
	GpuLaunchConfigHalf3D glc(lGradient_kernel<l>, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		lGradient_kernel<l><<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, lPhase, in, out, G);
	gpuErrorCheck();
}
void lGradient_gpu(const vector3<int>& S, const complex* in, std::vector<complex*> out, int l, const matrix3<>& G)
{	SwitchTemplate_l(l, lGradient_gpu, (S, in, out, G))
}

template<int l> __global__
void lDivergence_kernel(int zBlock, const vector3<int> S, const complex lPhase, const array<const complex*,2*l+1> in, complex* out, const matrix3<> G)
{	COMPUTE_halfGindices
	lDivergence_calc<l>(i, iG, IS_NYQUIST, lPhase, in, out, G);
}
template<int l> void lDivergence_gpu(const vector3<int>& S, array<const complex*,2*l+1> in, complex* out, const matrix3<>& G)
{	const complex lPhase = cis(l*0.5*M_PI);
	GpuLaunchConfigHalf3D glc(lDivergence_kernel<l>, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		lDivergence_kernel<l><<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, lPhase, in, out, G);
	gpuErrorCheck();
}
void lDivergence_gpu(const vector3<int>& S, const std::vector<const complex*>& in, complex* out, int l, const matrix3<>& G)
{	SwitchTemplate_l(l, lDivergence_gpu, (S, in, out, G))
}


template<int l, int m> __global__
void lGradientStress_kernel(int zBlock, const vector3<int> S, const matrix3<> G, const RadialFunctionG w, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT, complex lPhase)
{	COMPUTE_halfGindices
	double weight = real(conj(X[i]) * Y[i] * lPhase) * (IS_NYQUIST
			? 0 //drop nyquist frequency contributions
			: (iG[2]==0 ? 1 : 2) ); //reciprocal space weights
		grad_RRT[i] = weight * lGradientStress_calc<l*(l+1)+m>(iG, G, w);
}
template<int l, int m> void lGradientStress_gpu(const vector3<int>& S, const matrix3<>& G, const RadialFunctionG& w, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	const complex lPhase = cis(l*0.5*M_PI);
	GpuLaunchConfigHalf3D glc(lGradientStress_kernel<l,m>, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		lGradientStress_kernel<l,m><<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, G, w, X, Y, grad_RRT, lPhase);
	gpuErrorCheck();
}
void lGradientStress_gpu(const vector3<int>& S, const matrix3<>& G, const RadialFunctionG& w, const complex* X, const complex* Y, int l, int m, symmetricMatrix3<>* grad_RRT)
{	SwitchTemplate_lm(l, m, lGradientStress_gpu, (S, G, w, X, Y, grad_RRT))
}


__global__
void multiplyBlochPhase_kernel(int zBlock, const vector3<int> S, const vector3<> invS, complex* v, const vector3<> k)
{	COMPUTE_rIndices
	v[i] *= blochPhase_calc(iv, invS, k);
}
void multiplyBlochPhase_gpu(const vector3<int>& S, const vector3<>& invS, complex* v, const vector3<>& k)
{	GpuLaunchConfig3D glc(multiplyBlochPhase_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		 multiplyBlochPhase_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, invS, v, k);
	gpuErrorCheck();
}


__global__
void radialFunction_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT,
	complex* F, const RadialFunctionG f, vector3<> r0)
{	COMPUTE_halfGindices
	F[i] = radialFunction_calc(iG, GGT, f, r0);
}
void radialFunction_gpu(const vector3<int> S, const matrix3<>& GGT,
	complex* F, const RadialFunctionG& f, vector3<> r0)
{	GpuLaunchConfigHalf3D glc(radialFunction_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		radialFunction_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, F, f, r0);
	gpuErrorCheck();
}

__global__
void radialFunctionMultiply_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* in, const RadialFunctionG f)
{	COMPUTE_halfGindices
	in[i] *= f(sqrt(GGT.metric_length_squared(iG)));
}
void radialFunctionMultiply_gpu(const vector3<int> S, const matrix3<>& GGT, complex* in, const RadialFunctionG& f)
{	GpuLaunchConfigHalf3D glc(radialFunctionMultiply_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		radialFunctionMultiply_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, in, f);
	gpuErrorCheck();
}

__global__
void convolveStress_kernel(int zBlock, vector3<int> S, const matrix3<> GGT, const RadialFunctionG w, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	COMPUTE_halfGindices
	double weight = ((iG[2]==0) or (2*iG[2]==S[2])) ? 1 : 2; //weight factor for points in reduced reciprocal space of real scalar fields
	double G = sqrt(GGT.metric_length_squared(iG));
	double minus_wPrime_by_G = G ? (-w.deriv(G)/G) : 0.;
	grad_RRT[i] = (weight * minus_wPrime_by_G * real(X[i].conj() * Y[i])) * outer(vector3<>(iG));
}

void convolveStress_gpu(vector3<int> S, const matrix3<>& GGT, const RadialFunctionG& w, const complex* X, const complex* Y, symmetricMatrix3<>* grad_RRT)
{	GpuLaunchConfigHalf3D glc(convolveStress_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		convolveStress_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, w, X, Y, grad_RRT);
}

__global__
void exp_kernel(int N, double* X, double prefac)
{	int i = kernelIndex1D(); if(i<N) X[i] = exp(prefac*X[i]);
}
void exp_gpu(int N, double* X, double prefac)
{	GpuLaunchConfig1D glc(exp_kernel, N);
	exp_kernel<<<glc.nBlocks,glc.nPerBlock>>>(N, X, prefac);
	gpuErrorCheck();
}

__global__
void log_kernel(int N, double* X, double prefac)
{	int i = kernelIndex1D(); if(i<N) X[i] = log(prefac*X[i]);
}
void log_gpu(int N, double* X, double prefac)
{	GpuLaunchConfig1D glc(log_kernel, N);
	log_kernel<<<glc.nBlocks,glc.nPerBlock>>>(N, X, prefac);
	gpuErrorCheck();
}

__global__
void sqrt_kernel(int N, double* X, double prefac)
{	int i = kernelIndex1D(); if(i<N) X[i] = sqrt(prefac*X[i]);
}
void sqrt_gpu(int N, double* X, double prefac)
{	GpuLaunchConfig1D glc(sqrt_kernel, N);
	sqrt_kernel<<<glc.nBlocks,glc.nPerBlock>>>(N, X, prefac);
	gpuErrorCheck();
}

__global__
void inv_kernel(int N, double* X, double prefac)
{	int i = kernelIndex1D(); if(i<N) X[i] = prefac/X[i];
}
void inv_gpu(int N, double* X, double prefac)
{	GpuLaunchConfig1D glc(inv_kernel, N);
	inv_kernel<<<glc.nBlocks,glc.nPerBlock>>>(N, X, prefac);
	gpuErrorCheck();
}

__global__
void pow_kernel(int N, double* X, double scale, double alpha)
{	int i = kernelIndex1D(); if(i<N) X[i] = pow(scale*X[i],alpha);
}
void pow_gpu(int N, double* X, double scale, double alpha)
{	GpuLaunchConfig1D glc(pow_kernel, N);
	pow_kernel<<<glc.nBlocks,glc.nPerBlock>>>(N, X, scale, alpha);
	gpuErrorCheck();
}

__global__
void gaussConvolve_kernel(int zBlock, const vector3<int> S, const matrix3<> GGT, complex* data, double sigma)
{	COMPUTE_halfGindices
	data[i] *= exp(-0.5*sigma*sigma*GGT.metric_length_squared(iG));
}
void gaussConvolve_gpu(const vector3<int>& S, const matrix3<>& GGT, complex* data, double sigma)
{	GpuLaunchConfig3D glc(gaussConvolve_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		gaussConvolve_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, GGT, data, sigma);
}


__global__
void changeGrid_kernel(int zBlock, const vector3<int> S, const vector3<int> Sin, const vector3<int> Sout, const complex* in, complex* out)
{	COMPUTE_halfGindices
	changeGrid_calc(iG, Sin, Sout, in, out);
}
void changeGrid_gpu(const vector3<int>& S, const vector3<int>& Sin, const vector3<int>& Sout, const complex* in, complex* out)
{	GpuLaunchConfigHalf3D glc(changeGrid_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		changeGrid_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, Sin, Sout, in, out);
	gpuErrorCheck();
}

__global__
void changeGridFull_kernel(int zBlock, const vector3<int> S, const vector3<int> Sin, const vector3<int> Sout, const complex* in, complex* out)
{	COMPUTE_fullGindices
	changeGridFull_calc(iG, Sin, Sout, in, out);
}
void changeGridFull_gpu(const vector3<int>& S, const vector3<int>& Sin, const vector3<int>& Sout, const complex* in, complex* out)
{	GpuLaunchConfig3D glc(changeGridFull_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		changeGridFull_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, Sin, Sout, in, out);
	gpuErrorCheck();
}


__global__
void gradient_kernel(int zBlock, const vector3<int> S, const matrix3<> G, const complex* Xtilde, vector3<complex*> gradTilde)
{	COMPUTE_halfGindices
	gradient_calc(i, iG, IS_NYQUIST, G, Xtilde, gradTilde);
}
void gradient_gpu(const vector3<int> S, const matrix3<> G, const complex* Xtilde, vector3<complex*> gradTilde)
{	GpuLaunchConfigHalf3D glc(gradient_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		gradient_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, G, Xtilde, gradTilde);
	gpuErrorCheck();
}


__global__
void divergence_kernel(int zBlock, const vector3<int> S, const matrix3<> G, vector3<const complex*> Vtilde, complex* divTilde)
{	COMPUTE_halfGindices
	divergence_calc(i, iG, IS_NYQUIST, G, Vtilde, divTilde);
}
void divergence_gpu(const vector3<int> S, const matrix3<> G, vector3<const complex*> Vtilde, complex* divTilde)
{	GpuLaunchConfigHalf3D glc(divergence_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		divergence_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, G, Vtilde, divTilde);
	gpuErrorCheck();
}


__global__
void tensorGradient_kernel(int zBlock, const vector3<int> S, const matrix3<> G, const complex* Xtilde, tensor3<complex*> gradTilde)
{	COMPUTE_halfGindices
	tensorGradient_calc(i, iG, IS_NYQUIST, G, Xtilde, gradTilde);
}
void tensorGradient_gpu(const vector3<int> S, const matrix3<> G, const complex* Xtilde, tensor3<complex*> gradTilde)
{	GpuLaunchConfigHalf3D glc(tensorGradient_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		tensorGradient_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, G, Xtilde, gradTilde);
	gpuErrorCheck();
}


__global__
void tensorDivergence_kernel(int zBlock, const vector3<int> S, const matrix3<> G, tensor3<const complex*> Vtilde, complex* divTilde)
{	COMPUTE_halfGindices
	tensorDivergence_calc(i, iG, IS_NYQUIST, G, Vtilde, divTilde);
}
void tensorDivergence_gpu(const vector3<int> S, const matrix3<> G, tensor3<const complex*> Vtilde, complex* divTilde)
{	GpuLaunchConfigHalf3D glc(tensorDivergence_kernel, S);
	for(int zBlock=0; zBlock<glc.zBlockMax; zBlock++)
		tensorDivergence_kernel<<<glc.nBlocks,glc.nPerBlock>>>(zBlock, S, G, Vtilde, divTilde);
	gpuErrorCheck();
}
