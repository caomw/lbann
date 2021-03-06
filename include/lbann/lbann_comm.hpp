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
// lbann_comm .hpp .cpp - LBANN communication utilities
////////////////////////////////////////////////////////////////////////////////

#ifndef LBANN_COMM_HPP_INCLUDED
#define LBANN_COMM_HPP_INCLUDED

#include <vector>
#include "lbann_base.hpp"
using namespace El;

namespace lbann
{

  template <typename T>
  using lbann_mpi_req = mpi::Request<T>;

  /**
   * Manage communication.
   * This supports separate models, each of which are split over potentially
   * several processes. Every model is split over the same number of processes.
   * The corresponding processes between models are on the "inter-model
   * communicator".
   * You can also do point-to-point or broadcast communication to arbitrary sets
   * of processes.
   */
  class lbann_comm
  {
  public:
    /**
     * Init communicators for models each with procs_per_model processes,
     * defaulting to every process in one model.
     */
    lbann_comm(int procs_per_model = 0);
    ~lbann_comm();

    /** Get which model this process is in. */
    inline int get_model_rank() const { return model_rank; }
    /** Get the rank of this process in its model. */
    inline int get_rank_in_model() const { return rank_in_model; }
    /** Get my rank in COMM_WORLD. */
    inline int get_rank_in_world() const { return mpi::Rank(mpi::COMM_WORLD); }
    /** Return the COMM_WORLD rank of the rank'th processor in model. */
    inline int get_world_rank(int model, int rank) const {
      return procs_per_model * model + rank;
    }
    /** Return the rank of the master process in this model. */
    inline int get_model_master() const { return 0; }
    /** Return the rank of the inter-model master process. */
    inline int get_intermodel_master() const { return 0; }
    /** Return the rank of the world master process. */
    inline int get_world_master() const { return 0; }
    /** Return true if this process is the master process in its model. */
    inline bool am_model_master() const {
      return get_rank_in_model() == get_model_master();
    }
    /** Return true if this process is the world master process. */
    inline bool am_world_master() const {
      return get_rank_in_world() == get_world_master();
    }
    /** Return a grid to use for this model. */
    inline Grid& get_model_grid() { return *grid; }
    /** Return the total number of models. */
    inline int get_num_models() const { return num_models; }
    /* Return the number of processes in a model. */
    inline int get_procs_per_model() const { return procs_per_model; }
    /** Return the number of processes in a compute node. */
    inline int get_procs_per_node() const { return procs_per_node; }
    /** Return the rank of this process within its compute node. */
    inline int get_rank_in_node() const { return rank_in_node; }

