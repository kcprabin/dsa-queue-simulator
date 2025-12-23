#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// ============================================================================
// CONFIGURATION - OPTIMIZED VALUES
// ============================================================================

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 900
#define FPS 60

#define NUM_ROADS 4
#define NUM_LANES 3
#define MAX_QUEUE 100
#define MAX_MOVING_CARS 100

#define PRIORITY_TRIGGER 10
#define PRIORITY_EXIT 5
#define PRIORITY_TRIGGER_L1 8
#define PRIORITY_EXIT_L1 3

#define LANE1_MAX_QUEUE 5
#define LANE3_MAX_QUEUE 5

#define PROCESS_TIME 600
#define GREEN_TIME 3000
#define FREE_FLOW_INTERVAL 400

#define LANE_WIDTH 60
#define CAR_WIDTH 30
#define CAR_HEIGHT 45
#define CAR_SPACING 70

// OPTIMIZED PHYSICS
#define CAR_MAX_SPEED 3.0f
#define CAR_ACCELERATION 0.12f
#define CAR_DECELERATION 0.20f
#define TURN_SPEED_FACTOR 0.65f
#define WAYPOINT_THRESHOLD 12.0f
#define MIN_CAR_DISTANCE 60.0f

// DOT RENDERING
#define DOT_RADIUS 5  // Size of vehicle dots

#define FONT_PATH "C:\\Windows\\Fonts\\Arial.ttf"
#define DATA_DIR "C:\\TrafficShared\\"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    float x, y;
} Point2D;

typedef struct {
    Point2D points[16];
    int count;
    int currentIndex;
} VehiclePath;

typedef enum {
    CAR_WAITING,
    CAR_MOVING,
    CAR_EXITED
} CarState;

typedef struct Vehicle {
    char id[20];
    char road;
    int lane;

    float x, y;
    float vx, vy;
    float speed;
    float heading;

    VehiclePath path;
    bool pathAssigned;

    unsigned long long arrivalTime;
    CarState state;
    int colorIdx;

    struct Vehicle* next;
} Vehicle;

typedef struct {
    Vehicle* head;
    Vehicle* tail;
    int count;
} Lane;

typedef struct {
    Vehicle* movingCars[MAX_MOVING_CARS];
    int movingCount;
    CRITICAL_SECTION moveLock;
} MovingCars;

typedef struct {
    Lane lanes[NUM_ROADS][NUM_LANES];
    int greenRoad;
    bool priorityMode;
    int processed;
    int totalWait;
    float avgWait;
    unsigned long long startTime;
    volatile bool running;
    int nextColorIdx;
    CRITICAL_SECTION lock;
} System;

// ============================================================================
// GLOBALS
// ============================================================================

const char* ROADS[] = { "A", "B", "C", "D" };
const char* LANE_FILES[] = {
    DATA_DIR "lanea.txt",
    DATA_DIR "laneb.txt",
    DATA_DIR "lanec.txt",
    DATA_DIR "laned.txt"
};

System sys;
MovingCars movingCars;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

const SDL_Color CAR_COLORS[] = {
    {231, 76, 60, 255},
    {52, 152, 219, 255},
    {46, 204, 113, 255},
    {241, 196, 15, 255},
    {155, 89, 182, 255},
    {230, 126, 34, 255},
};
const int NUM_CAR_COLORS = 6;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

Point2D makePoint(float x, float y) {
    Point2D p;
    p.x = x;
    p.y = y;
    return p;
}

float getDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

bool isPathClear(Vehicle* v) {
    if (!v) return false;

    EnterCriticalSection(&movingCars.moveLock);

    for (int i = 0; i < movingCars.movingCount; i++) {
        Vehicle* other = movingCars.movingCars[i];
        if (!other || other == v) continue;

        float dist = getDistance(v->x, v->y, other->x, other->y);
        if (dist < MIN_CAR_DISTANCE) {
            LeaveCriticalSection(&movingCars.moveLock);
            return false;
        }
    }

    LeaveCriticalSection(&movingCars.moveLock);
    return true;
}

