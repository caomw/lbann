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
//
// lbann_data_reader .hpp - Input data base class for training, testing
////////////////////////////////////////////////////////////////////////////////

#ifndef LBANN_DATA_READER_HPP
#define LBANN_DATA_READER_HPP

#include "lbann/lbann_base.hpp"
#include "lbann/utils/lbann_random.hpp"
#include "lbann/utils/lbann_exception.hpp"
#include "lbann/lbann_comm.hpp"
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>


/**
 * @todo - add support for save and restore
 */
namespace lbann
{
	class DataReader
	{
	public:
    DataReader(int batchSize, bool shuffle) :
      BatchSize(batchSize), CurrentPos(0), m_shuffle(shuffle),
      m_stride(batchSize), m_base_offset(0), m_model_offset(0), 
      m_use_alt_last_mini_batch_size(false),
      m_last_mini_batch_threshold(0), m_last_mini_batch_size(batchSize), m_last_mini_batch_stride(0) 
    {}
    DataReader(int batchSize) :
      DataReader(batchSize, true) {}
    
    DataReader(const DataReader& source) :
      BatchSize(source.BatchSize), CurrentPos(source.CurrentPos), m_shuffle(source.m_shuffle),
      m_stride(source.m_stride), m_base_offset(source.m_base_offset), m_model_offset(source.m_model_offset),
      m_use_alt_last_mini_batch_size(source.m_use_alt_last_mini_batch_size),
      m_last_mini_batch_threshold(source.m_last_mini_batch_threshold), m_last_mini_batch_size(source.m_last_mini_batch_size), m_last_mini_batch_stride(source.m_last_mini_batch_stride),
      ShuffledIndices(source.ShuffledIndices), m_unused_indices(source.m_unused_indices)
    {}

    virtual ~DataReader() {}

    /**
     * Prepare to start processing an epoch of data.
     * If shuffle is true, then shuffle the indices of the data set
     * If the base offset is not specified set it to 0
     * If the stride is not specified set it to batch size
     */
    void setup(int base_offset, int stride, int model_offset = 0, lbann_comm *comm = NULL) {
      m_model_offset = model_offset;
      m_base_offset = base_offset;
      m_stride = stride;

      if(comm != NULL) {
        calculate_multi_model_data_distribution(comm);
        m_use_alt_last_mini_batch_size = true;
      }

      CurrentPos = m_base_offset + m_model_offset;
      if (m_shuffle) {
        std::shuffle(ShuffledIndices.begin(), ShuffledIndices.end(),
                     get_generator());
      }
    }

    void setup() { DataReader::setup(0, BatchSize); }

		virtual int fetch_data(Mat& X) { return 0; }
		// 	if (CurrentPos < (int)ShuffledIndices.size()) {
		// 		return true;
		// 	}
		// 	else {
		// 		return false;
		// 	}
		// }
		virtual int fetch_label(Mat& Y) { return 0; }
		// 	if (CurrentPos < (int)ShuffledIndices.size()) {
		// 		return true;
		// 	}
		// 	else {
		// 		return false;
		// 	}
		// }
    /**
     * During the network's update phase, the data reader will
     * advanced the current position pointer.  If the pointer wraps
     * around, then reshuffle the data indicies.
     */
    virtual bool update() {
      if(m_use_alt_last_mini_batch_size && CurrentPos+m_stride > m_last_mini_batch_threshold) {
        CurrentPos += m_last_mini_batch_stride;
      }else {
        CurrentPos += m_stride;
      }
      if (CurrentPos < (int)ShuffledIndices.size()) {
        return true;
      } else {
        if (m_shuffle) {
          std::shuffle(ShuffledIndices.begin(), ShuffledIndices.end(),
                       get_generator());
        }
        CurrentPos = m_base_offset + m_model_offset;
        return false;
      }
    }

		virtual int getNumLabels() { return 0; }
    virtual int get_linearized_data_size() { return 0; }
    virtual int get_linearized_label_size() { return 0; }

    bool position_valid()   { return (CurrentPos < (int)ShuffledIndices.size()); }
    int getBatchSize()      { 
      if(m_use_alt_last_mini_batch_size && CurrentPos >= m_last_mini_batch_threshold) {
        return m_last_mini_batch_size;
      }else {
        return BatchSize; 
      }
    }
		int getPosition()       { return CurrentPos; }
    int get_next_position() { 
      if(m_use_alt_last_mini_batch_size && CurrentPos+m_stride > m_last_mini_batch_threshold) {
        return CurrentPos + m_last_mini_batch_stride;
      }else {
        return CurrentPos + m_stride;
      }
    }
		int* getIndices()       { return &ShuffledIndices[0]; }
		int getNumData()        { return (int)ShuffledIndices.size(); }
		int get_num_unused_data() { return (int)m_unused_indices.size(); }
		int* get_unused_data()    { return &m_unused_indices[0]; }

    void select_subset_of_data(size_t max_sample_count, bool firstN) {
      size_t num_data_samples = getNumData();
      
      /// If the user requested fewer than the total data set size, select
      /// a random set from the entire data set.
      if (max_sample_count != 0) {
        max_sample_count = __MIN(max_sample_count, num_data_samples);
        if(!firstN) {
          std::shuffle(ShuffledIndices.begin(), ShuffledIndices.end(), get_generator());
        }
        m_unused_indices=std::vector<int>(ShuffledIndices.begin() + max_sample_count, ShuffledIndices.end());
        ShuffledIndices.resize(max_sample_count);

        if(!firstN) {
          std::sort(ShuffledIndices.begin(), ShuffledIndices.end());
          std::sort(m_unused_indices.begin(), m_unused_indices.end());
        }

        // std::cout << "shuffled indices ";
        // for (auto i = ShuffledIndices.begin(); i != ShuffledIndices.end(); ++i)
        //   std::cout << *i << ' ';
        // std::cout << std::endl;

        // std::cout << "unused indices ";
        // for (auto i = m_unused_indices.begin(); i != m_unused_indices.end(); ++i)
        //   std::cout << *i << ' ';
        // std::cout << std::endl;
      }
    }

