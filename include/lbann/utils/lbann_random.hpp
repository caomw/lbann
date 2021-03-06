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

#ifndef LBANN_UTILS_RNG_HPP
#define LBANN_UTILS_RNG_HPP

#include "lbann/lbann_base.hpp"
#include <random>

namespace lbann {

typedef std::mt19937 rng_gen;  // Mersenne Twister

/**
 * Return a reference to the global LBANN random number generator.
 * @note If compiling with OpenMP, this is stored in a threadprivate variable.
 */
rng_gen& get_generator();

/**
 * Initialize the random number generator (with optional seed).
 * @todo Support saving/restoring the generator's state. This is directly
 * supported via the >> and << operators on the generator (reading/writing
 * from/to a stream).
 */
void init_random(int seed = -1);

/**
 * Make mat into an m x n matrix where each entry is independently drawn from
 * a Gaussian distribution with given mean and standard deviation.
 * Unless selected so at compile-time, this ensures the entries of the matrix do
 * not change as the grid it is distributed over changes; that is, it will have
 * the same entries when mat spans any number of processes.
 */
void gaussian_fill(ElMat& mat, El::Int m, El::Int n, DataType mean = 0.0f,
                   DataType stddev = 1.0f);
/**
 * Make mat into an m x n matrix where each entry is an indepenent Bernoulli
 * random variable with parameter p.
 * This makes the same gaurantees as gaussian_fill.
 */
void bernoulli_fill(ElMat& mat, El::Int m, El::Int n, double p = 0.5);
/**
 * Make mat into an m x n matrix where each entry is independently uniformly
 * sampled from a ball with the given center and radius.
 * This makes the same guarantees as gaussian_fill.
 */
void uniform_fill(ElMat& mat, El::Int m, El::Int n, DataType center = 0.0f,
                  DataType radius = 1.0f);

/**
 * Make mat into an m x n matrix where each entry is independently drawn from
 * a Gaussian distribution with given mean and standard deviation.
 * This always ensures that the entries of the matrix do not change as the grid
 * it is distributed over changes.
 */
void gaussian_fill_procdet(ElMat& mat, El::Int m, El::Int n,
                           DataType mean = 0.0f, DataType stddev = 1.0f);
/**
 * Make mat into an m x n matrix where each entry is an independent Bernoulli
 * random variable with parameter p.
 * This makes the same guarantees as gaussian_fill_procdet.
 */
void bernoulli_fill_procdet(ElMat& mat, El::Int m, El::Int n, double p = 0.5);
/**
 * Make mat into an m x n matrix where each entry is independently uniformly
 * sampled from a ball with the given center and radius.
 * This makes the same guarantees as gaussian_fill_procdet.
 */
void uniform_fill_procdet(ElMat& mat, El::Int m, El::Int n,
                          DataType center = 0.0f, DataType radius = 1.0f);

template<typename DistType,typename DType=DataType>
class rng {

  private:
    DistType m_dist; // Distribution type

  public:
  typename DistType::result_type gen() { return m_dist(get_generator()); }
    rng(){ }
    // bernoulli_distribution with prob p
  rng(DType p) : m_dist(p) {}
    // (uniform) real distribution between min/mean and max/stdev
  rng(DType a,DType b) : m_dist(a,b) {}
};

/** Multiply entries of distributed matrix  with
 * a multiplier generated according to bernoulli_distribution
 */
template <typename DType=DataType>
void rng_bernoulli(const float p, DistMat* m) {

  /// the scale for undropped inputs at training time given as @f$ 1 / (1 - p) @f$
  float scale = 1. / (1. - p);

  //@todo: use seed from parameter
  rng<std::bernoulli_distribution,DType> myrn(p); //magic seed?

  for (int row = 0; row < m->LocalHeight(); ++row) {
    for (int col = 0; col < m->LocalWidth(); ++col) {
      m->Set(row,col,myrn.gen()*scale); //SetLocal?
    }
  }
}


}// end namespace
#endif // LBANN_UTILS_RNG_HPP
