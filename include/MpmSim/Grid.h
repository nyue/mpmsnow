#ifndef MPMSIM_GRID_H
#define MPMSIM_GRID_H

#include "tbb/enumerable_thread_specific.h"
#include "tbb/blocked_range.h"

#include "ShapeFunction.h"
#include "ParticleData.h"
#include "CollisionObject.h"
#include "LinearSolver.h"
#include "ConstitutiveModel.h"

#include <Eigen/Dense>

#define DECLARE_GRIDSPLATTER_CONSTRUCTOR( DerivedClass ) \
	DerivedClass( const Grid& g, const ParticleData& d, const ParticleData::PartitionList& partition, Eigen::VectorXf& result, const void* args ) \
		: Grid::GridSplatter( g, d, partition, result, args ) {}

namespace MpmSim
{

class Grid
{

public:

	Grid( const ParticleData& d, float timeStep, const ShapeFunction& m_shapeFunction, const ConstitutiveModel& model, int dimension = 3 );

	void draw() const;
	void computeDensities( ParticleData& d ) const;
 
	void updateGridVelocities(
		const ParticleData& d,
		const std::vector<CollisionObject*>& collisionObjects,
		const LinearSolver& implicitSolver );

	float updateDeformationGradients( ParticleData& d );
	void updateParticleVelocities( ParticleData& d, const std::vector<CollisionObject*>& collisionObjects );
	
	const Eigen::VectorXf& masses() const;

	const Eigen::VectorXf& getVelocities() const;
	void setVelocities( const Eigen::VectorXf& );

	const ConstitutiveModel& constitutiveModel() const;

	float gridH() const;

	void origin( Eigen::Vector3f& o ) const;

	class PointToGridIterator
	{
	public:
		
		PointToGridIterator( const Grid& g );
		
		void initialize( const Eigen::Vector3f& p, bool computeDerivatives = false );
		bool next();
		void gridPos( Eigen::Vector3i& pos ) const;
		void dw( Eigen::Vector3f& g ) const;
		float w() const;

	private:

		const Grid& m_grid;

		int m_diameter;
		
		std::vector<float> m_w[3];
		std::vector<float> m_dw[3];
		Eigen::Vector3i m_pos;
		Eigen::Vector3i m_base;
		bool m_gradients;
		
	};
	
	PointToGridIterator& pointIterator() const;
	
	friend class GridSplatter;
	class GridSplatter
	{
		public:
			GridSplatter(
				const Grid& g,
				const ParticleData& d,
				const ParticleData::PartitionList& partition,
				Eigen::VectorXf& result,
				const void* args
			);
			
			virtual ~GridSplatter() {}
			
			void operator()(const tbb::blocked_range<int> &r) const;
		
		protected:
			virtual void splat(
				ParticleData::IndexIterator begin,
				ParticleData::IndexIterator end,
				const Grid& g,
				const ParticleData& d,
				Eigen::VectorXf& result,
				const void* args ) const = 0;
			
		private:

			const Grid& m_g;
			const ParticleData& m_d;
			const ParticleData::PartitionList& m_partition;
			mutable Eigen::VectorXf& m_result;
			const void* m_args;

	};

	template <class Splatter>
	void splat( const ParticleData& d, Eigen::VectorXf& result, const void* args = 0 ) const;

	int coordsToIndex( const Eigen::Vector3i& pos ) const;

	// energy stored in the grid. Only used for testing...
	float calculateEnergy( const ParticleData& d ) const;
	
	// forces (due to energy derivatives)
	void calculateForces( const ParticleData& d, Eigen::VectorXf& forces ) const;

	// computes how the forces change when you perturb the grid nodes by dx:
	void calculateForceDifferentials( const ParticleData& d, const Eigen::VectorXf& dx, Eigen::VectorXf& df ) const;
	
	static bool collide( Eigen::Vector3f& v, const Eigen::Vector3f& x, const std::vector<CollisionObject*>& collisionObjects );
	
private:
	
	friend class ImplicitUpdateMatrix;

	// testing
	
	static inline void minMax( float x, float& min, float& max );
	inline int fixDim( float& min, float& max ) const;

	float m_gridH;
	float m_timeStep;
	
	Eigen::Vector3f m_min;
	Eigen::Vector3f m_max;
	Eigen::Vector3i m_n;
	
	int m_dimension;
	
	Eigen::VectorXf m_gridMasses;
	Eigen::VectorXf m_gridVelocities;
	Eigen::VectorXf m_prevGridVelocities;
	std::vector<bool> m_nodeCollided;

	const ShapeFunction& m_shapeFunction;
	const ConstitutiveModel& m_constitutiveModel;
	
	mutable tbb::enumerable_thread_specific< std::auto_ptr< PointToGridIterator > > m_pointIterators;

};

} // namespace MpmSim

#include "MpmSim/Grid.inl"

#endif // MPMSIM_GRID_H
