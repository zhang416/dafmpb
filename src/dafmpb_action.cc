//=============================================================================
// DAFMPB: DASHMM Accelerated Adaptive Fast Multipole Poisson-Boltzmann Solver 
//
// Portions Copyright (c) 2014, Institute of Computational Mathematics, CAS
// Portions Copyright (c) 2014, Oak Ridge National Laboratory
// Portions Copyright (c) 2017, Trustees of Indiana University,
//
// All rights reserved.
//
// This program is a free software; you can redistribute it and/or modify it
// uner the terms of the GNU General Public License version 3 as published by 
// the Free Software Foundation. 
//=============================================================================

#include <chrono> 
#include <cassert>
#include "dafmpb.h"
#include "dashmm/arraymetadata.h"

namespace dafmpb {

void sum_ident_handler(double *input, const size_t bytes) {
  size_t count = bytes / sizeof(double); 
  for (size_t i = 0; i < count; ++i) 
    input[i] = 0.0;
}
HPX_ACTION(HPX_FUNCTION, HPX_ATTR_NONE, sum_ident_op_, sum_ident_handler, 
           HPX_POINTER, HPX_SIZE_T); 

void sum_op_handler(double *lhs, const double *rhs, size_t bytes) {
  size_t count = bytes / sizeof(double); 
  for (size_t i = 0; i < count; ++i) 
    lhs[i] += rhs[i]; 
}
HPX_ACTION(HPX_FUNCTION, HPX_ATTR_NONE, sum_op_, sum_op_handler, 
           HPX_POINTER, HPX_POINTER, HPX_SIZE_T); 

int allocate_reducer_handler() {
  hpx_addr_t retval; 
  int num_ranks = hpx_get_num_ranks(); 
  retval = hpx_lco_reduce_new(num_ranks, sizeof(double), 
                              sum_ident_op_, sum_op_); 
  hpx_exit(sizeof(retval), &retval); 
}
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, 
           allocate_reducer_, allocate_reducer_handler);

int reset_reducer_handler(hpx_addr_t reduce) {
  hpx_lco_reset_sync(reduce); 
  hpx_exit(0, nullptr); 
} 
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, reset_reducer_, reset_reducer_handler, 
           HPX_ADDR); 

int inner_product_handler(hpx_addr_t data, hpx_addr_t reduce, int x, int y) {
  int myrank = hpx_get_my_rank(); 
  size_t meta = sizeof(dashmm::ArrayMetaData<Node>); 
  hpx_addr_t global = hpx_addr_add(data, meta * myrank, meta); 
  dashmm::ArrayMetaData<Node> *local{nullptr}; 
  assert(hpx_gas_try_pin(global, (void **)&local)); 

  Node *nodes = local->data; 
  size_t count = local->local_count; 
  double temp = 0.0; 

  if (x == y || x == -y) {
    if (x == -1) {
      // Compute ||rhs||_2
      for (int i = 0; i < count; ++i) 
        temp += pow(nodes[i].rhs[0], 2) + pow(nodes[i].rhs[1], 2);
    } else {
      // Compute ||q_x||_2
      for (int i = 0; i < count; ++i) 
        temp += pow(nodes[i].gmres[2 * x], 2) + 
          pow(nodes[i].gmres[2 * x + 1], 2);
    }
  } else {
    assert(x != -1 && y != -1); 
    // Compute <q_x, q_y> 
    for (int i = 0; i < count; ++i) 
      temp += nodes[i].gmres[2 * x] * nodes[i].gmres[2 * y] + 
        nodes[i].gmres[2 * x + 1] * nodes[i].gmres[2 * y + 1]; 
  }

  hpx_lco_set(reduce, sizeof(double), &temp, HPX_NULL, HPX_NULL); 
  hpx_lco_get(reduce, sizeof(double), &temp); 

  if (x == y) {
    temp = sqrt(temp); 

    if (x != -1) {
      // Normalize q_x
      for (int i = 0; i < count; ++i) {
        nodes[i].gmres[2 * x] /= temp; 
        nodes[i].gmres[2 * x + 1] /= temp;
      }
    } 
  } else if (x != -y) {
    // Overwrite q_x with q_x - temp * q_y
    for (int i = 0; i < count; ++i) {
      nodes[i].gmres[2 * x] -= temp * nodes[i].gmres[2 * y]; 
      nodes[i].gmres[2 * x + 1] -= temp * nodes[i].gmres[2 * y + 1];
    }
  }

  hpx_gas_unpin(global); 
  hpx_exit(sizeof(temp), &temp); 
}
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, inner_product_, 
           inner_product_handler, HPX_ADDR, HPX_ADDR, HPX_INT, HPX_INT); 

