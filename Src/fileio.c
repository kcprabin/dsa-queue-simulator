#include "fileio.h"
#include "globals.h"
#include "geometry.h"
#include "queue.h"
#include <string.h>

void loadVehiclesFromInputFiles() {
    static int lastReadLines[4] = { 0, 0, 0, 0 };

    for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
        FILE* f = fopen(files[roadIdx], "r");
        if (!f) continue;

        char line[128];
        int currentLine = 0;

        while (currentLine < lastReadLines[roadIdx] && fgets(line, sizeof(line), f)) {
            currentLine++;
        }

        while (fgets(line, sizeof(line), f)) {
            int id, lane;
            char name[NAME_MAX];

            if (sscanf(line, "%d %15s %d", &id, name, &lane) == 3) {
                Vehicle v;
                v.id = id;
                v.fromRoad = roadIdx;
                v.isStopped = 0;
                strncpy(v.name, name, NAME_MAX - 1);
                v.name[NAME_MAX - 1] = '\0';

                Lane* targetLane = NULL;
                int laneIndex = 0;

                if (lane == 1) {
                    targetLane = &roads[roadIdx].L1;
                    laneIndex = mapLogicalLaneToPhysical(roadIdx, 1);
                }
                else if (lane == 2) {
                    targetLane = &roads[roadIdx].L2;
                    laneIndex = mapLogicalLaneToPhysical(roadIdx, 2);
                }
                else if (lane == 3) {
                    targetLane = &roads[roadIdx].L3;
                    laneIndex = mapLogicalLaneToPhysical(roadIdx, 3);
                }

                if (targetLane) {
                    float sx, sy;
                    calculateSpawnPosition(roadIdx, laneIndex, &sx, &sy);
                    v.x = sx;
                    v.y = sy;

                    if (!detectCollisionInLane(targetLane, sx, sy, -1)) {
                        queueInsert(targetLane, v);
                    }
                }
            }

            lastReadLines[roadIdx]++;
        }

        fclose(f);
    }
}
