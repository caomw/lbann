////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#ifndef LBANN_LAYERS_TARGET_LAYER_UNSUPERVISED_INCLUDED
#define LBANN_LAYERS_TARGET_LAYER_UNSUPERVISED_HPP_INCLUDED

#include "lbann/layers/lbann_target_layer.hpp"
#include "lbann/io/lbann_distributed_minibatch_parallel_io.hpp"
#include "lbann/layers/lbann_input_layer_distributed_minibatch_parallel_io.hpp" //@generalize to base class

namespace lbann
{
  class target_layer_unsupervised : public target_layer, public distributed_minibatch_parallel_io {
  public:
    target_layer_unsupervised(lbann_comm* comm, int num_parallel_readers, uint mini_batch_size, std::map<execution_mode, DataReader*> data_readers, bool shared_data_reader);
    //target_layer_unsupervised(lbann_comm* comm, input_layer* in_layer);

    void setup(int num_prev_neurons);
    DataType forwardProp(DataType prev_WBL2NormSum);
    void backProp();
    execution_mode get_execution_mode();
    void set_input_layer(input_layer_distributed_minibatch_parallel_io*); //@todo replace with base layer class

  public:
    Mat* input_mat;
    CircMat* input_circmat;
    input_layer_distributed_minibatch_parallel_io* m_input_layer;

  protected:
    void fp_linearity(ElMat&, ElMat&, ElMat&, ElMat&) {}
    void bp_linearity() {}
  };
}

#endif  // LBANN_LAYERS_TARGET_LAYER_UNSUPERVISED_HPP_INCLUDED