// ============================================================================
// PATH GENERATION
// ============================================================================

void generatePath(Vehicle* v) {
    if (!v) return;

    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;
    int roadWidth = LANE_WIDTH * NUM_LANES;

    int laneOffset;
    if (v->lane == 1) laneOffset = -LANE_WIDTH;
    else if (v->lane == 2) laneOffset = 0;
    else laneOffset = LANE_WIDTH;

    VehiclePath* path = &v->path;
    path->count = 0;
    path->currentIndex = 0;

    Point2D start;

    if (v->road == 'A') {
        start.x = (float)(cx + laneOffset);
        start.y = (float)(cy - roadWidth / 2 - 200);
        path->points[path->count++] = start;

        if (v->lane == 1) {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy - 100));
            path->points[path->count++] = makePoint((float)(cx - 35), (float)(cy - 75));
            path->points[path->count++] = makePoint((float)(cx - 75), (float)(cy - 35));
            path->points[path->count++] = makePoint((float)(cx - 100), (float)cy);
            path->points[path->count++] = makePoint((float)(cx - roadWidth / 2 - 200), (float)cy);
        }
        else if (v->lane == 2) {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy - 80));
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)cy);
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy + 80));
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy + roadWidth / 2 + 200));
        }
        else {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy - 100));
            path->points[path->count++] = makePoint((float)(cx + 35), (float)(cy - 75));
            path->points[path->count++] = makePoint((float)(cx + 75), (float)(cy - 35));
            path->points[path->count++] = makePoint((float)(cx + 100), (float)cy);
            path->points[path->count++] = makePoint((float)(cx + roadWidth / 2 + 200), (float)cy);
        }
    }
    else if (v->road == 'B') {
        start.x = (float)(cx + laneOffset);
        start.y = (float)(cy + roadWidth / 2 + 200);
        path->points[path->count++] = start;

        if (v->lane == 1) {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy + 100));
            path->points[path->count++] = makePoint((float)(cx + 35), (float)(cy + 75));
            path->points[path->count++] = makePoint((float)(cx + 75), (float)(cy + 35));
            path->points[path->count++] = makePoint((float)(cx + 100), (float)cy);
            path->points[path->count++] = makePoint((float)(cx + roadWidth / 2 + 200), (float)cy);
        }
        else if (v->lane == 2) {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy + 80));
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)cy);
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy - 80));
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy - roadWidth / 2 - 200));
        }
        else {
            path->points[path->count++] = makePoint((float)(cx + laneOffset), (float)(cy + 100));
            path->points[path->count++] = makePoint((float)(cx - 35), (float)(cy + 75));
            path->points[path->count++] = makePoint((float)(cx - 75), (float)(cy + 35));
            path->points[path->count++] = makePoint((float)(cx - 100), (float)cy);
            path->points[path->count++] = makePoint((float)(cx - roadWidth / 2 - 200), (float)cy);
        }
    }
    else if (v->road == 'C') {
        start.x = (float)(cx + roadWidth / 2 + 200);
        start.y = (float)(cy + laneOffset);
        path->points[path->count++] = start;

        if (v->lane == 1) {
            path->points[path->count++] = makePoint((float)(cx + 100), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx + 75), (float)(cy - 35));
            path->points[path->count++] = makePoint((float)(cx + 35), (float)(cy - 75));
            path->points[path->count++] = makePoint((float)cx, (float)(cy - 100));
            path->points[path->count++] = makePoint((float)cx, (float)(cy - roadWidth / 2 - 200));
        }
        else if (v->lane == 2) {
            path->points[path->count++] = makePoint((float)(cx + 80), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx - 80), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx - roadWidth / 2 - 200), (float)(cy + laneOffset));
        }
        else {
            path->points[path->count++] = makePoint((float)(cx + 100), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx + 75), (float)(cy + 35));
            path->points[path->count++] = makePoint((float)(cx + 35), (float)(cy + 75));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + 100));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + roadWidth / 2 + 200));
        }
    }
    else {
        start.x = (float)(cx - roadWidth / 2 - 200);
        start.y = (float)(cy + laneOffset);
        path->points[path->count++] = start;

        if (v->lane == 1) {
            path->points[path->count++] = makePoint((float)(cx - 100), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx - 75), (float)(cy + 35));
            path->points[path->count++] = makePoint((float)(cx - 35), (float)(cy + 75));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + 100));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + roadWidth / 2 + 200));
        }
        else if (v->lane == 2) {
            path->points[path->count++] = makePoint((float)(cx - 80), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)cx, (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx + 80), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx + roadWidth / 2 + 200), (float)(cy + laneOffset));
        }
        else {
            path->points[path->count++] = makePoint((float)(cx - 100), (float)(cy + laneOffset));
            path->points[path->count++] = makePoint((float)(cx - 75), (float)(cy - 35));
            path->points[path->count++] = makePoint((float)(cx - 35), (float)(cy - 75));
            path->points[path->count++] = makePoint((float)cx, (float)(cy - 100));
            path->points[path->count++] = makePoint((float)cx, (float)(cy - roadWidth / 2 - 200));
        }
    }

    v->pathAssigned = true;
    v->x = start.x;
    v->y = start.y;

    const char* directions[] = { "", "LEFT", "STRAIGHT", "RIGHT" };
    printf("[PATH] %s: Road %c Lane %d (%s) - %d waypoints - Color %d\n",
        v->id, v->road, v->lane, directions[v->lane], path->count, v->colorIdx);
}

