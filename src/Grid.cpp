#include "Grid.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <iostream>

#include <GL/gl.h>

#include <Eigen/Geometry>
#include <Eigen/SVD>

using namespace Eigen;

#define DECLARE_WEIGHTARRAY( NAME ) float buf_##NAME[12]; float * NAME[] = { &buf_##NAME[1], &buf_##NAME[5], &buf_##NAME[9] };

Grid::Grid( const ParticleData& d )
{
	// work out the physical size of the grid:
	m_xmin = m_ymin = m_zmin = 1.e10;
	m_xmax = m_ymax = m_zmax = -1.e10;

	for( size_t i=0; i < d.particleX.size(); ++i )
	{
		minMax( d.particleX[i][0], m_xmin, m_xmax );
		minMax( d.particleX[i][1], m_ymin, m_ymax );
		minMax( d.particleX[i][2], m_zmin, m_zmax );
	}
	
	// calculate grid dimensions and quantize bounding box:
	m_nx = fixDim( m_xmin, m_xmax );
	m_ny = fixDim( m_ymin, m_ymax );
	m_nz = fixDim( m_zmin, m_zmax );
			
	// little array with indexes going from -1 to store shape function weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	Vector3i particleCell;
	
	// calculate masses:
	m_gridMasses.resize( m_nx * m_ny * m_nz );
	m_gridMasses.setZero();

	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		Vector3f x = d.particleX[p];
		cellAndWeights( x, particleCell, w );
		// splat the masses:
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					float weight = w[0][i] * w[1][j] * w[2][k];
					m_gridMasses[ idx ] +=
						d.particleM[p] * weight;
				}
			}
		}
	}

	// calculate velocities:
	m_gridVelocities.resize( m_nx * m_ny * m_nz * 3 );
	m_gridVelocities.setZero();

	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w );
		
		// splat the velocities:
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					if( m_gridMasses[idx] > 0 )
					{
						float particleMass = d.particleM[p];
						float gridCellMass = m_gridMasses[idx];
						float overallWeight = w[0][i] * w[1][j] * w[2][k] *
							( particleMass / gridCellMass );

						m_gridVelocities.segment<3>( 3 * idx ) += overallWeight * d.particleV[p];
					}
				}
			}
		}
	}		
}

void Grid::draw() const
{
	glColor3f( 0,0.3f,0 );
	glBegin( GL_LINES );

	// xy
	for( int i=0; i <= m_nx; ++i )
	{
		for( int j=0; j <= m_ny; ++j )
		{
			glVertex3f( m_xmin + i * GRID_H, m_ymin + j * GRID_H, m_zmin );
			glVertex3f( m_xmin + i * GRID_H, m_ymin + j * GRID_H, m_zmax );
		}
	}
	// zy
	for( int i=0; i <= m_nz; ++i )
	{
		for( int j=0; j <= m_ny; ++j )
		{
			glVertex3f( m_xmin, m_ymin + j * GRID_H, m_zmin + i * GRID_H );
			glVertex3f( m_xmax, m_ymin + j * GRID_H, m_zmin + i * GRID_H );
		}
	}

	// xz
	for( int i=0; i <= m_nx; ++i )
	{
		for( int j=0; j <= m_nz; ++j )
		{
			glVertex3f( m_xmin + i * GRID_H, m_ymin, m_zmin + j * GRID_H );
			glVertex3f( m_xmin + i * GRID_H, m_ymax, m_zmin + j * GRID_H );
		}
	}
	glEnd();
}

void Grid::computeDensities( ParticleData& d ) const
{
	d.particleDensities.resize( d.particleX.size(), 0 );
	
	// little array with indexes going from -1 to store shape function weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	Vector3i particleCell;

	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w );
		
		// transfer densities back onto the particles:
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					
					// accumulate the particle's density:
					d.particleDensities[p] += w[0][i] * w[1][j] * w[2][k] * m_gridMasses[ idx ] / ( GRID_H * GRID_H * GRID_H );
					
				}
			}
		}
	}
}

void Grid::updateParticleVelocities( ParticleData& d )
{
	// little array with indexes going from -1 to store shape function weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	Vector3i particleCell;

	// this is, like, totally doing things FLIP style. The paper recommends a combination of FLIP and PIC...
	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w );

		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					d.particleV[p] += w[0][i] * w[1][j] * w[2][k] *
						( m_gridVelocities.segment<3>( 3 * idx ) - m_prevGridVelocities.segment<3>( 3 * idx ) );
				}
			}
		}
	}
}

