#ifndef QUEUE_H
#define QUEUE_H

#include "types.h"

void queueInitialize(Lane* l);
int queueInsert(Lane* l, Vehicle v);
int queueRemove(Lane* l, Vehicle* out);
Vehicle* queueGetVehicleAt(Lane* l, int index);

#endif // QUEUE_H
