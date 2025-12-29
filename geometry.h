#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "types.h"

float calculateDistance(float x1, float y1, float x2, float y2);
int mapLogicalLaneToPhysical(int road, int logicalLane);
void calculateSpawnPosition(int road, int laneIndex, float* outx, float* outy);
void calculateIntersectionLaneCenter(int road, int logicalLane, float* outx, float* outy);

#endif // GEOMETRY_H
