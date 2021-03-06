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

#ifndef __DAFMPB_SERIALIZER_H__
#define __DAFMPB_SERIALIZER_H__

#include <cstring> 
#include "dashmm/serializer.h"
#include "dafmpb.h"
#include "dafmpb_lhs.h"

namespace dashmm {

class NodeFullSerializer : public Serializer {
public: 
  ~NodeFullSerializer() { } 

  size_t size(void *object) const override {
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    size_t retval = 0; 
    retval = sizeof(int) * 3 + // Index, n_patches,  gmres buffer size
      sizeof(Point) * 3 + // position, normal_i and normal_o 
      sizeof(dafmpb::Patch) * n->patch.size() + // patch 
      sizeof(double) * 8; // area, projected, rhs[0]@2, x0@[2], gmres[0]@2
    return retval; 
  }

  void *serialize(void *object, void *buffer) const override {
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *dest = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 
    int n_patches = n->patch.size(); 
    int n_gmres = n->gmres.size(); 

    bytes = sizeof(int); 
    memcpy(dest, &(n->index), bytes); 
    dest += bytes; 
    
    memcpy(dest, &n_patches, bytes); 
    dest += bytes; 

    memcpy(dest, &n_gmres, bytes); 
    dest += bytes; 

    bytes = sizeof(Point); 
    memcpy(dest, &(n->position), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->normal_i), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->normal_o), bytes); 
    dest += bytes; 

    bytes = sizeof(dafmpb::Patch) * n_patches; 
    memcpy(dest, n->patch.data(), bytes); 
    dest += bytes; 

    bytes = sizeof(double); 
    memcpy(dest, &(n->area), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->projected), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->rhs[0]), bytes * 2); 
    dest += bytes * 2; 

    memcpy(dest, &(n->x0[0]), bytes * 2); 
    dest += bytes * 2; 

    double *gmres = n->gmres.data(); 
    memcpy(dest, gmres, bytes * 2); 
    dest += bytes * 2; 

    return dest;
  }

  void *deserialize(void *buffer, void *object) const override {
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *src = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 
    int n_patches = 0; 
    int n_gmres = 0; 

    bytes = sizeof(int); 
    memcpy(&(n->index), src, bytes); 
    src += bytes; 

    memcpy(&n_patches, src, bytes); 
    src += bytes; 

    memcpy(&n_gmres, src, bytes); 
    src += bytes; 
    n->gmres.resize(n_gmres); 

    bytes = sizeof(Point); 
    memcpy(&(n->position), src, bytes); 
    src += bytes; 

    memcpy(&(n->normal_i), src, bytes); 
    src += bytes; 

    memcpy(&(n->normal_o), src, bytes); 
    src += bytes; 

    dafmpb::Patch *p = reinterpret_cast<dafmpb::Patch *>(src); 
    n->patch.assign(p, p + n_patches); 

    src += sizeof(dafmpb::Patch) * n_patches; 

    bytes = sizeof(double); 
    memcpy(&(n->area), src, bytes); 
    src += bytes; 

    memcpy(&(n->projected), src, bytes); 
    src += bytes; 

    memcpy(n->rhs, src, bytes * 2); 
    src += bytes * 2; 
    
    memcpy(n->x0, src, bytes * 2); 
    src += bytes * 2; 

    double *v = reinterpret_cast<double *>(src); 
    n->gmres[0] = v[0]; 
    n->gmres[1] = v[1]; 

    src += bytes * 2; 

    return src; 
  }
}; 

class NodePartialSerializer : public Serializer {
public: 
  ~NodePartialSerializer() { } 

  size_t size(void *object) const override {
    return sizeof(int) * 2 + // Index, and gmres buffer size
      + sizeof(Point) * 2 + // position, normal_i
      sizeof(double) * 3; // area, gmres[2 * iter] @2 
  } 

  void *serialize(void *object, void *buffer) const override {
    int iter = builtin_dafmpb_table_->s_iter(); 
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *dest = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 
    int n_gmres = n->gmres.size(); 

    bytes = sizeof(int);
    memcpy(dest, &(n->index), bytes); 
    dest += bytes; 

    memcpy(dest, &n_gmres, bytes); 
    dest += bytes; 

    bytes = sizeof(Point); 
    memcpy(dest, &(n->position), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->normal_i), bytes); 
    dest += bytes; 

    bytes = sizeof(double); 
    memcpy(dest, &(n->area), bytes); 
    dest += bytes; 

    bytes = sizeof(double) * 2; 
    double *v = &(n->gmres[2 * iter]); 
    memcpy(dest, v, bytes); 
    dest += bytes; 

    return dest; 
  }
   
  void *deserialize(void *buffer, void *object) const override {
    int iter = builtin_dafmpb_table_->s_iter(); 
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *src = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 
    int n_gmres = 0; 

    bytes = sizeof(int); 
    memcpy(&(n->index), src, bytes); 
    src += bytes; 

    memcpy(&n_gmres, src, bytes); 
    src += bytes; 
    n->gmres.resize(n_gmres); 

    bytes = sizeof(Point); 
    memcpy(&(n->position), src, bytes); 
    src += bytes; 

    memcpy(&(n->normal_i), src, bytes); 
    src += bytes; 

    bytes = sizeof(double); 
    memcpy(&(n->area), src, bytes); 
    src += bytes; 

    bytes = sizeof(double) * 2; 
    double *v = reinterpret_cast<double *>(src); 
    n->gmres[2 * iter] = v[0]; 
    n->gmres[2 * iter + 1] = v[1]; 

    src += bytes; 

    return src; 
  }
}; 

class NodeMinimumSerializer : public Serializer {
public: 
  ~NodeMinimumSerializer() { } 

  size_t size(void *object) const override {
    return sizeof(int) + // Index 
      + sizeof(Point) * 2 + // position, normal_o 
      sizeof(double) * 5; // gmres[0]@2, rhs[0]@2, area
  }

  void *serialize(void *object, void *buffer) const override {
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *dest = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 

    bytes = sizeof(int); 
    memcpy(dest, &(n->index), bytes); 
    dest += bytes; 

    bytes = sizeof(Point); 
    memcpy(dest, &(n->position), bytes); 
    dest += bytes; 

    memcpy(dest, &(n->normal_o), bytes); 
    dest += bytes; 

    bytes = sizeof(double) * 2; 
    double *v = &(n->gmres[0]); 
    memcpy(dest, v, bytes); 
    dest += bytes; 

    v = &(n->rhs[0]); 
    memcpy(dest, v, bytes); 
    dest += bytes; 

    memcpy(dest, &(n->area), sizeof(double)); 
    dest += sizeof(double); 

    return dest;
  }

  void *deserialize(void *buffer, void *object) const override {
    dafmpb::Node *n = reinterpret_cast<dafmpb::Node *>(object); 
    char *src = reinterpret_cast<char *>(buffer); 
    size_t bytes = 0; 

    bytes = sizeof(int); 
    memcpy(&(n->index), src, bytes); 
    src += bytes; 

    bytes = sizeof(Point); 
    memcpy(&(n->position), src, bytes); 
    src += bytes; 

    memcpy(&(n->normal_o), src, bytes); 
    src += bytes; 

    n->gmres.resize(2); 
    double *v = reinterpret_cast<double *>(src); 
    n->gmres[0] = v[0]; 
    n->gmres[1] = v[1]; 

    n->rhs[0] = v[2]; 
    n->rhs[1] = v[3]; 

    n->area = v[4]; 
    src += sizeof(double) * 5; 
  
    return src;
  }
}; 

} // namespace dashmm


#endif // __DAFMPB_SERIALIZER_H__