int linear_combination_handler(hpx_addr_t data, double *c, int k) {
  int myrank = hpx_get_my_rank(); 
  size_t meta = sizeof(dashmm::ArrayMetaData<Node>); 
  hpx_addr_t global = hpx_addr_add(data, meta * myrank, meta); 
  dashmm::ArrayMetaData<Node> *local{nullptr}; 
  assert(hpx_gas_try_pin(global, (void **)&local)); 

  Node *nodes = local->data; 
  size_t count = local->local_count; 

  for (int i = 0; i < count; ++i) {
    for (int j = 0; j <= k; ++j) {
      nodes[i].x0[0] += c[j] * nodes[i].gmres[2 * j]; 
      nodes[i].x0[1] += c[j] * nodes[i].gmres[2 * j + 1];

      nodes[i].gmres[2 * j] = 0.0; 
      nodes[i].gmres[2 * j + 1] = 0.0; 
    }
    nodes[i].gmres[2 * k + 2] = 0.0; 
    nodes[i].gmres[2 * k + 3] = 0.0; 

    // Copy the new guess to q0 slot 
    nodes[i].gmres[0] = nodes[i].x0[0]; 
    nodes[i].gmres[1] = nodes[i].x0[1]; 
  }

  hpx_gas_unpin(global); 
  hpx_exit(0, nullptr); 
}
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, linear_combination_, 
           linear_combination_handler, HPX_ADDR, HPX_POINTER, HPX_INT); 


void set_rhs(Node *n, const size_t count, const double *dielectric) {
  for (int i = 0; i < count; ++i) {
    n[i].rhs[0] /= (*dielectric); 
    n[i].rhs[1] /= (*dielectric); 

    n[i].x0[0] = n[i].rhs[0]; 
    n[i].x0[1] = n[i].rhs[1]; 

    n[i].gmres[0] = n[i].rhs[0]; 
    n[i].gmres[1] = n[i].rhs[1]; 
  }
}

void set_r0(Node *n, const size_t count, const double *unused) {
  for (int i = 0; i < count; ++i) {
    n[i].gmres[0] = n[i].rhs[0] - n[i].gmres[2]; 
    n[i].gmres[1] = n[i].rhs[1] - n[i].gmres[3]; 

    n[i].gmres[2] = 0.0; 
    n[i].gmres[3] = 0.0;
  }
}

double DAFMPB::generalizedInnerProduct(int x, int y) {
  using namespace std::chrono; 
  high_resolution_clock::time_point t1, t2; 

  double retval = 0.0; 
  hpx_addr_t data = nodes_.data(); 

  t1 = high_resolution_clock::now(); 
  hpx_run_spmd(&inner_product_, &retval, &data, &reducer_, &x, &y); 
  t2 = high_resolution_clock::now(); 
  t_inner_ += duration_cast<duration<double>>(t2 - t1).count(); 

  hpx_run(&reset_reducer_, nullptr, &reducer_); 

  return retval;
}

void DAFMPB::linearCombination(int k) {
  hpx_addr_t data = nodes_.data(); 
  double *c = residual_.data(); 
  hpx_run_spmd(&linear_combination_, nullptr, &data, &c, &k); 
}

} // namespace dafmpb 
