#ifndef TYPES_H
#define TYPES_H

#include "config.h"

typedef struct {
    int id;
    float x, y;
    int fromRoad;
    int isStopped;
    char name[NAME_MAX];
} Vehicle;

typedef struct {
    Vehicle v;
    int targetRoad;
    int waitingTime;
} TransitionVehicle;

typedef struct {
    Vehicle data[MAX_QUEUE];
    int front, rear, count;
} Lane;

typedef struct {
    Lane L1;
    Lane L2;
    Lane L3;
} RoadData;

#endif // TYPES_H
