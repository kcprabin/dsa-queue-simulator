#ifndef PHYSICS_H
#define PHYSICS_H

#include "types.h"

float measureDistanceToFrontVehicle(Lane* L, int road, int vehicleIndex);
int detectCollisionInLane(Lane* l, float x, float y, int skipIndex);
void calculateRightTurnMovementVector(int road, float* dx, float* dy);
void updateRightTurnLane(Lane* L, int road);
void updateLaneVehiclesToIntersection(Lane* L, int road);
void insertVehicleIntoTransition(Vehicle v, int targetRoad);

#endif // PHYSICS_H
