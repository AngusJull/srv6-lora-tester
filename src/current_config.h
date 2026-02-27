/**
 * This file contains the details about a specific board configuration that is to be compiled into the
 * application
 */

#ifndef _CURRENT_CONFIG_H_
#define _CURRENT_CONFIG_H_

#ifdef TOPOLOGY_LINEAR_5
#  include "configs/linear_5_topo.h"
#else
#  ifdef TOPOLOGY_STAR
#    include "configs/star_topo.h"
#  else
#    ifdef TOPOLOGY_MESH
#      include "configs/mesh_topo.h"
#    else
// Use the 2 node linear topology if none are selected
#      include "configs/linear_2_topo.h"
#    endif // TOPOLOGY_MESH
#  endif   // TOPOLOGY_STAR
#endif     // TOPOLOGY_LINEAR_5

#ifndef NUM_NODES
#  error "Chosen topology does not define any nodes"
#endif

#endif // _CURRENT_CONFIG_H_