// ============================================================================
// PHYSICS
// ============================================================================

void updateVehiclePhysics(Vehicle* v, float deltaTime) {
    if (!v || !v->pathAssigned || v->path.currentIndex >= v->path.count) {
        if (v) v->state = CAR_EXITED;
        return;
    }

    Point2D target = v->path.points[v->path.currentIndex];

    float dx = target.x - v->x;
    float dy = target.y - v->y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance < WAYPOINT_THRESHOLD) {
        v->path.currentIndex++;

        if (v->path.currentIndex >= v->path.count) {
            v->state = CAR_EXITED;
            return;
        }

        target = v->path.points[v->path.currentIndex];
        dx = target.x - v->x;
        dy = target.y - v->y;
        distance = sqrtf(dx * dx + dy * dy);
    }

    if (distance < 0.1f) return;

    float dirX = dx / distance;
    float dirY = dy / distance;

    float targetSpeed = CAR_MAX_SPEED;

    if (!isPathClear(v)) {
        targetSpeed *= 0.2f;
    }

    if (v->path.currentIndex < v->path.count - 1) {
        Point2D next = v->path.points[v->path.currentIndex + 1];
        float angle = atan2f(next.y - target.y, next.x - target.x) - atan2f(dy, dx);

        while (angle > (float)M_PI) angle -= (float)(2 * M_PI);
        while (angle < -(float)M_PI) angle += (float)(2 * M_PI);

        if (fabsf(angle) > (float)M_PI / 6.0f) {
            targetSpeed *= TURN_SPEED_FACTOR;
        }
    }

    if (v->speed < targetSpeed) {
        v->speed += CAR_ACCELERATION;
        if (v->speed > targetSpeed) v->speed = targetSpeed;
    }
    else if (v->speed > targetSpeed) {
        v->speed -= CAR_DECELERATION;
        if (v->speed < targetSpeed) v->speed = targetSpeed;
    }

    v->vx = dirX * v->speed;
    v->vy = dirY * v->speed;

    v->x += v->vx;
    v->y += v->vy;

    v->heading = atan2f(v->vy, v->vx);
}