    /** Perform a sum reduction of mat over the inter-model communicator. */
    void intermodel_sum_matrix(Mat& mat);
    void intermodel_sum_matrix(DistMat& mat);
    /** Non-blocking intermodel_sum_matrix. */
    //void nb_intermodel_sum_matrix(Mat& mat, mpi::Request& req);
    //void nb_intermodel_sum_matrix(DistMat& mat, mpi::Request& req);
    /** Broadcast mat over the inter-model communicator starting from root. */
    void intermodel_broadcast_matrix(Mat& mat, int root);
    void intermodel_broadcast_matrix(DistMat& mat, int root);
    /** Non-blocking intermodel_broadcast_matrix. */
    //void nb_intermodel_broadcast_matrix(Mat& mat, int root, mpi::Request& req);
    //void nb_intermodel_broadcast_matrix(DistMat& mat, int root,
    //                                    mpi::Request& req);
    /**
     * Inter-model broadcast, returns the broadcast value.
     * Root process specifies root and val, other processes just root.
     */
    template <typename T>
    T intermodel_broadcast(int root, T val = {}) {
      mpi::Broadcast(&val, 1, root, intermodel_comm);
      if (get_rank_in_model() == root) {
        bytes_sent += sizeof(T);
      } else {
        bytes_received += sizeof(T);
      }
      return val;
    }
    /**
     * Within-model broadcast, returns the broadcast value.
     * Root process specifies root and val, other processes just root.
     */
    template <typename T>
    T model_broadcast(int root, T val = {}) {
      mpi::Broadcast(&val, 1, root, model_comm);
      if (get_rank_in_model() == root) {
        bytes_sent += sizeof(T);
      } else {
        bytes_received += sizeof(T);
      }
      return val;
    }
    /** Inter-model gather (for non-root processes). */
    template <typename T>
    void intermodel_gather(T send, int root) {
      bytes_sent += sizeof(T);
      mpi::Gather(&send, 1, (T*) NULL, 0, root, intermodel_comm);
    }
    /** Inter-model gather (for root processes). */
    template <typename T>
    void intermodel_gather(T send, std::vector<T>& recv) {
      mpi::Gather(&send, 1, recv.data(), 1, get_model_rank(),
                  intermodel_comm);
      bytes_received += sizeof(T) * (get_num_models() - 1);
    }
    /** Inter-model scalar-array gather (for non-root processes). */
    template <typename T>
    void intermodel_gather(T* send, int count, int root) {
      bytes_sent += sizeof(T) * count;
      mpi::Gather(send, count, (T*) NULL, 0, root, intermodel_comm);
    }
    /** Inter-model scalar-array gather (for root processes). */
    template <typename T>
    void intermodel_gather(T* send, int count, T* recv) {
      mpi::Gather(send, count, recv, count, get_model_rank(), intermodel_comm);
      bytes_received += sizeof(T) * count * (get_num_models() - 1);
    }
    /** Inter-model reduce (for non-root processes). */
    template <typename T>
    void intermodel_reduce(T send, int root, mpi::Op op = mpi::SUM) {
      bytes_sent += sizeof(T);
      mpi::Reduce(&send, (T*) NULL, 0, op, root, intermodel_comm);
    }
    /** Inter-model reduce (for root processes). */
    template <typename T>
    T intermodel_reduce(T send, mpi::Op op = mpi::SUM) {
      T val;
      mpi::Reduce(&send, &val, 1, op, get_model_rank(),
                  intermodel_comm);
      bytes_received += sizeof(T) * (get_num_models() - 1);
      return val;
    }
    /** Within-model reduce (for non-root processes). */
    template <typename T>
    void model_reduce(T send, int root, mpi::Op op = mpi::SUM) {
      bytes_sent += sizeof(T);
      mpi::Reduce(&send, (T*) NULL, 1, op, root, model_comm);
    }
    /** Within-model reduce (for root processes). */
    template <typename T>
    T model_reduce(T send, mpi::Op op = mpi::SUM) {
      T val;
      mpi::Reduce(&send, &val, 1, op, get_rank_in_model(), model_comm);
      bytes_received += sizeof(T) * (get_procs_per_model() - 1);
      return val;
    }
    /** Within-model scalar array reduce (for non-root processes). */
    template <typename T>
    void model_reduce(T* send, int count, int root, mpi::Op op = mpi::SUM) {
      bytes_sent += sizeof(T) * count;
      mpi::Reduce(send, (T*) NULL, count, op, root, model_comm);
    }
    /** Within-model scalar array reduce (for root processes). */
    template <typename T>
    void model_reduce(T* send, int count, T* recv, mpi::Op op = mpi::SUM) {
      mpi::Reduce(send, recv, count, op, get_rank_in_model(), model_comm);
      bytes_received += sizeof(T) * count * (get_procs_per_model() - 1);
    }
    /** Within-model all-reduce. */
    template <typename T>
    T model_allreduce(T send, mpi::Op op = mpi::SUM) {
      T val;
      bytes_sent += sizeof(T);
      mpi::AllReduce(&send, &val, 1, op, model_comm);
      bytes_received += sizeof(T) * (get_procs_per_model() - 1);
      return val;
    }
    /** Scalar array within-model all-reduce. */
    template <typename T>
    void model_allreduce(T* send, int count, T* recv, mpi::Op op = mpi::SUM) {
      bytes_sent += count * sizeof(T);
      mpi::AllReduce(send, recv, count, op, model_comm);
      bytes_received += count * sizeof(T) * (get_procs_per_model() - 1);
    }

    /** Wait for a non-blocking request to complete. */
    template <typename T>
    void wait(lbann_mpi_req<T>& req) {
      mpi::Wait(req);
    }