float Grid::matrixDoubleDot( const Matrix3f& a, const Matrix3f& b )
{
	return
		a(0,0) * b(0,0) + a(0,1) * b(0,1) + a(0,2) * b(0,2) + 
		a(1,0) * b(1,0) + a(1,1) * b(1,1) + a(1,2) * b(1,2) + 
		a(2,0) * b(2,0) + a(2,1) * b(2,1) + a(2,2) * b(2,2);
}

Matrix3f Grid::computeRdifferential( const Matrix3f& dF, const Matrix3f& R, const Matrix3f& S )
{
	Matrix3f M = R.transpose() * dF - dF.transpose() * R;
	Vector3f w( M(0,1), M(0,2), M(1,2) );
	
	Matrix3f G;
	G(0,0) = S(0,0) + S(1,1);
	G(1,1) = S(0,0) + S(2,2);
	G(2,2) = S(1,1) + S(2,2);
	
	G(0,1) = G(1,0) = S(1,2);
	G(0,2) = G(2,0) = -S(0,2);
	G(1,2) = G(2,1) = S(0,1);
	
	w = G.inverse() * w;
	
	Matrix3f RtdR;
	RtdR(0,0) = RtdR(1,1) = RtdR(2,2) = 0;
	
	RtdR(0,1) = w[0];
	RtdR(1,0) = -w[0];
	
	RtdR(0,2) = w[1];
	RtdR(2,0) = -w[1];
	
	RtdR(1,2) = w[2];
	RtdR(2,1) = -w[2];
	
	return R * RtdR;
}


void Grid::applyImplicitUpdateMatrix(
	const ParticleData& d,
	const std::vector<CollisionObject*>& collisionObjects,
	const VectorXf& vNPlusOne,
	VectorXf& result ) const
{

	// This method computes the left hand side of the following equation:
	// v^(n+1) - TIME_STEP / m * dF(v^(n+1) * TIME_STEP)
	
	// work out force differentials when you perturb the grid positions by v * TIME_STEP:
	VectorXf df( vNPlusOne.size() );
	calculateForceDifferentials( d, TIME_STEP * vNPlusOne, df );
	
	result = vNPlusOne;
	for( int i=0; i < m_nx; ++i )
	{
		for( int j=0; j < m_ny; ++j )
		{
			for( int k=0; k < m_nz; ++k )
			{
				int idx = coordsToIndex( i, j, k );
				if( m_gridMasses[idx] != 0 )
				{
					Vector3f resultV = result.segment<3>( 3 * idx ) - TIME_STEP / m_gridMasses[idx] * df.segment<3>( 3 * idx );
					
					// project out collided degrees of freedom:
					/*
					Vector3f x( GRID_H * i + m_xmin, GRID_H * j + m_ymin, GRID_H * k + m_zmin );
					for( size_t objIdx = 0; objIdx < collisionObjects.size(); ++objIdx )
					{
						if( m_nodeCollided[idx] )
						{
							Vector3f n;
							collisionObjects[objIdx]->grad( x, n );
							
							float nDotV = n.dot( resultV );
							resultV -= ( nDotV / n.dot(n) ) * n;
						}
					}
					*/
					result.segment<3>( 3 * idx ) = resultV;
				}
			}
		}
	}

	for( int i=0; i < m_gridMasses.size(); ++i )
	{
		result.segment<3>( 3 * i ) *= m_gridMasses[i];
	}

}

