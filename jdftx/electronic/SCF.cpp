/*-------------------------------------------------------------------
Copyright 2013 Deniz Gunceler

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

#include <electronic/SCF.h>

SCF::SCF(Everything& e): e(e), overlap(e.residualMinimizerParams.history, e.residualMinimizerParams.history, false)
{
	// set up the cacheing size
	ResidualMinimizerParams& rp = e.residualMinimizerParams;
	switch(rp.vectorExtrapolationMethod)
	{	case Anderson:
			rp.history = 2;
			break;
		case DIIS:
			rp.history = 4;
			break;
		default:
			rp.history = 1;	
			break;
	}
	
}

#define ifTau(command) if(e.exCorr.needsKEdensity()) command;

#define overlapResiduals(r1, r2) \
	(integral(r1[0]*r2[0]) + (r1.size() == 2 ? integral(r1[1]*r2[1]) : 0.))
	
#define	cacheResidual(var) \
	pastResiduals_ ## var.push_back(variable_ ## var - pastVariables_ ## var.back());

void SCF::minimize()
{
	ElecInfo& eInfo = e.eInfo;
	ElecVars& eVars = e.eVars;
	ResidualMinimizerParams rp = e.residualMinimizerParams;
	
	// Compute energy for the initial guess
	e.cntrl.fixed_n = false;
	e.ener = Energies();
	e.iInfo.update(e.ener);
	eVars.elecEnergyAndGrad(e.ener, 0, 0, 0);	
	e.ener = Energies(); 
	e.cntrl.fixed_n = true;
	
	// Initialize the variable that defines the single particle (Kohn-Sham) Hamiltonian
	// It can be either the densities (electronic and KE) or the potentials (Vscloc and Vtau)
	DataRptrCollection& variable_n = (rp.mixedVariable == density ? eVars.n : eVars.Vscloc);
	DataRptrCollection& variable_tau = (rp.mixedVariable == density ? eVars.tau : eVars.Vtau);
	logPrintf("\nWill mix electronic and kinetic %s at each iteration.\n", (rp.mixedVariable==density ? "density" : "potential"));
	
	// Set up variable history for vector extrapolation
	std::vector<DataRptrCollection> pastVariables_n, pastVariables_tau, pastResiduals_n, pastResiduals_tau;
	
	double Eprev = 0., E = 0.;
	
	logPrintf("\n------------------- SCF Cycle ---------------------\n");
	for(int scfCounter=0; scfCounter<e.residualMinimizerParams.nIterations; scfCounter++)
	{	
		// Clear history if full
		if((pastResiduals_n.size() >= rp.history) or (pastVariables_n.size() >= rp.history))
		{	pastVariables_n.erase(pastVariables_n.begin());
			ifTau(pastVariables_tau.erase(pastVariables_tau.begin()))
			if(rp.vectorExtrapolationMethod != plain)
			{	pastResiduals_n.erase(pastResiduals_n.begin());
				ifTau(pastResiduals_tau.erase(pastResiduals_tau.begin()););
			}
		}
		
		// Cache the old energy and variables
		Eprev = E;
		pastVariables_n.push_back(clone(variable_n));
		ifTau(pastVariables_tau.push_back(clone(variable_tau)))
		
		// Solve at fixed hamiltonian
		e.cntrl.fixed_n = true; e.ener = Energies();
		logSuspend(); e.elecMinParams.fpLog = nullLog;
		for(int q = 0; q < eInfo.nStates; q++)
		{	
			BandMinimizer bmin(e, q, true);
			bmin.minimize(e.elecMinParams);
		}
		logResume(); e.elecMinParams.fpLog = globalLog;
		e.cntrl.fixed_n = false; e.ener = Energies();
		
		// Compute new density and energy
		e.iInfo.update(e.ener);
		E = eVars.elecEnergyAndGrad(e.ener, 0, 0, 0);
		
		logPrintf("ResidualMinimize: Iter:\t%i\tEtot: %.15e\tdE: %.3e\n", scfCounter, E, E-Eprev);
		
		// Check for convergence, mix density or potential if otherwise
		if(fabs(E-Eprev) < rp.energyDiffThreshold)
		{	logPrintf("Residual Minimization Converged (|Delta E|<%le).\n\n", rp.energyDiffThreshold);
			break;
		}
		else
		{	
			if((rp.vectorExtrapolationMethod == plain))
				mixPlain(variable_n, variable_tau, pastVariables_n.back(), pastVariables_tau.back(), 0.5);
			else if(rp.vectorExtrapolationMethod == Anderson)
			{	cacheResidual(n)
				ifTau(cacheResidual(tau))
				mixAnderson(variable_n, variable_tau, pastVariables_n, pastVariables_tau, pastResiduals_n, pastResiduals_tau);
			}
			else if(rp.vectorExtrapolationMethod == DIIS)
			{	cacheResidual(n)
				ifTau(cacheResidual(tau))
				mixDIIS(variable_n, variable_tau, pastVariables_n, pastVariables_tau, pastResiduals_n, pastResiduals_tau);
			}
		
			// Recompute Vscloc if mixing density
			if(e.residualMinimizerParams.mixedVariable == density)
				e.eVars.EdensityAndVscloc(e.ener);
		}
		
		logFlush();
	}
}

void SCF::mixPlain(DataRptrCollection& variable_n, DataRptrCollection& variable_tau, 
						 DataRptrCollection& mixingVariable_n, DataRptrCollection& mixingVariable_tau, double mixFraction)
{		// Mix old and new variable
		variable_n = mixFraction*variable_n + (1.-mixFraction)*mixingVariable_n;
		ifTau(variable_tau = mixFraction*variable_tau + (1.-mixFraction)*mixingVariable_tau;)
}

void SCF::mixDIIS(DataRptrCollection& variable_n, DataRptrCollection& variable_tau, 
				  std::vector< DataRptrCollection >& pastVariables_n, std::vector< DataRptrCollection >& pastVariables_tau, 
				  std::vector< DataRptrCollection >& pastResiduals_n, std::vector< DataRptrCollection >& pastResiduals_tau)
{
	
	logPrintf("\nWARNING: DIIS is still very experimental.  Exercise extreme caution when using it.\n");
	
	// dimension of the subspace over which minimization is done
	size_t ndim = pastResiduals_n.size();
	
	// Compute the overlap of the new residual with the older ones
	for(size_t j=0; j<ndim; j++)
	{	double tempOverlap = overlapResiduals(pastResiduals_n[j], pastResiduals_n.back());
		overlap.set(j, ndim-1, tempOverlap);
		if(j != ndim-1) overlap.set(ndim-1, j, tempOverlap);
	}
	
	// if only 1 previous density exists, then does plain mixing
	if(ndim != e.residualMinimizerParams.history)
	{	mixPlain(variable_n, variable_tau, pastVariables_n.back(), pastVariables_tau.back());
		return;
	}
	
	// diagonalize the residual overlap matrix to get the minimum of residual
	matrix thisOverlap = overlap(0, ndim, 0, ndim);
	matrix overlapEvecs(ndim, ndim); diagMatrix overlapEigs(ndim);
	thisOverlap.diagonalize(overlapEvecs, overlapEigs);
	
/*	logPrintf("Overlap Matrix:\n");
	thisOverlap.print_real(globalLog);
	logPrintf("\nOverlap Eigs:\n");
	overlapEigs.print(globalLog);
	logPrintf("\nOverlap Evecs:\n");
	overlapEvecs.print_real(globalLog);
	logPrintf("\n");
*/

	// normalizes the coefs of the eigenvector so that sum(x_i) = 1.
	double norm = 0.;
	for(size_t j=0; j<ndim; j++)
		norm += overlapEvecs.data()[overlapEvecs.index(j, 0)].real();
	logPrintf("\n\tNorm: %f\n", norm);
	
	// updates variables
	DataRptrCollection residual = clone(pastResiduals_n.back());
	for(size_t s=0; s<e.eVars.n.size(); s++)
	{	variable_n[s] *= 0.;
		residual[s] *= 0.;
		ifTau(variable_tau[s] *= 0.)
	}
	for(size_t j=0; j<ndim; j++)
	{	double weight = overlapEvecs.data()[overlapEvecs.index(j, 0)].real()/norm;
		for(size_t s=0; s<e.eVars.n.size(); s++)
		{	variable_n[s] += weight*pastVariables_n[j][s];
			residual[s] += weight*pastResiduals_n[j][s];
			ifTau(variable_tau[s] += weight*pastVariables_tau[j][s])
		}
	}
	
	logPrintf("\n\tTotal electron check: %f\n\n", integral(e.eVars.n[0]));
	logPrintf("\n\tThis residual: %f \t New residual: %f\n\n", overlapResiduals(pastResiduals_n.back(), pastResiduals_n.back()), overlapResiduals(residual, residual));
}