    /** Barrier among the inter-model processes. */
    void intermodel_barrier();
    /** Barrier among processes in this model. */
    void model_barrier();
    /** Barrier among all processes. */
    void global_barrier();

    /** Send a buffer to rank in model. */
    template <typename T>
    void send(const T* data, int count, int model, int rank) {
      bytes_sent += sizeof(T) * count;
      mpi::Send(data, count, get_world_rank(model, rank), mpi::COMM_WORLD);
    }
    template <typename T> void send(const T* data, int count, int model) {
      send(data, count, model, rank_in_model);
    }
    void send(Mat& mat, int model, int rank);
    void send(DistMat& mat, int model, int rank);
    void send(Mat& mat, int model) { send(mat, model, rank_in_model); }
    void send(DistMat& mat, int model) { send(mat, model, rank_in_model); }

    /** Corresponding non-blocking sends. */
    template <typename T>
    void nb_send(const T* data, int count, int model, int rank,
                 lbann_mpi_req<T>& req) {
      bytes_sent += sizeof(T) * count;
      mpi::ISend(data, count, get_world_rank(model, rank), mpi::COMM_WORLD, req);
    }
    template <typename T> void nb_send(const T* data, int count, int model,
                                       lbann_mpi_req<T>& req) {
      nb_send(data, count, model, rank_in_model, req);
    }
    void nb_send(Mat& mat, int model, int rank, lbann_mpi_req<DataType>& req);
    void nb_send(DistMat& mat, int model, int rank, lbann_mpi_req<DataType>& req);
    void nb_send(Mat& mat, int model, lbann_mpi_req<DataType>& req) {
      nb_send(mat, model, rank_in_model, req);
    }
    void nb_send(DistMat& mat, int model, lbann_mpi_req<DataType>& req) {
      nb_send(mat, model, rank_in_model, req);
    }

    /** Corresponding receive to send. */
    template <typename T> void recv(T* data, int count, int model, int rank) {
      mpi::Recv(data, count, get_world_rank(model, rank), mpi::COMM_WORLD);
      bytes_received += sizeof(T) * count;
    }
    template <typename T> void recv(T* data, int count, int model) {
      recv(data, count, model, rank_in_model);
    }
    void recv(Mat& mat, int model, int rank);
    void recv(DistMat& mat, int model, int rank);
    void recv(Mat& mat, int model) { recv(mat, model, rank_in_model); }
    void recv(DistMat& mat, int model) { recv(mat, model, rank_in_model); }
    /** As above, but receive from anyone. */
    template <typename T> void recv(T* data, int count) {
      mpi::Recv(data, count, mpi::ANY_SOURCE, mpi::COMM_WORLD);
      bytes_received += sizeof(T) * count;
    }
    void recv(Mat& mat);
    void recv(DistMat& mat);

    /** Corresponding non-blocking receives. */
    template <typename T> void nb_recv(T* data, int count, int model, int rank,
                                       lbann_mpi_req<T>& req) {
      mpi::IRecv(data, count, get_world_rank(model, rank), mpi::COMM_WORLD,
                 req);
      bytes_received += sizeof(T) * count;
    }
    template <typename T> void nb_recv(T* data, int count, int model,
                                       lbann_mpi_req<T>& req) {
      recv(data, count, model, rank_in_model, req);
    }
    void nb_recv(Mat& mat, int model, int rank, lbann_mpi_req<DataType>& req);
    void nb_recv(DistMat& mat, int model, int rank, lbann_mpi_req<DataType>& req);
    void nb_recv(Mat& mat, int model, lbann_mpi_req<DataType>& req) {
      nb_recv(mat, model, rank_in_model, req);
    }
    void nb_recv(DistMat& mat, int model, lbann_mpi_req<DataType>& req) {
      nb_recv(mat, model, rank_in_model, req);
    }
    template <typename T> void nb_recv(T* data, int count, lbann_mpi_req<T>& req) {
      mpi::IRecv(data, count, mpi::ANY_SOURCE, mpi::COMM_WORLD, req);
      bytes_received += sizeof(T) * count;
    }
    void nb_recv(Mat& mat, lbann_mpi_req<DataType>& req);
    void nb_recv(DistMat& mat, lbann_mpi_req<DataType>& req);

