CMAKE_MINIMUM_REQUIRED ( VERSION 2.8.11 )

PROJECT ( mpmsnow )

ENABLE_TESTING()

FIND_PACKAGE ( HDK REQUIRED )
FIND_PACKAGE ( Eigen REQUIRED )
FIND_PACKAGE ( TBB REQUIRED )
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( GLUT REQUIRED )

IF ( NOT WIN32 )
  ADD_DEFINITIONS ( -fPIC -DOPENVDB_ENABLED )
ENDIF ()

INCLUDE_DIRECTORIES ( include )
INCLUDE_DIRECTORIES ( ${EIGEN_INCLUDE_DIR} )
INCLUDE_DIRECTORIES ( $ENV{HT}/include )

ADD_LIBRARY ( mpmsim STATIC
  src/CollisionObject.cpp
  src/MpmSim/CollisionPlane.cpp
  src/MpmSim/ConjugateResiduals.cpp
  src/MpmSim/CubicBsplineShapeFunction.cpp
  src/ForceField.cpp
  src/MpmSim/GravityField.cpp
  src/MpmSim/Grid.cpp
  src/MpmSim/MaterialPointData.cpp
  src/MpmSim/ShapeFunction.cpp
  src/MpmSim/Sim.cpp
  src/MpmSim/SnowConstitutiveModel.cpp
  src/MpmSim/SquareMagnitudeTermination.cpp
  )

SET ( MPM_OPENGL_LIBRARIES
  ${OPENGL_gl_LIBRARY}
  ${OPENGL_glu_LIBRARY}
  ${GLUT_Xi_LIBRARY}
  ${GLUT_Xmu_LIBRARY}
  ${GLUT_glut_LIBRARY}  
 )

SET ( MPM_REQUIRED_LIBRARIES
  mpmsim
  ${Tbb_TBBMALLOC_LIBRARY}
  ${Tbb_TBB_LIBRARY}
  ${MPM_OPENGL_LIBRARIES}
  )

ADD_EXECUTABLE ( simtest
  src/main.cpp
  src/CollisionObject.cpp
  src/ForceField.cpp
  )

TARGET_LINK_LIBRARIES ( simtest
  ${MPM_REQUIRED_LIBRARIES}
  )

ADD_EXECUTABLE ( unittest
  src/test.cpp
  src/tests/TestConjugateResiduals.cpp
  src/tests/TestGrid.cpp
  src/tests/TestShapeFunction.cpp
  src/tests/TestSimClass.cpp
  src/tests/TestSnowConstitutiveModel.cpp
  )

TARGET_LINK_LIBRARIES ( unittest
  mpmsim
  ${Tbb_TBBMALLOC_LIBRARY}
  ${Tbb_TBB_LIBRARY}
  ${OPENGL_gl_LIBRARY}
  ${OPENGL_glu_LIBRARY}
  )

HDK_ADD_LIBRARY ( SOP_MPMSim
  src/houdiniPlugin/HoudiniSolveTermination.cpp
  src/houdiniPlugin/SOP_MPMSim.cpp
  src/houdiniPlugin/VDBCollisionObject.cpp
  )

TARGET_LINK_LIBRARIES ( SOP_MPMSim
  mpmsim
  )

