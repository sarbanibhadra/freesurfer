/**
 * @file  vecvolgpu.cu
 * @brief Holds datatype for float vector volume data on the GPU
 *
 * Holds a datatype for vector volume data on the GPU.
 */
/*
 * Original Author: Richard Edgar
 * CVS Revision Info:
 *    $Author: rge21 $
 *    $Date: 2011/02/11 19:12:55 $
 *    $Revision: 1.1 $
 *
 * Copyright (C) 2002-2008,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 *
 */

#include "cudacheck.h"
#include "cudatypeutils.hpp"

#include "vecvolgpu.hpp"

namespace GPU {

  namespace Classes {

    // ==========================================================
    // Memory management

    void VecVolGPU::Allocate( const dim3 myDims ) {
      
      // Check if we can re-use current memory
      if( myDims == this->dims ) {
        return;
      }

      this->Release();

      this->dims = myDims;

      cudaExtent tmpExtent = ExtentFromDims( this->dims );
      tmpExtent.width *= sizeof(float);

      // Allocate the memory
      CUDA_SAFE_CALL( cudaMalloc3D( &(this->d_x), tmpExtent ) );
      CUDA_SAFE_CALL( cudaMalloc3D( &(this->d_y), tmpExtent ) );
      CUDA_SAFE_CALL( cudaMalloc3D( &(this->d_z), tmpExtent ) );
    }

    // ----

    void VecVolGPU::Release( void ) {
      if( this->d_x.ptr != NULL ) {
        CUDA_SAFE_CALL( cudaFree( this->d_x.ptr ) );
        CUDA_SAFE_CALL( cudaFree( this->d_y.ptr ) );
        CUDA_SAFE_CALL( cudaFree( this->d_z.ptr ) );
        this->dims = make_uint3(0,0,0);
        this->d_x = make_cudaPitchedPtr(NULL,0,0,0);
        this->d_y = make_cudaPitchedPtr(NULL,0,0,0);
        this->d_z = make_cudaPitchedPtr(NULL,0,0,0);
      }
    }


    // ----


    size_t VecVolGPU::BufferSize( void ) const {
      size_t nElements;

      nElements = this->dims.x * this->dims.y * this->dims.z;

      return( nElements*sizeof(float) );
    }

    
    // ----

    
    void VecVolGPU::AllocateHostBuffers( float** h_x,
                                         float** h_y,
                                         float** h_z ) const {
      CUDA_SAFE_CALL( cudaHostAlloc( h_x,
                                     this->BufferSize(),
                                     cudaHostAllocDefault ) );
      CUDA_SAFE_CALL( cudaHostAlloc( h_y,
                                     this->BufferSize(),
                                     cudaHostAllocDefault ) );
      CUDA_SAFE_CALL( cudaHostAlloc( h_z,
                                     this->BufferSize(),
                                     cudaHostAllocDefault ) );
      
    }


    // ==========================================================
    // Data transfer

    void VecVolGPU::SendBuffers( const float* const h_x,
                                 const float* const h_y,
                                 const float* const h_z ) {

      cudaMemcpy3DParms copyParams = {0};

      copyParams.srcPtr = make_cudaPitchedPtr( (void*)h_x,
					       this->dims.x*sizeof(float),
					       this->dims.x,
					       this->dims.y );
      copyParams.dstPtr = this->d_x;
      copyParams.extent = ExtentFromDims( this->dims );
      copyParams.extent.width *= sizeof(float);
      copyParams.kind = cudaMemcpyHostToDevice;
      CUDA_SAFE_CALL( cudaMemcpy3D( &copyParams ) );


      copyParams.srcPtr = make_cudaPitchedPtr( (void*)h_y,
					       this->dims.x*sizeof(float),
					       this->dims.x,
					       this->dims.y );
      copyParams.dstPtr = this->d_y;
      CUDA_SAFE_CALL( cudaMemcpy3D( &copyParams ) );

      
      copyParams.srcPtr = make_cudaPitchedPtr( (void*)h_z,
					       this->dims.x*sizeof(float),
					       this->dims.x,
					       this->dims.y );
      copyParams.dstPtr = this->d_z;
      CUDA_SAFE_CALL( cudaMemcpy3D( &copyParams ) );
    }

    // ----

    void VecVolGPU::RecvBuffers( float* const h_x,
                                 float* const h_y,
                                 float* const h_z ) const {

      cudaMemcpy3DParms cpyPrms = {0};
      cpyPrms.srcPtr = this->d_x;
      cpyPrms.dstPtr = make_cudaPitchedPtr( (void*)h_x,
					    this->dims.x*sizeof(float),
					    this->dims.x,
					    this->dims.y );
      cpyPrms.extent = ExtentFromDims( this->dims );
      cpyPrms.extent.width *= sizeof(float);
      cpyPrms.kind = cudaMemcpyDeviceToHost;
      CUDA_SAFE_CALL( cudaMemcpy3D( &cpyPrms ) );

      cpyPrms.srcPtr = this->d_y;
      cpyPrms.dstPtr = make_cudaPitchedPtr( (void*)h_y,
					    this->dims.x*sizeof(float),
					    this->dims.x,
					    this->dims.y );
      CUDA_SAFE_CALL( cudaMemcpy3D( &cpyPrms ) );

      cpyPrms.srcPtr = this->d_z;
      cpyPrms.dstPtr = make_cudaPitchedPtr( (void*)h_z,
					    this->dims.x*sizeof(float),
					    this->dims.x,
					    this->dims.y );
      CUDA_SAFE_CALL( cudaMemcpy3D( &cpyPrms ) );

    }

  }
}