void Grid::calculateForceDifferentials( const ParticleData& d, const VectorXf& dx, VectorXf& df ) const
{
	df.setZero();

	// little array with indexes going from -1 to store shape function derivative weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	DECLARE_WEIGHTARRAY( dw );
	Vector3i particleCell;
	
	for( size_t p = 0; p < d.particleF.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w, dw );
		
		// work out deformation gradient differential for this particle when grid nodes are
		// moved by their respective v * Dt
		Matrix3f dFp = Matrix3f::Zero();
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
					Vector3f deltaX = dx.segment<3>( 3 * idx );
					dFp += deltaX * weightGrad.transpose() * d.particleF[p];
				}
			}
		}
		
		// work out energy derivatives with respect to the deformation gradient at this particle:
		// Ap = d2Psi / dF dF : dF (see the tech report). We've got dF, so just plug that into the
		// formulae...
		
		// if you look down in updateGridVelocities, you'll find this expression, which is used while
		// computing the force on a grid node:
		
		// 2 * MU * ( d.particleF[p] - d.particleR[p] ) + LAMBDA * ( d.particleJ[p] - 1 ) * d.particleJ[p] * d.particleFinvTrans[p];
		
		// what we're doing here is just assuming dFp is small and working out the corresponding variation in
		// that expression...
		
		float J = d.particleJ[p];

		// work out a couple of basic differentials:
		float dJ = J * matrixDoubleDot( d.particleFinvTrans[p], dFp );
		Matrix3f dFInvTrans = - d.particleFinvTrans[p] * dFp.transpose() * d.particleFinvTrans[p];
		
		Matrix3f dR = computeRdifferential( dFp, d.particleR[p], d.particleS[p] );
		
		// start with differential of 2 * MU * ( F - R )...
		Matrix3f Ap = 2 * d.particleMu[p] * ( dFp - dR );
		
		// add on differential of LAMBDA * ( J - 1 ) * J * F^-t
		// = LAMBDA * ( d( J - 1 ) * J F^-T + ( J - 1 ) * d( J F^-t ) )
		// = LAMBDA * ( dJ * J F^-T + ( J - 1 ) * ( dJ F^-t + J * d( F^-t ) )
		Ap += d.particleLambda[p] * ( dJ * J * d.particleFinvTrans[p] + ( J - 1 ) * ( dJ * d.particleFinvTrans[p] + J * dFInvTrans ) );
		
		Matrix3f forceMatrix = d.particleVolumes[p] * Ap * d.particleF[p].transpose();

		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
					
					// add on difference in velocity due to this force:
					df.segment<3>( 3 * idx ) -= forceMatrix * weightGrad;
				}
			}
		}
	}
}

void Grid::calculateForces( const ParticleData& d, VectorXf& forces ) const
{
	// little array with indexes going from -1 to store shape function derivative weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	DECLARE_WEIGHTARRAY( dw );
	Vector3i particleCell;
	
	// start with gravity:
	for( int i=0; i < m_gridMasses.size(); ++i )
	{
		forces.segment<3>(3 * i) = m_gridMasses[i] * Vector3f( 0, GRAVITY, 0 );
	}
	
	// add on internal forces:
	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w, dw );
		
		Matrix3f dEdF = 2 * d.particleMu[p] * ( d.particleF[p] - d.particleR[p] ) + d.particleLambda[p] * ( d.particleJ[p] - 1 ) * d.particleJ[p] * d.particleFinvTrans[p];
		
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
					forces.segment<3>( 3 * idx ) -= d.particleVolumes[p] * dEdF * d.particleF[p].transpose() * weightGrad;
				}
			}
		}
	}
}

float Grid::calculateEnergy( const ParticleData& d ) const
{
	float e = 0;
	for( size_t p=0; p < d.particleF.size(); ++p )
	{
		Matrix3f rigidDeviation = d.particleF[p] - d.particleR[p];
		float JminusOne = d.particleJ[p] - 1;
		e += d.particleVolumes[p] * ( d.particleMu[p] * matrixDoubleDot( rigidDeviation, rigidDeviation ) + 0.5f * d.particleLambda[p] * JminusOne * JminusOne );
	}
	return e;
}

