#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"

extern RoadData roads[4];
extern TransitionVehicle transitions[MAX_QUEUE];
extern int transitionCount;
extern int currentGreen;
extern int lightState;

extern const char* basedir;
extern const char* files[4];

#endif // GLOBALS_H