void SCF::mixAnderson(DataRptrCollection& variable_n, DataRptrCollection& variable_tau, 
					  std::vector< DataRptrCollection >& pastVariables_n, std::vector< DataRptrCollection >& pastVariables_tau, 
					  std::vector< DataRptrCollection >& pastResiduals_n, std::vector< DataRptrCollection >& pastResiduals_tau)
{
	// do a plain mixing for the first iteration
	if(pastResiduals_n.size() == 1)
	{	mixPlain(variable_n, variable_tau, pastVariables_n.back(), pastVariables_tau.back());
		return;
	}
	
	assert(pastResiduals_n.size() == 2);
	assert(pastVariables_n.size() == 2);
	ifTau(assert(pastVariables_tau.size() == 2))
	ifTau(assert(pastResiduals_tau.size() == 2))
	
	// compute the change in the residual
	DataRptrCollection deltaResidual = pastResiduals_n[1] - pastResiduals_n[0];
	
	//double alpha = -dot(pastResiduals[0],deltaResidual)/dot(deltaResidual, deltaResidual);
	double alpha = dot(pastResiduals_n[1], deltaResidual)/dot(deltaResidual, deltaResidual);
	logPrintf("\tmixingFraction = %f\n", 1-alpha);
	
	if((1-alpha) > 0.80 or (1-alpha) < 0.)
	{	logPrintf("\tDetected too agressive mixing fraction, resetting variable history.\n");
		mixPlain(variable_n, variable_tau, pastVariables_n.back(), pastVariables_tau.back(), 0.5);
		pastResiduals_n.clear();
		pastVariables_n.clear();
		ifTau(pastVariables_tau.clear())
		ifTau(pastResiduals_tau.clear())
		return;
	}
	
	variable_n = (1.-alpha)*(pastVariables_n[1]+pastResiduals_n[1]) + alpha*(pastVariables_n[0]+pastResiduals_n[0]);
	ifTau(variable_tau = (1.-alpha)*(pastVariables_tau[1]+pastResiduals_tau[1]) + alpha*(pastVariables_tau[0]+pastResiduals_tau[0]))
}
