// ======================================================================== //
// Copyright 2018-2022 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

/* copied from OWL project, and put into new namespace to avoid naming conflicts.*/

#pragma once

#include "cukd/common.h"
#include "cukd/helpers.h"
#include "cukd/spatial-kdtree.h"

namespace cukd {

  template <typename scalar_t>
  inline __device__ __host__
  auto sqr(scalar_t f) { return f * f; }

  template <typename scalar_t>
  inline __device__ __host__
  scalar_t sqrt(scalar_t f);

  template<> inline __device__ __host__
  float sqrt(float f) { return ::sqrtf(f); }

  template <typename point_traits_a, typename point_traits_b=point_traits_a>
  inline __device__ __host__
  auto sqrDistance(const typename point_traits_a::point_t& a,
                   const typename point_traits_b::point_t& b)
  {
    typename point_traits_a::scalar_t res = 0;
#pragma unroll
    for(int i=0; i<min(point_traits_a::numDims, point_traits_b::numDims); ++i) {
      const auto diff = point_traits_a::getCoord(a, i) - point_traits_b::getCoord(b, i);
      res += sqr(diff);
    }
    return res;
  }

  template <typename point_traits_a, typename point_traits_b=point_traits_a>
  inline __device__ __host__
  auto distance(const typename point_traits_a::point_t& a,
                const typename point_traits_b::point_t& b)
  {
    typename point_traits_a::scalar_t res = 0;
#pragma unroll
    for(int i=0; i<min(point_traits_a::numDims, point_traits_b::numDims); ++i) {
      const auto diff = point_traits_a::getCoord(a, i) - point_traits_b::getCoord(b, i);
      res += sqr(diff);
    }
    return sqrt(res);
  }

  
  // Structure of parameters to control the behavior of the FCP search.
  // By default FCP will perform an exact nearest neighbor search, but the
  // following parameters can be set to cut some corners and make the search
  // approximate in favor of speed.
  struct FcpSearchParams {
    // Controls how many "far branches" of the tree will be searched. If set to
    // 0 the algorithm will only go down the tree once following the nearest
    // branch each time. Kernels may ignore this value.
    int far_node_inspect_budget = INT_MAX;

    /*! will only search for elements that are BELOW (i.e., NOT
        including) this radius. This in particular allows for cutting
        down on the number of branches to be visited during
        traversal */
    float cutOffRadius = INFINITY;
  };





  struct FCPResult {
    inline __device__ float initialCullDist2() const
    { return closestDist2; }
    
    inline __device__ float clear(float initialDist2)
    {
      closestDist2 = initialDist2;
      closestPrimID = -1;
      return closestDist2;
    }
    
    /*! process a new candidate with given ID and (square) distance;
        and return square distance to be used for subsequent
        queries */
    inline __device__ float processCandidate(int candPrimID, float candDist2)
    {
      if (candDist2 < closestDist2) {
        closestDist2 = candDist2;
        closestPrimID = candPrimID;
      }
      return closestDist2;
    }

    inline __device__ int returnValue() const
    { return closestPrimID; }
    
    int   closestPrimID;
    float closestDist2;
  };
  

} // ::cukd





#if CUKD_IMPROVED_TRAVERSAL
# if CUKD_STACK_FREE
// stack-free, improved traversal -- only for refernce, this doesn't make sense in practice
#  include "traverse-sf-imp.h"
namespace cukd {
  template<typename node_t,
           typename node_traits=default_node_traits<node_t>>
  inline __device__
  int fcp(typename node_traits::point_t queryPoint,
          const box_t<typename node_traits::point_t> worldBounds,
          const node_t *d_nodes,
          int N,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));
    traverse_sf_imp<FCPResult,node_t,node_traits>
      (result,queryPoint,worldBounds,d_nodes,N);
    return result.returnValue();
  }
} // :: cukd

# else
// stack-free, improved traversal
#  include "traverse-cct.h"
namespace cukd {
  template<typename node_t,
           typename node_traits=default_node_traits<node_t>>
  inline __device__
  int fcp(typename node_traits::point_t queryPoint,
          const box_t<typename node_traits::point_t> worldBounds,
          const node_t *d_nodes,
          int N,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));
    traverse_cct<FCPResult,node_t,node_traits>
      (result,queryPoint,worldBounds,d_nodes,N);
    return result.returnValue();
  }
} // :: cukd

# endif
#else
# if CUKD_STACK_FREE
// stack-free, regular traversal
#  include "traverse-stack-free.h"
namespace cukd {
  template<typename node_t,
           typename node_traits=default_node_traits<node_t>>
  inline __device__
  int fcp(typename node_traits::point_t queryPoint,
          const node_t *d_nodes,
          int N,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));
    traverse_stack_free<FCPResult,node_t,node_traits>
      (result,queryPoint,d_nodes,N);
    return result.returnValue();
  }
} // :: cukd
# else
// default stack-based traversal
#  include "traverse-default-stack-based.h"
namespace cukd {
  template<typename node_t,
           typename node_traits=default_node_traits<node_t>>
  inline __device__
  int fcp(typename node_traits::point_t queryPoint,
          const node_t *d_nodes,
          int N,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));
    traverse_default<FCPResult,node_t,node_traits>
      (result,queryPoint,d_nodes,N);
    return result.returnValue();
  }
} // :: cukd

# endif
#endif





