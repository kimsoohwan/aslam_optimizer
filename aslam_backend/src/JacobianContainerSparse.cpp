#include <aslam/backend/JacobianContainerSparse.hpp>
#include <sm/assert_macros.hpp>

namespace aslam {
  namespace backend {


    JacobianContainerSparse::~JacobianContainerSparse()
    {
    }

    /// \brief how many design variables does this jacobian container represent.
    size_t JacobianContainerSparse::numDesignVariables() const
    {
      return _jacobianMap.size();
    }


    JacobianContainerSparse::map_t::const_iterator JacobianContainerSparse::begin() const
    {
      return _jacobianMap.begin();
    }

    JacobianContainerSparse::map_t::const_iterator JacobianContainerSparse::end() const
    {
      return _jacobianMap.end();
    }

    JacobianContainerSparse::map_t::iterator JacobianContainerSparse::begin()
    {
      return _jacobianMap.begin();
    }

    JacobianContainerSparse::map_t::iterator JacobianContainerSparse::end()
    {
      return _jacobianMap.end();
    }


    /// \brief Apply the chain rule to the set of Jacobians.
    /// This may change the number of rows of this set of Jacobians
    /// by multiplying through by df_dx on the left.
    void JacobianContainerSparse::applyChainRule(const Eigen::MatrixXd& df_dx)
    {
      SM_ASSERT_EQ(Exception, df_dx.cols(), _rows, "Invalid matrix multiplication");
      map_t::iterator it = _jacobianMap.begin(),
                      it_end = _jacobianMap.end();
      for (; it != it_end; ++it)
        it->second = df_dx * it->second;
      _rows = df_dx.rows();
    }




    void JacobianContainerSparse::buildCorrelatedHessianBlock(const Eigen::VectorXd& e,
                                                        const Eigen::MatrixXd& J1, int j1_block,
                                                        SparseBlockMatrix& outHessian,
                                                        map_t::const_iterator it, map_t::const_iterator it_end) const
    {
      // Recursion base case.
      if (it == it_end)
        return;
      int j2_block = it->first->blockIndex();
      SM_ASSERT_LE_DBG(Exception, j1_block, j2_block, "The recursion should proceed in ordered blocks in order to only populate the upper diagonal of the Hessian. This violates the ordering.");
      const bool allocateIfMissing = true;
      Eigen::MatrixXd* J1t_invR_J2 = outHessian.block(j1_block, j2_block, allocateIfMissing);
      SM_ASSERT_TRUE_DBG(Exception, J1t_invR_J2 != NULL, "The Hessian block is NULL");
      SM_ASSERT_EQ_DBG(Exception, J1t_invR_J2->rows(), J1.cols(),
                       "The Hessian block has an unexpected number of rows. Block J1^T invR J2: (" <<
                       j1_block << ", " << j2_block << "). J1 is: " <<
                       J1.cols() << "x" << J1.rows() <<  ", J2 is: " <<
                       it->second.rows() << "x" << it->second.cols());
      SM_ASSERT_EQ_DBG(Exception, J1t_invR_J2->cols(), it->second.cols(),
                       "The Hessian block has an unexpected number of rows. Block J1^T invR J2: (" <<
                       j1_block << ", " << j2_block << "). J1 is: " <<
                       J1.cols() << "x" << J1.rows() <<  ", J2 is: " <<
                       it->second.rows() << "x" << it->second.cols());
      *J1t_invR_J2 += J1.transpose() * it->second;
      // Tail recursion
      buildCorrelatedHessianBlock(e, J1, j1_block, outHessian, ++it, it_end);
    }


