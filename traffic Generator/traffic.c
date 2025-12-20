#define _CRT_SECURE_NO_WARNINGS 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <direct.h>

const char* baseDir = "C:\\TrafficShared";
const char* files[4] = {    
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",         
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};

#define DEFAULT_INTERVAL_MS 500
#define NAME_MAX 16  
#define MIN_SPAWN_SPACING_MS 400

typedef struct {
    ULONGLONG lastSpawnTime[3];
} RoadSpawnTracker;

RoadSpawnTracker spawnTrackers[4] = { {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}} };


static int canSpawnOnLane(int road, int lane) {
    ULONGLONG now = GetTickCount64();
    ULONGLONG last = spawnTrackers[road].lastSpawnTime[lane - 1];
    if (now - last >= MIN_SPAWN_SPACING_MS) {
        spawnTrackers[road].lastSpawnTime[lane - 1] = now;
        return 1;
    }
    return 0;
}


static int append_vehicle_to_file(int road, int id, const char* name, int lane) {
    FILE* f = fopen(files[road], "a");
    if (!f) {
        printf("[ERROR] Cannot open file: %s\n", files[road]);
        return 0;
    }

    fprintf(f, "%d %s %d\n", id, name, lane);
    fflush(f);
    fclose(f);
    return 1;
}


static int choose_lane_weighted(void) {
    int r = rand() % 100;
    if (r < 25) return 1;
    if (r < 85) return 2;
    return 3;
}


static int choose_lane_safe(int road) {
    int p = choose_lane_weighted();
    if (canSpawnOnLane(road, p)) return p;

    for (int i = 1; i <= 3; i++) {
        if (canSpawnOnLane(road, i)) return i;
    }
    return -1;
}


static void clear_all_files(void) {
    printf("Clearing existing data files...\n");
    for (int i = 0; i < 4; i++) {
        FILE* f = fopen(files[i], "w");
        if (f) {
            fclose(f);
            printf("  ✓ Cleared: %s\n", files[i]);
        }
        else {
            printf("  ✗ Warning: Could not clear file: %s\n", files[i]);
        }
    }
    printf("\n");
}


static void ensure_directory_exists(void) {
    if (_mkdir(baseDir) == 0) {
        printf("Created directory: %s\n", baseDir);
    } else {
        DWORD attrib = GetFileAttributesA(baseDir);
        if (attrib == INVALID_FILE_ATTRIBUTES) {
            printf("[ERROR] Cannot create directory: %s\n", baseDir);
            exit(1);
        } else if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
            printf("Using existing directory: %s\n", baseDir);
        }
    }
}

int main(int argc, char** argv) {
    int interval_ms = DEFAULT_INTERVAL_MS;

    for (int i = 1; i < argc; i++) {
        int t = atoi(argv[i]);
        if (t > 0) {
            interval_ms = t;
            printf("Custom interval set: %dms\n", interval_ms);
        }
    }

    ensure_directory_exists();
    clear_all_files();

    srand((unsigned int)(time(NULL) ^ GetTickCount64()));
    int nextId = 1;

    int roadIdx = 0;

    printf("=== Traffic Generator Started ===\n");
    printf("Interval: %dms\n", interval_ms);
    printf("Output: %s\n", baseDir);
    printf("Format: ID Name Lane\n");
    printf("Vehicle Spawn Rate: 2 vehicles per second\n");
    printf("=================================\n\n");

    while (1) {
        int road = roadIdx % 4;
        roadIdx++;

        int lane = choose_lane_safe(road);
        if (lane == -1) {
            Sleep(interval_ms);
            continue;
        }       

        char name[NAME_MAX];
        snprintf(name, sizeof(name), "veh%d", nextId);

        if (append_vehicle_to_file(road, nextId, name, lane)) {
            printf("[%04d] Road %c, Lane %d: %s\n",
                nextId, 'A' + road, lane, name);
        }

        nextId++;
        Sleep(interval_ms);
    }

    return 0;
}