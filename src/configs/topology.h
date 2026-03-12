/*
 * Topology definitions
 */

#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_

#include "config_common.h"

enum topology {
    TOPOLOGY_LINEAR_2 = 0,
    TOPOLOGY_LINEAR_3,
    TOPOLOGY_LINEAR_5,
    TOPOLOGY_MESH,
    TOPOLOGY_STAR,
    TOPOLOGY_NUM_TOPOLOGIES
};

// Using externs here to get topologies without all the intermediate arrays and configuration required to build them
extern struct topology_configuration linear_2_topo;
extern struct topology_configuration linear_5_topo;
extern struct topology_configuration linear_3_topo;
extern struct topology_configuration mesh_topo;
extern struct topology_configuration star_topo;

static struct topology_configuration *topology_array[TOPOLOGY_NUM_TOPOLOGIES] = {
    [TOPOLOGY_LINEAR_2] = &linear_2_topo,
    [TOPOLOGY_LINEAR_3] = &linear_3_topo,
    [TOPOLOGY_LINEAR_5] = &linear_5_topo,
    [TOPOLOGY_MESH] = &mesh_topo,
    [TOPOLOGY_STAR] = &star_topo,
};

#endif // _TOPOLOGY_H_