    bool swap_used_and_unused_index_sets() {
      std::vector<int> tmp_indices = ShuffledIndices;
      ShuffledIndices = m_unused_indices;
      m_unused_indices = tmp_indices;
      return true;
    }

    DataReader& operator=(const DataReader& source) {
      this->BatchSize = source.BatchSize;
      this->CurrentPos = source.CurrentPos;
      this->m_shuffle = source.m_shuffle;
      this->m_stride = source.m_stride;
      this->m_base_offset = source.m_base_offset;
      this->m_model_offset = source.m_model_offset;
      this->m_use_alt_last_mini_batch_size = source.m_use_alt_last_mini_batch_size;
      this->m_last_mini_batch_threshold = source.m_last_mini_batch_threshold;
      this->m_last_mini_batch_size = source.m_last_mini_batch_size;
      this->m_last_mini_batch_stride = source.m_last_mini_batch_stride;

      // Vectors implement a deep copy
      this->ShuffledIndices = source.ShuffledIndices;
      this->m_unused_indices = source.m_unused_indices;
      return *this;
    }

    size_t trim_data_set(double use_percentage, bool firstN=false) {
      size_t max_sample_count = rint(getNumData()*use_percentage);
      
      if(max_sample_count > getNumData() || ((long) max_sample_count) < 0) {
        throw lbann_exception("data reader trim error: invalid number of samples selected");
      }
      select_subset_of_data(max_sample_count, firstN);

      return getNumData();
    }

    void calculate_multi_model_data_distribution(lbann_comm *comm) {
      int max_mini_batch_size = BatchSize;
      int num_parallel_readers_per_model = (m_stride / comm->get_num_models()) / max_mini_batch_size;

      int num_whole_mini_batches = rint(getNumData() / m_stride);
      int partial_mini_batch_size = (getNumData() - (num_whole_mini_batches*m_stride))/(comm->get_num_models() * num_parallel_readers_per_model);
      int world_master_remainder_data = 0;

      int world_master_remainder_adjustment = getNumData() 
        - (num_whole_mini_batches * m_stride) 
        - (partial_mini_batch_size * comm->get_num_models() * num_parallel_readers_per_model);
      if(comm->am_world_master()) {
        world_master_remainder_data = world_master_remainder_adjustment;
        world_master_remainder_adjustment = 0;
      }
      partial_mini_batch_size += world_master_remainder_data;

      m_last_mini_batch_threshold = m_stride * num_whole_mini_batches;
      m_last_mini_batch_size = partial_mini_batch_size;

      /// Note that comm->get_model_rank() + comm->get_rank_in_model() is not equivalent to comm->get_world_rank() from a parallel I/O perspective
      /// Given the data readers rank, how many readers have a higher rank
      int num_readers_at_full_stride = (comm->get_num_models() - comm->get_model_rank()) * num_parallel_readers_per_model;
      /// Given the data readers rank, how many readers have a lower rank
      int num_readers_at_last_stride = comm->get_model_rank() * num_parallel_readers_per_model;
      if(comm->get_rank_in_model() < num_parallel_readers_per_model) { /// If this rank is one of the readers, adjust the number of readers to account for that
        num_readers_at_full_stride -= comm->get_rank_in_model();
        num_readers_at_last_stride += comm->get_rank_in_model();
      }
      /// Compute how big the stride should be assuming that each higher ranked parallel reader has completed a full mini-batch
      /// and each lower ranked parallel reader has completed a partial mini-batch
      m_last_mini_batch_stride = max_mini_batch_size * num_readers_at_full_stride
        + (partial_mini_batch_size * (num_readers_at_last_stride)) + world_master_remainder_adjustment;

      //      cout << "[" << comm->get_rank_in_world() << "] " << comm->get_model_rank() << " model rank, num_whole_mini_batches=" << num_whole_mini_batches << " partial_mini_batch_size=" << partial_mini_batch_size << " world_master_remainder_data=" << world_master_remainder_data << " threshold " << m_last_mini_batch_threshold << " with a last stride of " << m_last_mini_batch_stride << " and stride of " << m_stride << " and there are " << num_parallel_readers_per_model << " parallel readers per model " <<endl;

      return;
    }

  protected:
    int							BatchSize;
    int 						CurrentPos;
    int             m_shuffle;
    int             m_stride;       /// Stride is typically batch_size, but may be a multiple of batch size if there are multiple readers
    int             m_base_offset;  /// If there are multiple instances of the reader, 
                                    /// then it may not reset to zero
    int             m_model_offset;  /// If there are multiple models with multiple instances of the reader, 
                                     /// each model's set of readers may not reset to zero
    /// Provide a set of size, strides, and thresholds to handle the last mini batch of a dataset
    bool            m_use_alt_last_mini_batch_size;
    int             m_last_mini_batch_threshold;
    int             m_last_mini_batch_size;
    int             m_last_mini_batch_stride;

    std::vector<int> 			ShuffledIndices;
    std::vector<int> 			m_unused_indices; /// Record of the indicies that are not being used for training
	};

}

#endif // LBANN_DATA_READER_HPP