void updateMovingCars() {
    EnterCriticalSection(&movingCars.moveLock);

    for (int i = 0; i < movingCars.movingCount; i++) {
        Vehicle* v = movingCars.movingCars[i];

        if (!v) {
            for (int j = i; j < movingCars.movingCount - 1; j++) {
                movingCars.movingCars[j] = movingCars.movingCars[j + 1];
            }
            movingCars.movingCount--;
            i--;
            continue;
        }

        updateVehiclePhysics(v, 1.0f / FPS);

        if (v->state == CAR_EXITED) {
            printf("[EXIT] %s completed (Road %c, Lane %d, Color %d)\n",
                v->id, v->road, v->lane, v->colorIdx);
            free(v);

            for (int j = i; j < movingCars.movingCount - 1; j++) {
                movingCars.movingCars[j] = movingCars.movingCars[j + 1];
            }
            movingCars.movingCount--;
            i--;
        }
    }

    LeaveCriticalSection(&movingCars.moveLock);
}

// ============================================================================
// VEHICLE MANAGEMENT
// ============================================================================

void addMovingCar(Vehicle* v) {
    if (!v) return;

    EnterCriticalSection(&movingCars.moveLock);

    if (movingCars.movingCount < MAX_MOVING_CARS) {
        v->state = CAR_MOVING;

        if (!v->pathAssigned) {
            generatePath(v);
        }

        v->speed = 0.0f;
        v->vx = v->vy = 0.0f;

        movingCars.movingCars[movingCars.movingCount++] = v;

        const char* laneType[] = { "", "LEFT", "STRAIGHT", "RIGHT" };
        printf("[START] %s (Road %c, Lane %d %s, Color %d)\n",
            v->id, v->road, v->lane, laneType[v->lane], v->colorIdx);
    }
    else {
        printf("[ERROR] Buffer full, dropping %s\n", v->id);
        free(v);
    }

    LeaveCriticalSection(&movingCars.moveLock);
}

void addVehicle(Lane* lane, const char* id, char road, int laneNum) {
    if (!lane || lane->count >= MAX_QUEUE) return;

    Vehicle* v = (Vehicle*)malloc(sizeof(Vehicle));
    if (!v) {
        printf("[ERROR] Memory allocation failed for vehicle %s\n", id);
        return;
    }

    memset(v, 0, sizeof(Vehicle));

    strncpy(v->id, id, sizeof(v->id) - 1);
    v->id[sizeof(v->id) - 1] = '\0';
    v->road = road;
    v->lane = laneNum;
    v->arrivalTime = GetTickCount64();
    v->state = CAR_WAITING;

    v->colorIdx = sys.nextColorIdx % NUM_CAR_COLORS;
    sys.nextColorIdx = (sys.nextColorIdx + 1) % NUM_CAR_COLORS;

    v->pathAssigned = false;
    v->x = v->y = -100;
    v->speed = 0.0f;
    v->next = NULL;

    if (lane->tail) {
        lane->tail->next = v;
        lane->tail = v;
    }
    else {
        lane->head = lane->tail = v;
    }
    lane->count++;
}