    void JacobianContainerSparse::buildHessianBlock(const Eigen::VectorXd& e,
                                              SparseBlockMatrix& outHessian,
                                              Eigen::VectorXd& outRhs,
                                              map_t::const_iterator it, map_t::const_iterator it_end) const
    {
      // Recursion base case.
      if (it == it_end)
        return;
      const int j1_block = it->first->blockIndex();
      // \todo Make this a debug assert.
      SM_ASSERT_NE_DBG(Exception, j1_block, -1, "Negative blocks shouldn't make it in here");
      // templated version: outRhs.segment< J1::ColsAtCompileTime >.(j1_block);
      outRhs.segment(outHessian.rowBaseOfBlock(j1_block), it->second.cols()) -= it->second.transpose() * e;
      // Recurse in to the list to build correlated blocks
      buildCorrelatedHessianBlock(e, it->second, j1_block, outHessian, it, it_end);
      // Tail recursion to traverse the list
      buildHessianBlock(e, outHessian, outRhs, ++it, it_end);
    }

    void JacobianContainerSparse::evaluateHessian(const Eigen::VectorXd& e, const Eigen::MatrixXd& sqrtInvR, SparseBlockMatrix& outHessian, Eigen::VectorXd& outRhs) const
    {
      SM_ASSERT_EQ_DBG(Exception, e.size(), _rows, "The error and this Jacobian container should have the same size");
      SM_ASSERT_EQ_DBG(Exception, e.size(), sqrtInvR.rows(), "The error and the covariance matrix don't have compatible sizes");
      //SparseMatrixBlock * block = outHessian.block(int r, int c, allocIfMissing);
      // This sucks but I'll have to take a copy of the map to keep this function const.
      map_t mapCopy = _jacobianMap;
      // Scale each Jacobian.
      map_t::iterator itt = mapCopy.begin();
      for (; itt != mapCopy.end(); ++itt) {
        itt->second = (itt->first->scaling() * sqrtInvR.transpose() * itt->second).eval();
      }
      map_t::const_iterator it = mapCopy.begin();
      map_t::const_iterator it_end = mapCopy.end();
      // Start the recursion
      buildHessianBlock(sqrtInvR.transpose() * e, outHessian, outRhs, it, it_end);
    }

    bool JacobianContainerSparse::isFinite(const DesignVariable& dv) const
    {
      map_t::const_iterator it = _jacobianMap.find(const_cast<DesignVariable*>(&dv));
      SM_ASSERT_TRUE(Exception, it != _jacobianMap.end(), "The design variable does not exist in the container");
      return it->second.allFinite();
    }

    const Eigen::MatrixXd& JacobianContainerSparse::Jacobian(const DesignVariable* dv) const
    {
      map_t::const_iterator it = _jacobianMap.find(const_cast<DesignVariable* >(dv));
      SM_ASSERT_TRUE(Exception, it != _jacobianMap.end(), "The design variable does not exist in the container");
      return it->second;
    }

    /// \brief Get design variable i.
    DesignVariable* JacobianContainerSparse::designVariable(size_t i)
    {
      SM_ASSERT_LT(Exception, i, numDesignVariables(), "Index out of range");
      map_t::iterator it = _jacobianMap.begin();
      for (size_t ii = 0; ii < i; ++ii)
        it++;
      return it->first;
    }

    /// \brief Get design variable i.
    const DesignVariable* JacobianContainerSparse::designVariable(size_t i) const
    {
      SM_ASSERT_LT(Exception, i, numDesignVariables(), "Index out of range");
      map_t::const_iterator it = _jacobianMap.begin();
      for (size_t ii = 0; ii < i; ++ii)
        it++;
      return it->first;
    }

  void JacobianContainerSparse::reset(int rows) {
    clear();
    _rows = rows;
  }
  
    /// \brief Clear the contents of this container
    void JacobianContainerSparse::clear()
    {
      _jacobianMap.clear();
    }

    Eigen::MatrixXd JacobianContainerSparse::asDenseMatrix() const
    {
      // \todo make efficient
      return asSparseMatrix().toDense();
    }

    Eigen::MatrixXd JacobianContainerSparse::asDenseMatrix(const std::vector<int>& colBlockIndices) const
    {
      // \todo make efficient...however, these are only for debugging and unit testing.
      return asSparseMatrix(colBlockIndices).toDense();
    }

