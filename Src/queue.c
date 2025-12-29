#include "queue.h"

void queueInitialize(Lane* l) {
    l->front = 0;
    l->rear = -1;
    l->count = 0;
}

int queueInsert(Lane* l, Vehicle v) {
    if (l->count >= MAX_QUEUE) return 0;
    l->rear = (l->rear + 1) % MAX_QUEUE;
    l->data[l->rear] = v;
    l->count++;
    return 1;
}

int queueRemove(Lane* l, Vehicle* out) {
    if (l->count == 0) return 0;
    *out = l->data[l->front];
    l->front = (l->front + 1) % MAX_QUEUE;
    l->count--;
    return 1;
}

Vehicle* queueGetVehicleAt(Lane* l, int index) {
    if (index < 0 || index >= l->count) return NULL;
    int idx = (l->front + index) % MAX_QUEUE;
    return &l->data[idx];
}