Vehicle* removeVehicle(Lane* lane) {
    if (!lane || !lane->head) return NULL;

    Vehicle* v = lane->head;
    lane->head = v->next;
    if (!lane->head) lane->tail = NULL;
    lane->count--;
    return v;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool initSystem() {
    srand((unsigned int)time(NULL));

    if (CreateDirectoryA(DATA_DIR, NULL) == 0) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            printf("Warning: Could not create directory %s (Error: %lu)\n", DATA_DIR, err);
        }
    }

    for (int i = 0; i < NUM_ROADS; i++) {
        FILE* f = fopen(LANE_FILES[i], "w");
        if (f) {
            fclose(f);
        }
        else {
            printf("Warning: Could not create file %s\n", LANE_FILES[i]);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() < 0) {
        printf("TTF Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    window = SDL_CreateWindow("Traffic Simulator - Dot Visualization",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    font = TTF_OpenFont(FONT_PATH, 12);
    if (!font) {
        printf("Font Warning: Could not load %s\n", FONT_PATH);
    }

    memset(&sys, 0, sizeof(System));
    memset(&movingCars, 0, sizeof(MovingCars));
    sys.greenRoad = -1;
    sys.running = true;
    sys.startTime = GetTickCount64();
    sys.nextColorIdx = 0;
    InitializeCriticalSection(&sys.lock);
    InitializeCriticalSection(&movingCars.moveLock);

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║   TRAFFIC SIMULATOR - DOT VISUALIZATION ⚪       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\nVISUALIZATION:\n");
    printf("  • Vehicles displayed as white dots\n");
    printf("  • Simplified, clean visualization\n");
    printf("  • Easy to track movement patterns\n");
    printf("\nLane Configuration:\n");
    printf("  Lane 1: LEFT TURN (free flow)\n");
    printf("  Lane 2: STRAIGHT (controlled by lights)\n");
    printf("  Lane 3: RIGHT TURN (free flow)\n");
    printf("══════════════════════════════════════════════════\n\n");

    return true;
}

void cleanup() {
    printf("Cleaning up resources...\n");

    sys.running = false;
    Sleep(200);

    EnterCriticalSection(&sys.lock);
    for (int r = 0; r < NUM_ROADS; r++) {
        for (int l = 0; l < NUM_LANES; l++) {
            Vehicle* v = sys.lanes[r][l].head;
            while (v) {
                Vehicle* next = v->next;
                free(v);
                v = next;
            }
            sys.lanes[r][l].head = NULL;
            sys.lanes[r][l].tail = NULL;
            sys.lanes[r][l].count = 0;
        }
    }
    LeaveCriticalSection(&sys.lock);

    EnterCriticalSection(&movingCars.moveLock);
    for (int i = 0; i < movingCars.movingCount; i++) {
        if (movingCars.movingCars[i]) {
            free(movingCars.movingCars[i]);
            movingCars.movingCars[i] = NULL;
        }
    }
    movingCars.movingCount = 0;
    LeaveCriticalSection(&movingCars.moveLock);

    DeleteCriticalSection(&sys.lock);
    DeleteCriticalSection(&movingCars.moveLock);

    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    printf("Cleanup complete.\n");
}

// ============================================================================
// TRAFFIC LOGIC
// ============================================================================

int calculateServed(int road) {
    int total = 0;
    for (int i = 0; i < NUM_ROADS; i++) {
        total += sys.lanes[i][1].count;
    }
    if (total == 0) return 1;

    int base = total / NUM_ROADS;
    return (base < 1) ? 1 : (base > 10 ? 10 : base);
}

void processVehicle(int road, int lane, const char* type) {
    if (road < 0 || road >= NUM_ROADS || lane < 0 || lane >= NUM_LANES) return;

    Vehicle* v = removeVehicle(&sys.lanes[road][lane]);
    if (!v) return;

    unsigned long long wait = GetTickCount64() - v->arrivalTime;
    int waitSec = (int)(wait / 1000);

    sys.totalWait += waitSec;
    sys.processed++;
    sys.avgWait = (float)sys.totalWait / sys.processed;

    printf("[%s-L%d %s] %s | Wait: %ds | Color: %d\n",
        ROADS[road], lane + 1, type, v->id, waitSec, v->colorIdx);

    addMovingCar(v);
}

// ============================================================================
// THREADS
// ============================================================================

DWORD WINAPI freeFlowOuterLanes(LPVOID arg) {
    (void)arg;

    while (sys.running) {
        EnterCriticalSection(&sys.lock);

        for (int r = 0; r < NUM_ROADS; r++) {
            if (sys.lanes[r][0].count > 0 && sys.running) {
                processVehicle(r, 0, "FREE-L1");
            }

            if (sys.lanes[r][2].count > 0 && sys.running) {
                processVehicle(r, 2, "FREE-L3");
            }
        }

        LeaveCriticalSection(&sys.lock);
        Sleep(FREE_FLOW_INTERVAL);
    }

    printf("[THREAD] freeFlowOuterLanes exiting...\n");
    return 0;
}

DWORD WINAPI mainController(LPVOID arg) {
    (void)arg;

    int lastRoad = 3;

    while (sys.running) {
        EnterCriticalSection(&sys.lock);

        int al2 = sys.lanes[0][1].count;

        if (al2 > PRIORITY_TRIGGER || (sys.priorityMode && al2 > PRIORITY_EXIT)) {
            if (!sys.priorityMode) {
                sys.priorityMode = true;
                printf("\n╔═══════════════════════════════════════╗\n");
                printf("║  PRIORITY MODE - Road A Lane 2       ║\n");
                printf("║  Count: %2d (Threshold: %d)           ║\n", al2, PRIORITY_TRIGGER);
                printf("╚═══════════════════════════════════════╝\n\n");
            }

            sys.greenRoad = 0;
            while (sys.lanes[0][1].count > PRIORITY_EXIT && sys.running) {
                processVehicle(0, 1, "PRIORITY-L2");
                LeaveCriticalSection(&sys.lock);
                Sleep(PROCESS_TIME);
                EnterCriticalSection(&sys.lock);
            }

            sys.priorityMode = false;
            printf("\n>>> Priority ended <<<\n\n");
            lastRoad = 0;
        }
        else {
            lastRoad = (lastRoad + 1) % NUM_ROADS;
            sys.greenRoad = lastRoad;

            int toServe = calculateServed(lastRoad);
            int available = sys.lanes[lastRoad][1].count;
            int serve = (available < toServe) ? available : toServe;

            if (serve > 0) {
                printf("\n[LIGHT] Road %s GREEN - Serving %d\n",
                    ROADS[lastRoad], serve);
            }

            for (int i = 0; i < serve && sys.running; i++) {
                processVehicle(lastRoad, 1, "CONTROLLED");
                LeaveCriticalSection(&sys.lock);
                Sleep(PROCESS_TIME);
                EnterCriticalSection(&sys.lock);
            }

            LeaveCriticalSection(&sys.lock);
            Sleep(GREEN_TIME);
            EnterCriticalSection(&sys.lock);
        }

        LeaveCriticalSection(&sys.lock);
        Sleep(100);
    }

    printf("[THREAD] mainController exiting...\n");
    return 0;
}

DWORD WINAPI fileReader(LPVOID arg) {
    (void)arg;

    char line[100];
    int id, lane;
    char name[20];

    while (sys.running) {
        for (int r = 0; r < NUM_ROADS; r++) {
            if (!sys.running) break;

            FILE* f = fopen(LANE_FILES[r], "r");
            if (!f) continue;

            int count = 0;
            while (fgets(line, sizeof(line), f) && sys.running) {
                if (sscanf(line, "%d %19s %d", &id, name, &lane) == 3) {
                    if (lane >= 1 && lane <= 3) {
                        EnterCriticalSection(&sys.lock);
                        addVehicle(&sys.lanes[r][lane - 1], name, ROADS[r][0], lane);
                        LeaveCriticalSection(&sys.lock);
                        count++;
                    }
                }
            }
            fclose(f);

            if (count > 0) {
                f = fopen(LANE_FILES[r], "w");
                if (f) fclose(f);
                printf("[INPUT] Road %s: +%d vehicles\n", ROADS[r], count);
            }
        }
        Sleep(1000);
    }

    printf("[THREAD] fileReader exiting...\n");
    return 0;
}

// ============================================================================
// RENDERING - SIMPLIFIED WITH WHITE DOTS
// ============================================================================

void drawTree(SDL_Renderer* rend, int x, int y, int size) {
    if (!rend) return;

    SDL_SetRenderDrawColor(rend, 101, 67, 33, 255);
    SDL_Rect trunk = { x - size / 6, y, size / 3, size / 2 };
    SDL_RenderFillRect(rend, &trunk);

    SDL_SetRenderDrawColor(rend, 34, 139, 34, 255);

    for (int layer = 0; layer < 3; layer++) {
        int radius = size / 2 - layer * size / 8;
        int offsetY = y - size / 4 - layer * size / 6;

        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderDrawPoint(rend, x + dx, offsetY + dy);
                }
            }
        }
    }
}

