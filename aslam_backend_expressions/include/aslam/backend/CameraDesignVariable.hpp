#ifndef ASLAM_BACKEND_CAMERA_VARIABLE_HPP
#define ASLAM_BACKEND_CAMERA_VARIABLE_HPP

#include <aslam/backend/DesignVariable.hpp>
#include <aslam/backend/DesignVariableAdapter.hpp>
#include <aslam/backend/JacobianContainer.hpp>


namespace aslam {
	namespace backend {

	template<typename CAMERA_T>
	class CameraDesignVariable
	{
	public:

		typedef CAMERA_T camera_t;
		typedef typename camera_t::keypoint_t keypoint_t;
		typedef typename camera_t::projection_t projection_t;
		typedef typename camera_t::projection_t::distortion_t distortion_t;
		typedef typename camera_t::shutter_t shutter_t;

		CameraDesignVariable(const boost::shared_ptr<camera_t> & camera);
		~CameraDesignVariable();


		Eigen::VectorXd euclideanToKeypoint(Eigen::Vector3d p);
		Eigen::VectorXd homogeneousToKeypoint(Eigen::Vector4d ph);

		void evaluateJacobians(JacobianContainer & outJacobians, Eigen::Vector4d ph) const;
		void evaluateJacobians(JacobianContainer & outJacobians, const Eigen::MatrixXd & applyChainRule, Eigen::Vector4d ph) const;

		void getDesignVariables(DesignVariable::set_t & designVariables) const;

		void setActive(bool p, bool d, bool s);

		void setTestBlockIndices() { _projectionDv->setBlockIndex(0); _distortionDv->setBlockIndex(1); _shutterDv->setBlockIndex(2); };

		boost::shared_ptr<DesignVariableAdapter<projection_t> > projectionDesignVariable() { return _projectionDv; }
		boost::shared_ptr<DesignVariableAdapter<distortion_t> > distortionDesignVariable()  { return _distortionDv; }
		boost::shared_ptr<DesignVariableAdapter<shutter_t> > shutterDesignVariable()  { return _shutterDv; }

		boost::shared_ptr<camera_t> camera() { return _camera; }

	private:
		boost::shared_ptr<camera_t> _camera;

		boost::shared_ptr<DesignVariableAdapter<projection_t> > _projectionDv;
		boost::shared_ptr<DesignVariableAdapter<distortion_t> > _distortionDv;
		boost::shared_ptr<DesignVariableAdapter<shutter_t> > _shutterDv;

	};


	}	// backend
}	// aslam

#include "implementation/CameraDesignVariable.hpp"


#endif /* ASLAM_BACKEND_CAMERA_VARIABLE_HPP */