unsigned Grid::matrixTexture( const ParticleData& d, const std::vector<CollisionObject*>& collisionObjects ) const
{
	VectorXf x( m_gridVelocities.size() );
	x.setZero();
	
	VectorXf b( m_gridVelocities.size() );
	MatrixXf M( m_gridVelocities.size(), m_gridVelocities.size() );
	
	for( int i=0; i < x.size(); ++i )
	{
		std::cerr << i << " of " << x.size() << std::endl;
		x[i] = 1;
		applyImplicitUpdateMatrix(d, collisionObjects, x, b );
		x[i] = 0;
		M.block( 0, i, m_gridVelocities.size(), 1 ) = b;
	}
	
	float maxM = M.maxCoeff();
	float minM = M.minCoeff();
	float norm = fabs( minM );
	if( fabs( maxM ) > norm )
	{
		norm = fabs( maxM );
	}

	std::vector<unsigned char> sparsity;
	int texW = int( ceil( (float)x.size() / 2 ) * 2 );
	int texH = texW;

	for(int j=0; j < texH; ++j)
	{
		for(int i=0; i < texW; ++i)
		{
			if( i >= x.size() || j >= x.size() )
			{
				sparsity.push_back( 0 );
				continue;
			}
			
			sparsity.push_back( (unsigned char)( fabs( M(i,j) ) * 255 / norm ) );
		}
	}

	GLuint matrixTexture;
	
	glEnable( GL_TEXTURE_2D );
	glGenTextures( 1, &matrixTexture );
	glBindTexture(GL_TEXTURE_2D, matrixTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, texW, texH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &sparsity[0] );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable( GL_TEXTURE_2D );
	
	std::cerr << "compute svd!!" << std::endl;
	
	JacobiSVD<MatrixXf> svd(M, Eigen::ComputeFullV);
	std::cerr << "done!" << std::endl;
	MatrixXf V = svd.matrixV();
	
	int numNulls = 0;
	for( int i=0; i < svd.singularValues().size(); ++i )
	{
		std::cerr << svd.singularValues()[i] << std::endl;
		if( svd.singularValues()[i] == 0 )
		{
			++numNulls;
		}
	}



	return matrixTexture;
}

void Grid::testForces( const ParticleData& d )
{
	// save the state so we don't screw the sim up:
	VectorXf originalGridVelocities = m_gridVelocities;

	// calculate da forces brah!
	VectorXf forces( m_gridVelocities.size() );
	calculateForces( d, forces );
	
	// calculate unperturbed energy:
	float e0 = calculateEnergy( d );
	
	// now we're gonna calculate energy derivatives... the stupid way!
	// we're gonna do this component by component, and we're gonna do it
	// by zeroing out the grid velocities, setting the component we're gonna
	// test to delta/TIME_STEP, advancing bits of the sim with that velocity field,
	// calculating the energy in the final state (in which one of the grid nodes
	// will have moved a distance delta along one of the axes), and using the result
	// to calculate a finite difference derivative!
	float delta = 0.01f;
	for( int idx = 0; idx < m_gridMasses.size(); ++idx )
	{
		for( size_t dim = 0; dim < 3; ++dim )
		{
			ParticleData dTest = d;
			m_gridVelocities.setZero();
			
			// perturb current grid point a distance delta along the current axis,
			// and calculate the resulting deformation gradients:
			m_gridVelocities( 3 * idx + dim ) = delta / TIME_STEP;
			updateDeformationGradients( dTest );
			
			// calculate the resulting energy:
			float e = calculateEnergy( dTest );
			
			// so force = -dE/dX = ( e0 - e ) / delta
			float f = ( e0 - e ) / delta;
			std::cerr << f << " == " << forces( 3 * idx + dim ) << "?  " << (3 * idx + dim) << " of " << forces.size() << std::endl;
		}
	}

	m_gridVelocities = originalGridVelocities;

}

void Grid::testForceDifferentials( const ParticleData& d )
{
	// calculate da forces brah!
	VectorXf forces( m_gridVelocities.size() );
	calculateForces( d, forces );

	// small random perturbation on the grid nodes:
	VectorXf dx( m_gridVelocities.size() );
	dx.setRandom();
	dx = dx * 0.01f;
	
	// calculate force differentials resulting from this perturbation:
	VectorXf forceDifferentials( m_gridVelocities.size() );
	calculateForceDifferentials( d, dx, forceDifferentials );
	
	// save the state so we don't screw the sim up:
	VectorXf originalGridVelocities = m_gridVelocities;
	ParticleData dTest = d;
	
	m_gridVelocities = dx / TIME_STEP;
	updateDeformationGradients( dTest );
	VectorXf perturbedForces( m_gridVelocities.size() );
	calculateForces( dTest, perturbedForces );
	
	VectorXf actualForceDifferentials = perturbedForces - forces;
	
	for( int i=0; i <forceDifferentials.size(); ++i )
	{
		#ifdef WIN32
		Sleep(100);
		#endif
		std::cerr << forceDifferentials[i] << " == " << actualForceDifferentials[i] << "? " << i << " of " << forceDifferentials.size() << std::endl;
	}
	
	m_gridVelocities = originalGridVelocities;
}