// MODIFIED: Simple white dot instead of detailed car
void drawCar(int x, int y, float heading, SDL_Color color) {
    if (!renderer) return;

    // Draw as white circle (dot) - ignores color and heading parameters
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // White

    // Draw filled circle
    for (int dy = -DOT_RADIUS; dy <= DOT_RADIUS; dy++) {
        for (int dx = -DOT_RADIUS; dx <= DOT_RADIUS; dx++) {
            if (dx * dx + dy * dy <= DOT_RADIUS * DOT_RADIUS) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

void drawLight(int x, int y, bool green) {
    if (!renderer) return;

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect pole = { x + 15, y + 35, 3, 40 };
    SDL_RenderFillRect(renderer, &pole);

    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_Rect box = { x, y, 35, 35 };
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, green ? 46 : 231, green ? 204 : 76, green ? 113 : 60, 255);
    SDL_Rect light = { x + 8, y + 8, 19, 19 };
    SDL_RenderFillRect(renderer, &light);
}

void render() {
    if (!renderer) return;

    EnterCriticalSection(&sys.lock);

    SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
    SDL_RenderClear(renderer);

    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;
    int roadWidth = LANE_WIDTH * NUM_LANES;

    drawTree(renderer, 80, 80, 50);
    drawTree(renderer, WINDOW_WIDTH - 80, 80, 50);
    drawTree(renderer, 80, WINDOW_HEIGHT - 80, 50);
    drawTree(renderer, WINDOW_WIDTH - 80, WINDOW_HEIGHT - 80, 50);
    drawTree(renderer, cx - roadWidth / 2 - 100, cy - roadWidth / 2 - 100, 55);
    drawTree(renderer, cx + roadWidth / 2 + 100, cy - roadWidth / 2 - 100, 55);
    drawTree(renderer, cx - roadWidth / 2 - 100, cy + roadWidth / 2 + 100, 55);
    drawTree(renderer, cx + roadWidth / 2 + 100, cy + roadWidth / 2 + 100, 55);

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect vRoad = { cx - roadWidth / 2, 0, roadWidth, WINDOW_HEIGHT };
    SDL_RenderFillRect(renderer, &vRoad);
    SDL_Rect hRoad = { 0, cy - roadWidth / 2, WINDOW_WIDTH, roadWidth };
    SDL_RenderFillRect(renderer, &hRoad);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int l = 1; l < NUM_LANES; l++) {
        int offset = l * LANE_WIDTH - roadWidth / 2;

        for (int i = 0; i < WINDOW_HEIGHT; i += 30) {
            if (i < cy - roadWidth / 2 - 10 || i > cy + roadWidth / 2 + 10) {
                SDL_Rect dash = { cx + offset - 1, i, 2, 15 };
                SDL_RenderFillRect(renderer, &dash);
            }
        }

        for (int i = 0; i < WINDOW_WIDTH; i += 30) {
            if (i < cx - roadWidth / 2 - 10 || i > cx + roadWidth / 2 + 10) {
                SDL_Rect dash = { i, cy + offset - 1, 15, 2 };
                SDL_RenderFillRect(renderer, &dash);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
    for (int i = 0; i < WINDOW_HEIGHT; i += 30) {
        SDL_Rect dash = { cx - 2, i, 4, 15 };
        SDL_RenderFillRect(renderer, &dash);
    }
    for (int i = 0; i < WINDOW_WIDTH; i += 30) {
        SDL_Rect dash = { i, cy - 2, 15, 4 };
        SDL_RenderFillRect(renderer, &dash);
    }

    drawLight(cx - 18, cy - roadWidth / 2 - 60, sys.greenRoad == 0);
    drawLight(cx - 18, cy + roadWidth / 2 + 25, sys.greenRoad == 1);
    drawLight(cx + roadWidth / 2 + 25, cy - 18, sys.greenRoad == 2);
    drawLight(cx - roadWidth / 2 - 60, cy - 18, sys.greenRoad == 3);

    // Draw waiting cars as white dots
    for (int r = 0; r < NUM_ROADS; r++) {
        for (int l = 0; l < NUM_LANES; l++) {
            Vehicle* v = sys.lanes[r][l].head;
            int pos = 0;

            while (v && pos < 10) {
                int x, y;
                int laneOffset;
                if (l == 0) laneOffset = -LANE_WIDTH;
                else if (l == 1) laneOffset = 0;
                else laneOffset = LANE_WIDTH;

                if (r == 0) {
                    x = cx + laneOffset;
                    y = cy - roadWidth / 2 - 80 - (pos * CAR_SPACING);
                }
                else if (r == 1) {
                    x = cx + laneOffset;
                    y = cy + roadWidth / 2 + 80 + (pos * CAR_SPACING);
                }
                else if (r == 2) {
                    x = cx + roadWidth / 2 + 80 + (pos * CAR_SPACING);
                    y = cy + laneOffset;
                }
                else {
                    x = cx - roadWidth / 2 - 80 - (pos * CAR_SPACING);
                    y = cy + laneOffset;
                }

                float heading = (r == 0 || r == 1) ?
                    ((r == 0) ? -(float)M_PI / 2.0f : (float)M_PI / 2.0f) :
                    ((r == 2) ? (float)M_PI : 0.0f);

                int safeColorIdx = (v->colorIdx >= 0 && v->colorIdx < NUM_CAR_COLORS) ?
                    v->colorIdx : 0;
                drawCar(x, y, heading, CAR_COLORS[safeColorIdx]);

                v = v->next;
                pos++;
            }
        }
    }

    LeaveCriticalSection(&sys.lock);

    // Draw moving cars as white dots
    EnterCriticalSection(&movingCars.moveLock);
    for (int i = 0; i < movingCars.movingCount; i++) {
        Vehicle* v = movingCars.movingCars[i];
        if (v) {
            int safeColorIdx = (v->colorIdx >= 0 && v->colorIdx < NUM_CAR_COLORS) ?
                v->colorIdx : 0;
            drawCar((int)v->x, (int)v->y, v->heading, CAR_COLORS[safeColorIdx]);
        }
    }
    LeaveCriticalSection(&movingCars.moveLock);

    SDL_RenderPresent(renderer);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Traffic Simulator Starting...\n");

    if (!initSystem()) {
        printf("Failed to initialize! Press Enter to exit...\n");
        getchar();
        return -1;
    }

    HANDLE threads[3] = { NULL, NULL, NULL };
    threads[0] = CreateThread(NULL, 0, mainController, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, freeFlowOuterLanes, NULL, 0, NULL);
    threads[2] = CreateThread(NULL, 0, fileReader, NULL, 0, NULL);

    if (!threads[0] || !threads[1] || !threads[2]) {
        printf("Failed to create threads!\n");
        sys.running = false;

        for (int i = 0; i < 3; i++) {
            if (threads[i]) {
                WaitForSingleObject(threads[i], 2000);
                CloseHandle(threads[i]);
            }
        }

        cleanup();
        return -1;
    }

    printf("System running. Press ESC to exit.\n");
    printf("Vehicles shown as white dots ⚪\n\n");

    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        updateMovingCars();
        render();
        SDL_Delay(1000 / FPS);
    }

    printf("\nShutting down gracefully...\n");
    sys.running = false;

    Sleep(500);

    HANDLE handles[3] = { threads[0], threads[1], threads[2] };
    DWORD waitResult = WaitForMultipleObjects(3, handles, TRUE, 5000);

    if (waitResult == WAIT_TIMEOUT) {
        printf("Warning: Some threads did not finish in time.\n");
        for (int i = 0; i < 3; i++) {
            if (threads[i]) {
                TerminateThread(threads[i], 1);
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        if (threads[i]) {
            CloseHandle(threads[i]);
        }
    }

    cleanup();

    printf("\n═══════════════════════════════════════\n");
    printf("Final Statistics:\n");
    printf("  Total processed: %d vehicles\n", sys.processed);
    printf("  Average wait: %.2fs\n", sys.avgWait);
    printf("  Runtime: %.2fs\n", (GetTickCount64() - sys.startTime) / 1000.0);
    printf("═══════════════════════════════════════\n");
    printf("Shutdown complete. Press Enter to exit...\n");
    getchar();

    return 0;
}