namespace cukd {
#if CUKD_IMPROVED_TRAVERSAL
  template<typename data_t,
           typename node_traits=default_node_traits<data_t>>
  inline __device__
  int fcp(const SpatialKDTree<data_t,node_traits> &tree,
          typename node_traits::point_t queryPoint,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));

    using node_t     = typename SpatialKDTree<data_t,node_traits>::Node;
    using point_t    = typename node_traits::point_t;
    using scalar_t   = typename scalar_type_of<point_t>::type;
    enum { num_dims  = num_dims_of<point_t>::value };
    
    scalar_t cullDist = result.initialCullDist2();

    /* can do at most 2**30 points... */
    struct StackEntry {
      int   nodeID;
      point_t closestCorner;
    };
    enum{ stack_depth = 50 };
    StackEntry stackBase[stack_depth];
    StackEntry *stackPtr = stackBase;

    int numSteps = 0;
    /*! current node in the tree we're traversing */
    int nodeID = 0;
    point_t closestPointOnSubtreeBounds = project(tree.bounds,queryPoint);
    if (sqrDistance(queryPoint,closestPointOnSubtreeBounds) > cullDist)
      return result.returnValue();
    node_t node;
    while (true) {
      while (true) {
        numSteps++;
        CUKD_STATS(if (cukd::g_traversalStats) ::atomicAdd(cukd::g_traversalStats,1));
        node = tree.nodes[nodeID];
        if (node.count)
          // this is a leaf...
          break;

        const auto query_coord = get_coord(queryPoint,node.dim);
        const bool leftIsClose = query_coord < node.pos;
        const int  lChild = node.offset+0;
        const int  rChild = node.offset+1;

        const int closeChild = leftIsClose?lChild:rChild;
        const int farChild   = leftIsClose?rChild:lChild;

        auto farSideCorner = closestPointOnSubtreeBounds;
          
        get_coord(farSideCorner,node.dim) = node.pos;

        const float farSideDist2 = sqrDistance(farSideCorner,queryPoint);
        if (farSideDist2 < cullDist) {
          stackPtr->closestCorner = farSideCorner;
          stackPtr->nodeID  = farChild;
          ++stackPtr;
          if ((stackPtr - stackBase) >= stack_depth) {
            printf("STACK OVERFLOW %i\n",int(stackPtr - stackBase));
            return -1;
          }
        }
        nodeID = closeChild;
      }

      for (int i=0;i<node.count;i++) {
        int primID = tree.primIDs[node.offset+i];
        CUKD_STATS(if (cukd::g_traversalStats) ::atomicAdd(cukd::g_traversalStats,1));
        auto dp = node_traits::get_point(tree.data[primID]);
          
        const auto sqrDist = sqrDistance(node_traits::get_point(tree.data[primID]),queryPoint);
        cullDist = result.processCandidate(primID,sqrDist);
      }
      
      while (true) {
        if (stackPtr == stackBase)  {
          return result.returnValue();
        }
        --stackPtr;
        closestPointOnSubtreeBounds = stackPtr->closestCorner;
        if (sqrDistance(closestPointOnSubtreeBounds,queryPoint) >= cullDist)
          continue;
        nodeID = stackPtr->nodeID;
        break;
      }
    }
  }
#else
  template<typename data_t,
           typename node_traits=default_node_traits<data_t>>
  inline __device__
  int fcp(const SpatialKDTree<data_t,node_traits> &tree,
          typename node_traits::point_t queryPoint,
          FcpSearchParams params = FcpSearchParams{})
  {
    FCPResult result;
    result.clear(sqr(params.cutOffRadius));

    using node_t     = typename SpatialKDTree<data_t,node_traits>::Node;
    using point_t    = typename node_traits::point_t;
    using scalar_t   = typename scalar_type_of<point_t>::type;
    enum { num_dims  = num_dims_of<point_t>::value };
    
    scalar_t cullDist = result.initialCullDist2();

    /* can do at most 2**30 points... */
    struct StackEntry {
      int   nodeID;
      float sqrDist;
    };
    enum{ stack_depth = 50 };
    StackEntry stackBase[stack_depth];
    StackEntry *stackPtr = stackBase;

    /*! current node in the tree we're traversing */
    int nodeID = 0;
    node_t node;
    int numSteps = 0;
    while (true) {
      while (true) {
        CUKD_STATS(if (cukd::g_traversalStats) ::atomicAdd(cukd::g_traversalStats,1));
        node = tree.nodes[nodeID];
        ++numSteps;
        if (node.count)
          // this is a leaf...
          break;
        const auto query_coord = get_coord(queryPoint,node.dim);
        const bool leftIsClose = query_coord < node.pos;
        const int  lChild = node.offset+0;
        const int  rChild = node.offset+1;

        const int closeChild = leftIsClose?lChild:rChild;
        const int farChild   = leftIsClose?rChild:lChild;
        
        const float sqrDistToPlane = sqr(query_coord - node.pos);
        if (sqrDistToPlane < cullDist) {
          stackPtr->nodeID  = farChild;
          stackPtr->sqrDist = sqrDistToPlane;
          ++stackPtr;
          if ((stackPtr - stackBase) >= stack_depth) {
            printf("STACK OVERFLOW %i\n",int(stackPtr - stackBase));
            return -1;
          }
        }
        nodeID = closeChild;
      }

      for (int i=0;i<node.count;i++) {
        int primID = tree.primIDs[node.offset+i];
        const auto sqrDist = sqrDistance(node_traits::get_point(tree.data[primID]),queryPoint);
        CUKD_STATS(if (cukd::g_traversalStats) ::atomicAdd(cukd::g_traversalStats,1));
        cullDist = result.processCandidate(primID,sqrDist);
      }
      
      while (true) {
        if (stackPtr == stackBase)  {
          return result.returnValue();
        }
        --stackPtr;
        if (stackPtr->sqrDist >= cullDist)
          continue;
        nodeID = stackPtr->nodeID;
        break;
      }
    }
  }
#endif
} // :: cukd