void Grid::updateGridVelocities( const ParticleData& d, const std::vector<CollisionObject*>& collisionObjects, const Solver& implicitSolver )
{
	m_prevGridVelocities = m_gridVelocities;

	// work out forces on grid points:
	VectorXf forces( m_gridVelocities.size() );
	calculateForces( d, forces );
	
	// work out forward velocity update - that's equation 10 in the paper:
	VectorXf forwardVelocities( m_gridVelocities.size() );
	m_nodeCollided.resize( m_gridMasses.size() );
	for( int i=0; i < m_nx; ++i )
	{
		for( int j=0; j < m_ny; ++j )
		{
			for( int k=0; k < m_nz; ++k )
			{
				int idx = coordsToIndex( i, j, k );
				m_nodeCollided[idx] = false;

				if( m_gridMasses[idx] == 0 )
				{
					forwardVelocities.segment<3>( 3 * idx ) = m_gridVelocities.segment<3>( 3 * idx );
				}
				else
				{
					Vector3f force = forces.segment<3>( 3 * idx );
					Vector3f velocity = m_gridVelocities.segment<3>( 3 * idx );
					Vector3f forwardVelocity = velocity + TIME_STEP * force / m_gridMasses[idx];

					// apply collisions:
					Vector3f x( GRID_H * i + m_xmin, GRID_H * j + m_ymin, GRID_H * k + m_zmin );
					for( size_t objIdx = 0; objIdx < collisionObjects.size(); ++objIdx )
					{
						float phi = collisionObjects[objIdx]->phi( x );
						if( phi <= 0 )
						{
							// intersecting the object
							Vector3f n;
							collisionObjects[objIdx]->grad( x, n );
							n.normalize();
							float nDotV = n.dot( forwardVelocity );
							if( nDotV < 0 )
							{
								// trying to move into the object:
								m_nodeCollided[idx] = true;

								// velocity perpendicular to the object
								Vector3f vPerp = nDotV * n;

								// remaining component is velocity paralell to the object:
								Vector3f vTangent = forwardVelocity - vPerp;

								forwardVelocity = vTangent * ( 1 + COULOMBFRICTION * nDotV / vTangent.norm() );
							}
						}
					}

					forwardVelocities.segment<3>( 3 * idx ) = forwardVelocity;
				}
			}
		}
	}
	
	for( int i=0; i < m_gridMasses.size(); ++i )
	{
		forwardVelocities.segment<3>( 3 * i ) *= m_gridMasses[i];
	}

	float tol_error = 1.e-7f;
	int iters = 30;

	implicitSolver(
		this,
		d,
		collisionObjects,
		forwardVelocities,
		m_gridVelocities );
	
}


void Grid::updateDeformationGradients( ParticleData& d )
{
	// little array with indexes going from -1 to store shape function derivative weights
	// on each dimension:
	DECLARE_WEIGHTARRAY( w );
	DECLARE_WEIGHTARRAY( dw );
	Vector3i particleCell;
	
	for( size_t p = 0; p < d.particleX.size(); ++p )
	{
		cellAndWeights( d.particleX[p], particleCell, w, dw );
		
		Vector3f v = Vector3f::Zero();
		Matrix3f delV = Matrix3f::Zero();
		for( int i=-1; i < 3; ++i )
		{
			for( int j=-1; j < 3; ++j )
			{
				for( int k=-1; k < 3; ++k )
				{
					int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
					Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
					Vector3f vSample = m_gridVelocities.segment<3>( 3 * idx );
					delV += vSample * weightGrad.transpose();
					v += vSample * w[0][i] * w[1][j] * w[2][k];
				}
			}
		}
		Matrix3f newParticleF = ( Matrix3f::Identity() + TIME_STEP * delV ) * d.particleF[p];
		d.particleF[p] = newParticleF;

		// find determinant and inverse transpose of deformation gradient:
		bool invertible;
		d.particleF[p].computeInverseAndDetWithCheck( d.particleFinvTrans[p], d.particleJ[p], invertible );
		if( invertible )
		{
			d.particleFinvTrans[p].transposeInPlace();
			
#ifdef PLASTICITY
			JacobiSVD<Matrix3f> svd(d.particleF[p], ComputeFullU | ComputeFullV );
			
			Vector3f singularValues = svd.singularValues();
			
			// apply plastic yeild:
			Matrix3f diagonalMat = Matrix3f::Zero();
			Matrix3f diagonalMatInv = Matrix3f::Zero();
			for( int i=0; i < 3; ++i )
			{
				// stretching:
				if( singularValues[i] > 1 + THETA_S )
				{
					singularValues[i] = 1 + THETA_S;
				}
				
				// compression:
				if( singularValues[i] < 1 - THETA_C )
				{
					singularValues[i] = 1 - THETA_C;
				}
				diagonalMat(i,i) = singularValues[i];
				diagonalMatInv(i,i) = 1.0f / singularValues[i];
			}
			
			d.particleF[p] = svd.matrixU() * diagonalMat * svd.matrixV().transpose();
			//d.particleR[p] = svd.matrixU() * svd.matrixV().transpose();
			//d.particleS[p] = svd.matrixV() * diagonalMat * svd.matrixV().transpose();
			
			d.particleFplastic[p] = svd.matrixV() * diagonalMatInv * svd.matrixU().transpose() * d.particleFplastic[p];
			
			// apply hardening:
			float hardeningFactor = exp( HARDENING * ( 1 - d.particleFplastic[p].determinant() ) );
			
			d.particleMu[p] = MU * hardeningFactor;
			d.particleLambda[p] = LAMBDA * hardeningFactor;

#endif

			// find polar decomposition of deformation gradient:
			Affine3f trans( d.particleF[p] );	
			trans.computeRotationScaling( &d.particleR[p], &d.particleS[p] );

		}
	}
}


