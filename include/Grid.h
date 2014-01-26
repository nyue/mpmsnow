#ifndef MPMSNOW_GRID_H
#define MPMSNOW_GRID_H

#include "ParticleData.h"
#include "CollisionObject.h"
#include "Solver.h"

#include <Eigen/Dense>

#define GRID_H 0.1f
#define TIME_STEP 0.01f
#define INITIALDENSITY 400
#define GRAVITY -9.8f
#define COULOMBFRICTION 0.5f

#define THETA_C 2.5e-2f
#define THETA_S 7.5e-3f

#define HARDENING 10

//#define PLASTICITY 1

#define YOUNGSMODULUS 1.4e5f
#define POISSONRATIO 0.2f

#define MU ( YOUNGSMODULUS / ( 2 * ( 1 + POISSONRATIO ) ) )
#define LAMBDA ( YOUNGSMODULUS * POISSONRATIO / ( ( 1 + POISSONRATIO ) * ( 1 - 2 * POISSONRATIO ) ) )
#define BETA 1

class Solver;

class Grid
{

public:

	Grid( const ParticleData& d );

	void draw() const;
	void computeDensities( ParticleData& d ) const;

	void updateGridVelocities(
		const ParticleData& d,
		const std::vector<CollisionObject*>& collisionObjects,
		const Solver& implicitSolver );

	void updateDeformationGradients( ParticleData& d );
	void updateParticleVelocities( ParticleData& d );
	
	// testing:
	void testForces( const ParticleData& d );
	void testForceDifferentials( const ParticleData& d );
	unsigned matrixTexture( const ParticleData& d, const std::vector<CollisionObject*>& collisionObjects ) const;

	void applyImplicitUpdateMatrix( const ParticleData& d, const std::vector<CollisionObject*>& collisionObjects, const Eigen::VectorXf& v, Eigen::VectorXf& result ) const;

private:

	static float matrixDoubleDot( const Eigen::Matrix3f& a, const Eigen::Matrix3f& b );
	static Eigen::Matrix3f computeRdifferential( const Eigen::Matrix3f& dF, const Eigen::Matrix3f& R, const Eigen::Matrix3f& S );
	
	void calculateForces( const ParticleData& d, Eigen::VectorXf& forces ) const;
	void calculateForceDifferentials( const ParticleData& d, const Eigen::VectorXf& dx, Eigen::VectorXf& df ) const;

	// testing
	float calculateEnergy( const ParticleData& d ) const;
	
	// shape functions and derivatives:
	static inline float N( float x );
	static inline float DN( float x );

	static inline void minMax( float x, float& min, float& max );
	static inline int fixDim( float& min, float& max );

	inline int coordsToIndex( int x, int y, int z ) const;

	void cellAndWeights( const Eigen::Vector3f& particleX, Eigen::Vector3i& particleCell, float *w[], float** dw = 0 ) const;

private:

	float m_xmin;
	float m_ymin;
	float m_zmin;

	float m_xmax;
	float m_ymax;
	float m_zmax;
	
	int m_nx;
	int m_ny;
	int m_nz;

	Eigen::VectorXf m_gridMasses;
	Eigen::VectorXf m_gridVelocities;
	Eigen::VectorXf m_prevGridVelocities;
	std::vector<bool> m_nodeCollided;

};


#endif // MPMSNOW_GRID_H
