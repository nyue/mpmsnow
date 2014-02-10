#ifndef MPMSIM_SNOWConstitutiveMODEL_H
#define MPMSIM_SNOWConstitutiveMODEL_H

#include "ConstitutiveModel.h"

namespace MpmSim
{

class SnowConstitutiveModel : public ConstitutiveModel
{
public:
	
	SnowConstitutiveModel(
			float youngsModulus,
			float poissonRatio,
			float hardening,
			float compressiveStrength,
			float tensileStrength );

	virtual void initParticles( ParticleData& p ) const;

	// update deformation at particle p:
	virtual void updateDeformation( ParticleData& d ) const;

	// energy density for particle p:
	virtual float energyDensity( const ParticleData& d, size_t p ) const;
	
	// matrix of derivatives of energy density with respect to components of the deformation gradient at particle p:
	virtual void dEnergyDensitydF( Eigen::Matrix3f& result, const ParticleData& d, size_t p ) const;
	
	// computes the change in dEnergyDensitydF when you change F by dFp:
	virtual void dEdFDifferential( Eigen::Matrix3f& result, const Eigen::Matrix3f& dFp, const ParticleData& d, size_t p ) const;

private:

	static float matrixDoubleDot( const Eigen::Matrix3f& a, const Eigen::Matrix3f& b );
	static Eigen::Matrix3f computeRdifferential( const Eigen::Matrix3f& dF, const Eigen::Matrix3f& R, const Eigen::Matrix3f& Ginv );
	
	float m_youngsModulus;
	float m_poissonRatio;
	float m_hardening;
	float m_compressiveStrength;
	float m_tensileStrength;

	float m_mu;
	float m_lambda;

};

} //namespace MpmSim

#endif