inline float Grid::N( float x )
{
	float ax = fabs(x);
	if( ax < 1 )
	{
		return 0.5f * ax * ax * ax - ax * ax + 2.0f/3;
	}
	else if( ax < 2 )
	{
		return -ax * ax * ax / 6 + ax * ax - 2 * ax + 4.0f/3;
	}
	else
	{
		return 0;
	}
}

inline float Grid::DN( float x )
{
	if( x < 0 )
	{
		return -DN( -x );
	}
	
	if( x < 1 )
	{
		return x * ( 1.5f * x - 2 );
	}
	else if( x < 2 )
	{
		x -= 2;
		return -0.5f * x * x;
	}
	else
	{
		return 0;
	}
}

inline int Grid::coordsToIndex( int x, int y, int z ) const
{
	return x + m_nx * ( y + z * m_ny );
}

void Grid::cellAndWeights( const Vector3f& particleX, Vector3i& particleCell, float *w[], float** dw ) const
{
	Vector3f positionInCell;
	positionInCell[0] = ( particleX[0] - m_xmin ) / GRID_H;
	positionInCell[1] = ( particleX[1] - m_ymin ) / GRID_H;
	positionInCell[2] = ( particleX[2] - m_zmin ) / GRID_H;
	
	particleCell[0] = (int)floor( positionInCell[0] );
	particleCell[1] = (int)floor( positionInCell[1] );
	particleCell[2] = (int)floor( positionInCell[2] );
	
	positionInCell -= particleCell.cast<float>();
	if( dw )
	{
		for( int i=0; i < 3; ++i )
		{
			dw[i][-1] = DN( positionInCell[i] + 1 ) / GRID_H;
			dw[i][0] = DN( positionInCell[i] ) / GRID_H;
			dw[i][1] = DN( positionInCell[i] - 1 ) / GRID_H;
			dw[i][2] = DN( positionInCell[i] - 2 ) / GRID_H;
		}
	}
	
	for( int i=0; i < 3; ++i )
	{
		w[i][-1] = N( positionInCell[i] + 1 );
		w[i][0] = N( positionInCell[i] );
		w[i][1] = N( positionInCell[i] - 1 );
		w[i][2] = N( positionInCell[i] - 2 );
	}
	
}

inline void Grid::minMax( float x, float& min, float& max )
{
	if( x < min )
	{
		min = x;
	}
	if( x > max )
	{
		max = x;
	}
}

inline int Grid::fixDim( float& min, float& max )
{
	float minPadded = min - 1.5f * GRID_H;
	float maxPadded = max + 1.5f * GRID_H;
	int n = int( ceil( ( maxPadded - minPadded ) / GRID_H ) ) + 1;
	min = minPadded;
	max = min + n * GRID_H;
	return n;
}