    SparseBlockMatrix JacobianContainerSparse::asSparseMatrix(const std::vector<int>& colBlockIndices) const
    {
      // \todo error checking.
      std::vector<int> rows(1);
      rows[0] = _rows;
      /// Step 2: fill the Jacobian
      SparseBlockMatrix J(rows, colBlockIndices, true);
      map_t::const_iterator it = _jacobianMap.begin();
      for (int col = 0; it != _jacobianMap.end(); ++it, col++) {
        const bool allocateBlock = true;
        SM_ASSERT_GE_LT_DBG(aslam::IndexOutOfBoundsException, (size_t)it->first->blockIndex(), 0, colBlockIndices.size(), "Block index is out of bounds");
        Eigen::MatrixXd& Ji = *J.block(0, it->first->blockIndex(), allocateBlock);
        Ji = it->second;
      }
      return J;
    }

    SparseBlockMatrix JacobianContainerSparse::asSparseMatrix() const
    {
      /// Step 1: Determine the block structure of the Jacobian.
      std::vector<int> rows(1);
      rows[0] = _rows;
      std::vector<int> cols(numDesignVariables());
      int sum = 0;
      map_t::const_iterator it = _jacobianMap.begin();
      for (int i = 0 ; it != _jacobianMap.end(); ++it, ++i) {
        sum += it->first->minimalDimensions();
        cols[i] = sum;
      }
      /// Step 2: fill the Jacobian
      SparseBlockMatrix J(rows, cols, true);
      it = _jacobianMap.begin();
      for (int col = 0; it != _jacobianMap.end(); ++it, col++) {
        const bool allocateBlock = true;
        Eigen::MatrixXd& Ji = *J.block(0, col, allocateBlock);
        Ji = it->second;
      }
      return J;
    }


    int JacobianContainerSparse::cols() const
    {
      int sum = 0;
      map_t::const_iterator it = _jacobianMap.begin();
      for (int i = 0 ; it != _jacobianMap.end(); ++it, ++i) {
        sum += it->first->minimalDimensions();
      }
      return sum;
    }

    template <typename Matrix>
    void JacobianContainerSparse::addImpl(DesignVariable* dv, const Matrix& Jacobian, const bool isIdentity)
    {
      SM_ASSERT_EQ(Exception, Jacobian.rows(), _rows, "The Jacobian must have the same number of rows as this container");
      SM_ASSERT_EQ(Exception, Jacobian.cols(), dv->minimalDimensions(), "The Jacobian must have the same number of cols as dv->minimalDimensions()");
      // If the design variable isn't active, don't bother adding it.
      if (! dv->isActive())
        return;
      SM_ASSERT_GE_DBG(Exception, dv->blockIndex(), 0, "The design variable is active but the block index is less than zero.");
      map_t::iterator it = _jacobianMap.find(dv);
      if (it == _jacobianMap.end()) {
        if (this->chainRuleEmpty()) // stack empty
          _jacobianMap.emplace(dv, Jacobian.template cast<double>());
        else
          if (!isIdentity) // TODO: cleanup
            _jacobianMap.emplace(dv, this->chainRuleMatrix()*Jacobian.template cast<double>());
          else
            _jacobianMap.emplace(dv, this->chainRuleMatrix());
      } else {
        SM_ASSERT_TRUE_DBG(Exception, it->first == dv, "Two design variables had the same block index but different pointer values");
        if (this->chainRuleEmpty()) // stack empty
          it->second.noalias() += Jacobian.template cast<double>();
        else {
          if (!isIdentity) // TODO: cleanup
            it->second.noalias() += this->chainRuleMatrix()*Jacobian.template cast<double>();
          else
            it->second.noalias() += this->chainRuleMatrix();
        }
      }
    }

    void JacobianContainerSparse::add(DesignVariable* dv, const Eigen::Ref<const Eigen::MatrixXd>& Jacobian)
    {
      this->addImpl(dv, Jacobian, false);
    }

    void JacobianContainerSparse::add(DesignVariable* designVariable)
    {
      this->addImpl(designVariable, Eigen::MatrixXd::Identity(this->rows(), designVariable->minimalDimensions()), true);
    }

  } // namespace backend
} // namespace aslam
