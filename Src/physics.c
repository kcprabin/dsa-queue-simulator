#include "physics.h"
#include "geometry.h"
#include "globals.h"
#include "queue.h"
#include <math.h>

float measureDistanceToFrontVehicle(Lane* L, int road, int vehicleIndex) {
    Vehicle* current = queueGetVehicleAt(L, vehicleIndex);
    if (!current) return 999999.0f;

    float minDist = 999999.0f;

    for (int i = vehicleIndex + 1; i < L->count; i++) {
        Vehicle* ahead = queueGetVehicleAt(L, i);
        if (!ahead) continue;

        float distAlongRoad = 0.0f;
        switch (road) {
        case 0: distAlongRoad = ahead->y - current->y; break;
        case 1: distAlongRoad = current->x - ahead->x; break;
        case 2: distAlongRoad = current->y - ahead->y; break;
        case 3: distAlongRoad = ahead->x - current->x; break;
        }

        if (distAlongRoad > 0 && distAlongRoad < minDist) {
            minDist = distAlongRoad;
        }
    }

    return minDist;
}

int detectCollisionInLane(Lane* l, float x, float y, int skipIndex) {
    for (int i = 0; i < l->count; i++) {
        if (i == skipIndex) continue;
        Vehicle* other = queueGetVehicleAt(l, i);
        if (!other) continue;
        float dist = calculateDistance(x, y, other->x, other->y);
        if (dist < MIN_SPACING) return 1;
    }
    return 0;
}

void calculateRightTurnMovementVector(int road, float* dx, float* dy) {
    if (road == 0) { *dx = 0.0f; *dy = -VEHICLE_SPEED; }
    else if (road == 1) { *dx = VEHICLE_SPEED; *dy = 0.0f; }
    else if (road == 2) { *dx = 0.0f; *dy = VEHICLE_SPEED; }
    else { *dx = -VEHICLE_SPEED; *dy = 0.0f; }
}

void updateRightTurnLane(Lane* L, int road) {
    float dx, dy;
    calculateRightTurnMovementVector(road, &dx, &dy);

    int i = 0;
    while (i < L->count) {
        Vehicle* v = queueGetVehicleAt(L, i);
        if (!v) { i++; continue; }

        float newx = v->x + dx;
        float newy = v->y + dy;

        if (newx < -100 || newx > SCREEN_W + 100 || newy < -100 || newy > SCREEN_H + 100) {
            Vehicle temp;
            queueRemove(L, &temp);
            continue;
        }

        int canMove = 1;
        for (int j = 0; j < L->count; j++) {
            if (i == j) continue;
            Vehicle* other = queueGetVehicleAt(L, j);
            if (!other) continue;

            float distToOther;
            switch (road) {
            case 0: distToOther = v->y - other->y; break;
            case 1: distToOther = other->x - v->x; break;
            case 2: distToOther = other->y - v->y; break;
            case 3: distToOther = v->x - other->x; break;
            }

            if (distToOther > 0 && distToOther < MIN_SPACING) {
                canMove = 0;
                break;
            }
        }

        if (canMove) {
            v->x = newx;
            v->y = newy;
            v->isStopped = 0;
        }
        else {
            v->isStopped = 1;
        }

        i++;
    }
}

void updateLaneVehiclesToIntersection(Lane* L, int road) {
    float mvx = 0.0f, mvy = 0.0f;
    if (road == 0) { mvy = VEHICLE_SPEED; }
    else if (road == 1) { mvx = -VEHICLE_SPEED; }
    else if (road == 2) { mvy = -VEHICLE_SPEED; }
    else { mvx = VEHICLE_SPEED; }

    for (int i = L->count - 1; i >= 0; i--) {
        Vehicle* v = queueGetVehicleAt(L, i);
        if (!v) continue;

        float cx = SCREEN_W / 2.0f;
        float cy = SCREEN_H / 2.0f;
        float distToIntersection;

        if (road == 0) distToIntersection = cy - ROAD_W / 2.0f - v->y;
        else if (road == 1) distToIntersection = v->x - (cx + ROAD_W / 2.0f);
        else if (road == 2) distToIntersection = v->y - (cy + ROAD_W / 2.0f);
        else distToIntersection = cx - ROAD_W / 2.0f - v->x;

        if (lightState == RED_LIGHT || currentGreen != road) {
            if (distToIntersection < STOPPING_DISTANCE && distToIntersection > 0) {
                v->isStopped = 1;
                continue;
            }
        }

        float distToAhead = measureDistanceToFrontVehicle(L, road, i);

        if (distToAhead < STOPPING_DISTANCE) {
            if (i + 1 < L->count) {
                Vehicle* ahead = queueGetVehicleAt(L, i + 1);
                if (ahead && ahead->isStopped) {
                    v->isStopped = 1;
                    continue;
                }
            }

            if (distToAhead < MIN_FRONT_SPACING) {
                v->isStopped = 1;
                continue;
            }
        }

        float newx = v->x + mvx;
        float newy = v->y + mvy;

        if (!detectCollisionInLane(L, newx, newy, i)) {
            v->x = newx;
            v->y = newy;
            v->isStopped = 0;
        }
        else {
            v->isStopped = 1;
        }

        if (v->x < -100 || v->x > SCREEN_W + 100 || v->y < -100 || v->y > SCREEN_H + 100) {
            for (int j = i; j < L->count - 1; j++) {
                L->data[(L->front + j) % MAX_QUEUE] = L->data[(L->front + j + 1) % MAX_QUEUE];
            }
            L->count--;
        }
    }
}

void insertVehicleIntoTransition(Vehicle v, int targetRoad) {
    if (transitionCount >= MAX_QUEUE) return;
    v.isStopped = 0;
    transitions[transitionCount].v = v;
    transitions[transitionCount].targetRoad = targetRoad;
    transitions[transitionCount].waitingTime = 0;
    transitionCount++;
}
