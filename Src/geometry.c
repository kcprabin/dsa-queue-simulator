#include "geometry.h"
#include "config.h"

float calculateDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

int mapLogicalLaneToPhysical(int road, int logicalLane) {
    if (road == 0) {
        if (logicalLane == 3) return 0;
        if (logicalLane == 2) return 1;
        return 2;
    }
    else if (road == 1) {
        if (logicalLane == 3) return 0;
        if (logicalLane == 2) return 1;
        return 2;
    }
    else if (road == 2) {
        if (logicalLane == 3) return 2;
        if (logicalLane == 2) return 1;
        return 0;
    }
    else {
        if (logicalLane == 3) return 2;
        if (logicalLane == 2) return 1;
        return 0;
    }
}

void calculateSpawnPosition(int road, int laneIndex, float* outx, float* outy) {
    float cx = SCREEN_W / 2.0f;
    float cy = SCREEN_H / 2.0f;
    float startx = cx - ROAD_W / 2.0f;
    float starty = cy - ROAD_W / 2.0f;

    if (road == 0) { *outx = startx + LANE_W * (laneIndex + 0.5f); *outy = -30.0f; }
    else if (road == 1) { *outx = SCREEN_W + 30.0f; *outy = starty + LANE_W * (laneIndex + 0.5f); }
    else if (road == 2) { *outx = startx + LANE_W * (laneIndex + 0.5f); *outy = SCREEN_H + 30.0f; }
    else { *outx = -30.0f; *outy = starty + LANE_W * (laneIndex + 0.5f); }
}

void calculateIntersectionLaneCenter(int road, int logicalLane, float* outx, float* outy) {
    float cx = SCREEN_W / 2.0f;
    float cy = SCREEN_H / 2.0f;
    float startx = cx - ROAD_W / 2.0f;
    float starty = cy - ROAD_W / 2.0f;

    int idx = mapLogicalLaneToPhysical(road, logicalLane);
    if (road == 0) { *outx = startx + LANE_W * (idx + 0.5f); *outy = cy - ROAD_W / 2.0f - 8.0f; }
    else if (road == 1) { *outx = cx + ROAD_W / 2.0f + 8.0f; *outy = starty + LANE_W * (idx + 0.5f); }
    else if (road == 2) { *outx = startx + LANE_W * (idx + 0.5f); *outy = cy + ROAD_W / 2.0f + 8.0f; }
    else { *outx = cx - ROAD_W / 2.0f - 8.0f; *outy = starty + LANE_W * (idx + 0.5f); }
}
