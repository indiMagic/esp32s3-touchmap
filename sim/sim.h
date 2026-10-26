#pragma once

#include "kernel.h"

typedef struct {
    k_nav_snapshot_t nav;
    k_route_pt_t     route[64];
    uint32_t         route_n;
    k_sd_state_t     sd;
    bool             tiles_missing;
    uint32_t         now_ms;
    int              presented;
} sim_state_t;

extern sim_state_t g_sim;

void sim_dump_ppm(const char *path);
