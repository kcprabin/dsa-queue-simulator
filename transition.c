#include "transition.h"
#include "geometry.h"
#include "globals.h"
#include "queue.h"
#include <math.h>
#include <SDL.h>

void processIntersectionTransitions() {
    for (int i = 0; i < transitionCount - 1; i++) {
        for (int j = i + 1; j < transitionCount; j++) {
            if (transitions[j].waitingTime > transitions[i].waitingTime) {
                TransitionVehicle temp = transitions[i];
                transitions[i] = transitions[j];
                transitions[j] = temp;
            }
        }
    }

    for (int i = 0; i < transitionCount; i++) {
        TransitionVehicle* tv = &transitions[i];
        tv->waitingTime++;
        tv->v.isStopped = 0;

        float tx, ty;
        calculateIntersectionLaneCenter(tv->targetRoad, 3, &tx, &ty);
        float dx = tx - tv->v.x;
        float dy = ty - tv->v.y;
        float dist = sqrtf(dx * dx + dy * dy);

        float speed = VEHICLE_SPEED;

        if (dist < speed) {
            tv->v.x = tx;
            tv->v.y = ty;
            tv->v.fromRoad = tv->targetRoad;

            if (!detectCollisionInLane(&roads[tv->targetRoad].L3, tx, ty, -1)) {
                queueInsert(&roads[tv->targetRoad].L3, tv->v);

                for (int j = i; j < transitionCount - 1; j++) {
                    transitions[j] = transitions[j + 1];
                }
                transitionCount--;
                i--;
            }
            else {
                tv->v.isStopped = 1;
                if (tv->waitingTime > 50) {
                    queueInsert(&roads[tv->targetRoad].L3, tv->v);

                    for (int j = i; j < transitionCount - 1; j++) {
                        transitions[j] = transitions[j + 1];
                    }
                    transitionCount--;
                    i--;
                }
            }
        }
        else {
            int canMove = 1;

            for (int j = 0; j < transitionCount; j++) {
                if (i == j) continue;
                float otherDist = calculateDistance(tv->v.x, tv->v.y,
                    transitions[j].v.x, transitions[j].v.y);
                if (otherDist < MIN_FRONT_SPACING * 1.5f) {
                    if (transitions[j].waitingTime >= tv->waitingTime) {
                        canMove = 0;
                        break;
                    }
                }
            }

            if (canMove) {
                float newx = tv->v.x + speed * dx / dist;
                float newy = tv->v.y + speed * dy / dist;

                if (fabs(dx) < fabs(newx - tv->v.x)) newx = tx;
                if (fabs(dy) < fabs(newy - tv->v.y)) newy = ty;

                tv->v.x = newx;
                tv->v.y = newy;
                tv->v.isStopped = 0;
            }
            else {
                tv->v.isStopped = 1;
            }
        }
    }
}

void removeStuckTransitionVehicles() {
    static Uint32 lastCleanup = 0;
    Uint32 now = SDL_GetTicks();

    if (now - lastCleanup >= 5000) {
        for (int i = 0; i < transitionCount; i++) {
            if (transitions[i].waitingTime > 150) {
                for (int j = i; j < transitionCount - 1; j++) {
                    transitions[j] = transitions[j + 1];
                }
                transitionCount--;
                i--;
            }
        }
        lastCleanup = now;
    }
}
