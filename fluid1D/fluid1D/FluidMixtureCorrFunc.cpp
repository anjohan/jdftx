/*-------------------------------------------------------------------
Copyright 2012 Ravishankar Sundararaman

This file is part of Fluid1D.

Fluid1D is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fluid1D is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fluid1D.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------*/

#include <fluid1D/FluidMixture.h>
#include <core/BlasExtra.h>
#include <core/Util.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_linalg.h>

//Compute a^dagger.m.b for fixed size arrays
template<int N> double contract(double m[N][N], double a[N], double b[N])
{	double result = 0.0;
	for(register int i=0; i<N; i++)
		for(register int j=0; j<N; j++)
			result += m[i][j]*a[i]*b[j];
	return result;
}

ScalarFieldTildeCollection FluidMixture::getDirectCorrelations(const std::vector<double>& Nmol) const
{	assert(gInfo.coord == GridInfo::Spherical);

	//Compute direct correlations without factor of (-1/T):
	ScalarFieldTildeCollection C; nullToZero(C, gInfo, (nDensities*(nDensities+1))/2);
	
	//Compute site densities:
	std::vector<double> N(nDensities);
	for(unsigned ic=0; ic<component.size(); ic++)
	{	const Component& c = component[ic];
		for(int j=0; j<c.molecule->nSites; j++)
			N[c.offsetDensity + c.molecule->site[j].index] += Nmol[ic];
	}
	
	//--------- Electrostatics --------------
	ScalarFieldTilde coulomb(&gInfo); double* coulombData = coulomb.data();
	for(int i=0; i<gInfo.S; i++) coulombData[i] = i ? 4*M_PI/pow(gInfo.G[i],2) : 0.; //initialize Coulomb kernel
	//First loop over all site densities
	for(unsigned c1=0; c1<component.size(); c1++)
	{	unsigned offs1 = component[c1].offsetDensity;
		for(unsigned s1=0; s1<component[c1].indexedSite.size(); s1++)
		{	const SiteProperties& prop1 = *(component[c1].indexedSite[s1]);
			if(prop1.chargeKernel && prop1.chargeZ)
			{	ScalarFieldTilde weightedCoulomb1 = prop1.chargeZ * ((*prop1.chargeKernel) * coulomb);
				//Second loop over all site densities with net index lower than first one
				for(unsigned c2=0; c2<=c1; c2++)
				{	unsigned offs2 = component[c2].offsetDensity;
					double aDiel = (c1==c2) ? component[c1].fex->get_aDiel() : 1.; //correlation factor
					for(unsigned s2=0; s2<(c1==c2 ? s1+1 : component[c2].indexedSite.size()); s2++)
					{	const SiteProperties& prop2 = *(component[c2].indexedSite[s2]);
						if(prop2.chargeKernel && prop2.chargeZ)
							C[corrFuncIndex(offs1+s1,offs2+s2)] += (aDiel * prop2.chargeZ) * ((*prop2.chargeKernel) * weightedCoulomb1);
					}
				}
			}
		}
	}
	
	//--------- Hard-sphere mixture --------------
	//Compute uniform fluid weighted densities:
	double n0=0., n1=0., n2=0., n3=0.;
	std::vector<double> n0molArr(component.size(), 0); //partial n0 for molecules that need bonding corrections
	std::vector<int> n0mult(component.size(), 0); //number of sites which contribute to n0 for each molecule
	std::vector<std::map<double,int> > bond(component.size()); //sets of bonds for each molecule
	bool bondsPresent = false; //whether bonds are present for any molecule
	for(unsigned ic=0; ic<component.size(); ic++)
	{	const Component& c = component[ic];
		bond[ic] = c.molecule->getBonds();
		double& n0mol = n0molArr[ic];
		for(int j=0; j<c.molecule->nIndices; j++)
		{	const SiteProperties& s = *(c.indexedSite[j]);
			if(s.sphereRadius)
			{	double Nsite = N[c.offsetDensity+j];
				n0mult[ic] += c.indexedSiteMultiplicity[j];
				n0mol += s.w0->at(0) * Nsite;
				n1    += s.w1->at(0) * Nsite;
				n2    += s.w2->at(0) * Nsite;
				n3    += s.w3->at(0) * Nsite;
			}
		}
		n0 += n0mol;
		if(bond[ic].size()) bondsPresent = true;
	}
	if(n0) //at least one component in the mixture has hard spheres
	{	double hGrid = gInfo.rMax / gInfo.S;
		
		//Hessian of the White-Bear mark II FMT w.r.t (n0, n1, n2, n3, n1v, n2v) in the uniform fluid limit:
		//(Generated by a Mathematica script. Note that the tensor corrections do not affect these correlations since they are cubic)
		double d2phi[6][6] = {
			{0,0,0,1/(1 - n3),0,0},
			{0,0,((-5 + n3)/(-1 + n3) + (2*log(1 - n3))/n3)/3.,(-2*n2*(n3 - 3*pow(n3,2) + pow(-1 + n3,2)*log(1 - n3)))/(3.*pow(-1 + n3,2)*pow(n3,2)),0,0},
			{0,((-5 + n3)/(-1 + n3) + (2*log(1 - n3))/n3)/3.,-(n2*(n3*(1 + (-3 + n3)*n3) + pow(-1 + n3,2)*log(1 - n3)))/(6.*pow(-1 + n3,2)*pow(n3,2)*M_PI),(n3*(pow(n2,2)*(-2 + (-5 + n3)*(-1 + n3)*n3) + 8*n1*(-1 + n3)*n3*(-1 + 3*n3)*M_PI) + 2*pow(-1 + n3,3)*(pow(n2,2) - 4*n1*n3*M_PI)*log(1 - n3))/(12.*pow(-1 + n3,3)*pow(n3,3)*M_PI),0,0},
			{1/(1 - n3),(-2*n2*(n3 - 3*pow(n3,2) + pow(-1 + n3,2)*log(1 - n3)))/(3.*pow(-1 + n3,2)*pow(n3,2)),(n3*(pow(n2,2)*(-2 + (-5 + n3)*(-1 + n3)*n3) + 8*n1*(-1 + n3)*n3*(-1 + 3*n3)*M_PI) + 2*pow(-1 + n3,3)*(pow(n2,2) - 4*n1*n3*M_PI)*log(1 - n3))/(12.*pow(-1 + n3,3)*pow(n3,3)*M_PI),(n3*(pow(n2,3)*(-6 + n3*(21 + n3*(-26 + (19 - 2*n3)*n3))) + 12*(-1 + n3)*n3*(3*n0*(-1 + n3)*pow(n3,2) - 2*n1*n2*(2 + n3*(-5 + 7*n3)))*M_PI) - 6*n2*pow(-1 + n3,4)*(pow(n2,2) - 8*n1*n3*M_PI)*log(1 - n3))/(36.*pow(-1 + n3,4)*pow(n3,4)*M_PI),0,0},
			{0,0,0,0,0,-(-5 + n3)/(3.*(-1 + n3)) - (2*log(1 - n3))/(3.*n3)},
			{0,0,0,0,-(-5 + n3)/(3.*(-1 + n3)) - (2*log(1 - n3))/(3.*n3),(n2*(n3*(1 + (-3 + n3)*n3) + pow(-1 + n3,2)*log(1 - n3)))/(6.*pow(-1 + n3,2)*pow(n3,2)*M_PI)} };
		
		//First loop over all site densities
		for(unsigned c1=0; c1<component.size(); c1++)
		{	unsigned offs1 = component[c1].offsetDensity;
			for(unsigned s1=0; s1<component[c1].indexedSite.size(); s1++)
			{	const SiteProperties& prop1 = *(component[c1].indexedSite[s1]);
				if(prop1.sphereRadius)
				{	//Second loop over all site densities with net index lower than first one
					for(unsigned c2=0; c2<=c1; c2++)
					{	unsigned offs2 = component[c2].offsetDensity;
						for(unsigned s2=0; s2<(c1==c2 ? s1+1 : component[c2].indexedSite.size()); s2++)
						{	const SiteProperties& prop2 = *(component[c2].indexedSite[s2]);
							if(prop2.sphereRadius)
							{	double* Cdata = C[corrFuncIndex(offs1+s1,offs2+s2)].data();
								for(int i=0; i<gInfo.S; i++)
								{	double smooth = exp(-pow(0.5*gInfo.G[i]*hGrid,2)); //suppress Nyquist frequency components
									//Collect FMT weight functions for both components at current G:
									double weights1[6] = { (*prop1.w0)[i], (*prop1.w1)[i], (*prop1.w2)[i], (*prop1.w3)[i], gInfo.G[i]*(*prop1.w1v)[i], -gInfo.G[i]*(*prop1.w3)[i] };
									double weights2[6] = { (*prop2.w0)[i], (*prop2.w1)[i], (*prop2.w2)[i], (*prop2.w3)[i], gInfo.G[i]*(*prop2.w1v)[i], -gInfo.G[i]*(*prop2.w3)[i] };
									Cdata[i] += T * contract(d2phi, weights1, weights2) * smooth;
								}
							}
						}
					}
				}
			}
		}
		
		if(bondsPresent)
		{	for(unsigned ic=0; ic<component.size(); ic++)
			{	double n0mol = n0molArr[ic];
				for(std::map<double,int>::iterator b=bond[ic].begin(); b!=bond[ic].end(); b++)
				{	double Rhm = b->first;
					double scale = b->second*(-1.)/n0mult[ic]; //prefactor to bonding term including corrections for multiple-counting
					
					//Hessian of the Wertheim bonding corrections w.r.t (n0mol, n2, n3, n2v) in the uniform fluid limit:
					//(Generated by a Mathematica script. These are without multiple-counting corretcions)
					double d2abond[4][4] = {
						{0,(Rhm*(9 - 9*n3 + 4*n2*Rhm))/((-3 + 3*n3 - 2*n2*Rhm)*(-3 + 3*n3 - n2*Rhm)),-3/(-1 + n3) - 3/(3 - 3*n3 + n2*Rhm) - 3/(3 - 3*n3 + 2*n2*Rhm),0},
						{(Rhm*(9 - 9*n3 + 4*n2*Rhm))/((-3 + 3*n3 - 2*n2*Rhm)*(-3 + 3*n3 - n2*Rhm)),(n0mol*pow(Rhm,2)*(-45*pow(-1 + n3,2) + 36*n2*(-1 + n3)*Rhm - 8*pow(n2,2)*pow(Rhm,2)))/(pow(3 - 3*n3 + n2*Rhm,2)*pow(3 - 3*n3 + 2*n2*Rhm,2)),(9*n0mol*Rhm*(9 + 9*pow(n3,2) + 2*n2*Rhm*(4 + n2*Rhm) - 2*n3*(9 + 4*n2*Rhm)))/(pow(3 - 3*n3 + n2*Rhm,2)*pow(3 - 3*n3 + 2*n2*Rhm,2)),0},
						{-3/(-1 + n3) - 3/(3 - 3*n3 + n2*Rhm) - 3/(3 - 3*n3 + 2*n2*Rhm),(9*n0mol*Rhm*(9 + 9*pow(n3,2) + 2*n2*Rhm*(4 + n2*Rhm) - 2*n3*(9 + 4*n2*Rhm)))/(pow(3 - 3*n3 + n2*Rhm,2)*pow(3 - 3*n3 + 2*n2*Rhm,2)),3*n0mol*(pow(-1 + n3,-2) - 3/pow(3 - 3*n3 + n2*Rhm,2) - 3/pow(3 - 3*n3 + 2*n2*Rhm,2)),0},
						{0,0,0,(-2*n0mol*Rhm*(9 - 9*n3 + 2*n2*Rhm))/(n2*(3 - 3*n3 + n2*Rhm)*(3 - 3*n3 + 2*n2*Rhm))} };

					//Note that the bonding corrections in one molecule contribute to the direct correlations of all hard-sphere components:
					//First loop over all site densities
					for(unsigned c1=0; c1<component.size(); c1++)
					{	unsigned offs1 = component[c1].offsetDensity;
						for(unsigned s1=0; s1<component[c1].indexedSite.size(); s1++)
						{	const SiteProperties& prop1 = *(component[c1].indexedSite[s1]);
							if(prop1.sphereRadius)
							{	//Second loop over all site densities with net index lower than first one
								for(unsigned c2=0; c2<=c1; c2++)
								{	unsigned offs2 = component[c2].offsetDensity;
									for(unsigned s2=0; s2<(c1==c2 ? s1+1 : component[c2].indexedSite.size()); s2++)
									{	const SiteProperties& prop2 = *(component[c2].indexedSite[s2]);
										if(prop2.sphereRadius)
										{	double* Cdata = C[corrFuncIndex(offs1+s1,offs2+s2)].data();
											for(int i=0; i<gInfo.S; i++)
											{	double smooth = exp(-pow(0.5*gInfo.G[i]*hGrid,2)); //suppress Nyquist frequency components
												//Collect FMT weight functions for both components at current G:
												double weights1[4] = { c1==ic ? (*prop1.w0)[i] : 0., (*prop1.w2)[i], (*prop1.w3)[i], -gInfo.G[i]*(*prop1.w3)[i] };
												double weights2[4] = { c2==ic ? (*prop2.w0)[i] : 0., (*prop2.w2)[i], (*prop2.w3)[i], -gInfo.G[i]*(*prop2.w3)[i] };
												Cdata[i] += scale * T * contract(d2abond, weights1, weights2) * smooth;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	//---------- Excess functionals --------------
	for(std::vector<Component>::const_iterator c=component.begin(); c!=component.end(); c++)
		c->fex->directCorrelations(&N[c->offsetDensity], C);

	//--------- Mixing functionals --------------
	for(std::vector<const Fmix*>::const_iterator fmix=fmixArr.begin(); fmix!=fmixArr.end(); fmix++)
		(*fmix)->directCorrelations(N, C);

	//Return after including factor of (-1/T)
	C *= (-1./T);
	return C;
}

namespace FluidMixtureCorrFunc
{	//Square matrix class designed to interact with ScalarFieldCollection
	struct matrix
	{	const unsigned M; //M x M matrix
		double* data;
		
		matrix(unsigned M) : M(M), data(new double[M*M]) {}
		~matrix() { delete[] data; }
		matrix(const matrix& other) : M(other.M), data(new double[M*M]) { *this = other; }
		matrix& operator=(const matrix& other) { assert(M == other.M); eblas_copy(data, other.data, M*M); return *this; }
		
		double& operator()(unsigned i, unsigned j) { return data[i+j*M]; } //column-major storage
		const double& operator()(unsigned i, unsigned j) const { return data[i+j*M]; } //column-major storage
		
		void zero() { eblas_zero(M*M, data); }
		
		//! Load from a symmetric-matrix array
		void loadSymmetric(const std::vector<const double*>& data, unsigned offs)
		{	assert(data.size() == (M*(M+1))/2);
			unsigned iData = 0;
			for(unsigned i=0; i<M; i++)
				for(unsigned j=0; j<=i; j++)
					(*this)(i,j) = ( (*this)(j,i) = data[iData++][offs] );
		}
		
		//! Save to a symmetric-matrix array
		void saveSymmetric(std::vector<double*>& data, unsigned offs)
		{	assert(data.size() == (M*(M+1))/2);
			unsigned iData = 0;
			for(unsigned i=0; i<M; i++)
				for(unsigned j=0; j<=i; j++)
					data[iData++][offs] = 0.5 * ((*this)(i,j) + (*this)(j,i)); //explicitly symmetrize
		}
		
		matrix operator*(const matrix& other) const
		{	assert(M == other.M);
			matrix ret(M);
			ret.zero();
			for(unsigned k=0; k<M; k++)
				for(unsigned j=0; j<M; j++)
					for(unsigned i=0; i<M; i++)
						ret(i,k) += (*this)(i,j) * other(j,k);
			return ret;
		}
		
		matrix& operator+=(const matrix& other)
		{	assert(M == other.M);
			eblas_daxpy(M*M, 1., other.data,1, data,1);
			return *this;
		}
		matrix operator+(const matrix& other) const
		{	return matrix(*this) += other;
		}
		
		matrix& operator-=(const matrix& other)
		{	assert(M == other.M);
			eblas_daxpy(M*M, -1., other.data,1, data,1);
			return *this;
		}
		matrix operator-(const matrix& other) const
		{	return matrix(*this) -= other;
		}
		
		matrix inverse() const
		{	//Create a destructible copy, and perform in-place LU decomposition:
			matrix LU(*this);
			gsl_matrix_view LUview = gsl_matrix_view_array(LU.data, M, M);
			gsl_permutation* p = gsl_permutation_alloc(M); int signum;
			gsl_linalg_LU_decomp(&(LUview.matrix), p, &signum);
			//Compute the inverse from the LU decomposition:
			matrix ret(M);
			gsl_matrix_view retView = gsl_matrix_view_array(ret.data, M, M);
			gsl_linalg_LU_invert(&(LUview.matrix), p, &(retView.matrix));
			return ret;
		}
	};
};


ScalarFieldCollection FluidMixture::getPairCorrelations(const std::vector<double>& Nmol) const
{	assert(gInfo.coord == GridInfo::Spherical);
	using namespace FluidMixtureCorrFunc;
	
	//Get the direct correlations:
	ScalarFieldTildeCollection cTilde = getDirectCorrelations(Nmol);
	std::vector<const double*> cData = getConstData(cTilde);
	
	//Initialize a diagonal matrix with molecular densities
	matrix diagN(nDensities);
	diagN.zero();
	for(unsigned ic=0; ic<component.size(); ic++)
	{	unsigned iStart = component[ic].offsetDensity;
		unsigned iStop = iStart + component[ic].indexedSite.size();
		for(unsigned i=iStart; i<iStop; i++)
			diagN(i,i) = Nmol[ic];
	}
	
	//Initialize an identity matrix:
	matrix eye(nDensities);
	eye.zero();
	for(unsigned i=0; i<nDensities; i++)
		eye(i,i) = 1.;
	
	//Compute the total correlation h by the generalized Ornstein-Zernike relation (See Water1D/OZ.pdf):
	ScalarFieldTildeCollection hTilde; nullToZero(hTilde, gInfo, (nDensities*(nDensities+1))/2);
	std::vector<double*> hData = getData(hTilde);
	
	for(int i=0; i<gInfo.S; i++)
	{	double G = gInfo.G[i];
		
		//Get direct corrrrelations at current G:
		matrix c(nDensities);
		c.loadSymmetric(cData, i);
		
		//Compute intra-molecular structure:
		matrix I(nDensities);
		I.zero();
		for(const Component& c: component)
		{	unsigned offs = c.offsetDensity;
			const Molecule& m = *(c.molecule);
			for(const Site& s1: m.site)
				for(const Site& s2: m.site)
					I(offs+s1.index, offs+s2.index) += gsl_sf_bessel_j0(G * (s1.pos-s2.pos).length());
		}
		
		//Generalized OZ-relation:
		matrix h = (eye - I*c*diagN).inverse() * I*c*I;
		h.saveSymmetric(hData, i);
	}
	cTilde.clear(); 
	
	//Collect site multiplicities:
	std::vector<int> siteMult;
	for(const Component& c: component)
		for(int mult: c.indexedSiteMultiplicity)
			siteMult.push_back(mult);
	
	//Fourier transform, normalize for site multiplicities and offset h to get the pair correlations g:
	ScalarFieldCollection g((nDensities*(nDensities+1))/2);
	for(unsigned i=0; i<nDensities; i++)
		for(unsigned j=0; j<=i; j++)
			g[corrFuncIndex(i,j)] = 1. + (1./(siteMult[i]*siteMult[j])) * I(hTilde[corrFuncIndex(i,j)]);
	return g;
}


int FluidMixture::corrFuncIndex(unsigned i1, unsigned i2, const Fex* fex) const
{	unsigned offs = fex ? get_offsetDensity(fex) : 0;
	unsigned j1 = offs + std::max(i1,i2);
	unsigned j2 = offs + std::min(i1,i2);
	return j2 + (j1*(j1+1))/2;
}