    /** Determine the size (count) of an incoming message. */
    template <typename T> int get_count(int model, int rank) {
      MPI_Status status;
      MPI_Probe(get_world_rank(model, rank), MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      return mpi::GetCount<T>(status);
    }
    template <typename T>
    int get_count(int model) { return get_count<T>(model, rank_in_model); }

    /**
     * Broadcast data to the ranks in dests, beginning from root.
     * @todo Can probably optimize this.
     */
    template <typename T>
    void broadcast(T* data, int count, std::vector<int>& dests, int root) {
      mpi::Group bcast_group;
      mpi::Comm bcast_comm;
      std::vector<int> ranks;
      ranks.push_back(root);
      ranks.insert(ranks.end(), dests.begin(), dests.end());
      create_group(ranks, bcast_group);
      // Elemental doesn't expose this, so we have to reach into its internals.
      // This lets us create a communicator without involving all of COMM_WORLD.
      // Use a tag of 0; should not matter unless we're multi-threaded.
      MPI_Comm_create_group(mpi::COMM_WORLD.comm, bcast_group.group, 0,
                            &(bcast_comm.comm));
      int translated_root = mpi::Translate(mpi::COMM_WORLD, root, bcast_comm);
      mpi::Broadcast(data, count, translated_root, bcast_comm);
      mpi::Free(bcast_comm);
      mpi::Free(bcast_group);
    }
    void broadcast(Mat& mat, std::vector<int>& dests, int root);
    void broadcast(DistMat& mat, std::vector<int>& dests, int root);

    // Statistics methods.
    /** Return the number of model barriers performed. */
    inline size_t get_num_model_barriers() const { return num_model_barriers; }
    /** Return the number of inter-model barriers performed. */
    inline size_t get_num_intermodel_barriers() const { return num_intermodel_barriers; }
    /** Return the number of global barriers performed. */
    inline size_t get_num_global_barriers() const { return num_global_barriers; }
    /** Return the number of bytes sent. */
    inline size_t get_bytes_sent() const { return bytes_sent; }
    /** Return the number of bytes received. */
    inline size_t get_bytes_received() const { return bytes_received; }
    inline void reset_stats_counters() {
      num_model_barriers = 0;
      num_intermodel_barriers = 0;
      num_global_barriers = 0;
      bytes_sent = 0;
      bytes_received = 0;
    }
  private:
    /** Communicator for every process in this model. */
    mpi::Comm model_comm;
    /** Communicator for every process with the same model rank. */
    mpi::Comm intermodel_comm;
    /** Communicator for every process in the same compute node. */
    mpi::Comm node_comm;
    /** Grid for this model. */
    Grid* grid;
    /** Number of models. */
    int num_models;
    /** Number of processors per model. */
    int procs_per_model;
    /** Rank of the model this process is in. */
    int model_rank;
    /** Rank of this process within its model. */
    int rank_in_model;
    /** Number of processers per compute node. */
    int procs_per_node;
    /** Rank of this process within its compute node. */
    int rank_in_node;
    
    // Various statistics counters.
    size_t num_model_barriers;
    size_t num_intermodel_barriers;
    size_t num_global_barriers;
    size_t bytes_sent;
    size_t bytes_received;

    /** MPI tag for point-to-point communication. (Unused) */
    static const int PT2PT_TAG = 42;
    /** Create a new group from a list of ranks. (Needs to be freed.) */
    inline void create_group(std::vector<int>& ranks, mpi::Group& g) {
      mpi::Group world_group;
      mpi::CommGroup(mpi::COMM_WORLD, world_group);
      mpi::Incl(world_group, (int) ranks.size(), ranks.data(), g);
    }

    /** Setup communicator for processes in the same compute node.
     *  We obtain a string specifying the compute node. The string is
     *  hashed (with salt) and used to split the communicators. To
     *  avoid hash collisions, the splitting procedure is repeated
     *  with a different salt. */
    void setup_node_comm();
    
  };
}

#endif  // LBANN_COMM_HPP_INCLUDED
