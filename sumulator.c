#define _CRT_SECURE_NO_WARNINGS 
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <math.h>
#include <string.h>

#define SCREEN_W 900
#define SCREEN_H 900
#define ROAD_W 200
#define LANE_W (ROAD_W/3.0f)
#define MAX_QUEUE 50
#define VEHICLE_SPEED 2.0f
#define VEHICLE_SIZE 12
#define STOPPING_DISTANCE 30.0f
#define MIN_SPACING 20.0f
#define MIN_FRONT_SPACING 25.0f
#define sleep_ms(x) Sleep(x)
#define GREEN_LIGHT 0
#define RED_LIGHT 1
#define NAME_MAX 16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char* basedir = "C:\\TrafficShared\\";
const char* files[4] = {
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};

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

// Circular queue for lane management
typedef struct {
    Vehicle data[MAX_QUEUE];
    int front, rear, count;
} Lane;

typedef struct {
    Lane L1;  // Left turn
    Lane L2;  // Straight
    Lane L3;  // Right turn
} RoadData;

RoadData roads[4];  // 0=North, 1=South, 2=East, 3=West
TransitionVehicle transitions[MAX_QUEUE];
int transitionCount = 0;
int currentGreen = 0;
int lightState = GREEN_LIGHT;

// Returns opposite road: N↔S, E↔W
static int getOppositeRoad(int road) {
    switch (road) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 0;
    case 3: return 1;
    }
    return 0;
}

void initLane(Lane* l) {
    l->front = 0;
    l->rear = -1;
    l->count = 0;
}

int enqueue(Lane* l, Vehicle v) {
    if (l->count >= MAX_QUEUE) return 0;
    l->rear = (l->rear + 1) % MAX_QUEUE;
    l->data[l->rear] = v;
    l->count++;
    return 1;
}

static int dequeue(Lane* l, Vehicle* out) {
    if (l->count == 0) return 0;
    *out = l->data[l->front];
    l->front = (l->front + 1) % MAX_QUEUE;
    l->count--;
    return 1;
}

static Vehicle* getLaneVehicle(Lane* l, int index) {
    if (index < 0 || index >= l->count) return NULL;
    int idx = (l->front + index) % MAX_QUEUE;
    return &l->data[idx];
}

float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

// Maps logical lane (1=left, 2=straight, 3=right) to physical lane index
static int lane_index_for(int road, int logicalLane) {
    if (road == 0) {
        if (logicalLane == 3) return 0;
        if (logicalLane == 2) return 1;
        return 2;
    }
    else if (road == 1) {
        if (logicalLane == 3) return 0;
        if (logicalLane == 2) return 1;
        return 2;
    }
    else if (road == 2) {
        if (logicalLane == 3) return 2;
        if (logicalLane == 2) return 1;
        return 0;
    }
    else {
        if (logicalLane == 3) return 2;
        if (logicalLane == 2) return 1;
        return 0;
    }
}

// Calculate spawn position outside the screen
static void spawn_coords_for_fixed(int road, int laneIndex, float* outx, float* outy) {
    float cx = SCREEN_W / 2.0f;
    float cy = SCREEN_H / 2.0f;
    float startx = cx - ROAD_W / 2.0f;
    float starty = cy - ROAD_W / 2.0f;

    if (road == 0) { *outx = startx + LANE_W * (laneIndex + 0.5f); *outy = -30.0f; }
    else if (road == 1) { *outx = SCREEN_W + 30.0f; *outy = starty + LANE_W * (laneIndex + 0.5f); }
    else if (road == 2) { *outx = startx + LANE_W * (laneIndex + 0.5f); *outy = SCREEN_H + 30.0f; }
    else { *outx = -30.0f; *outy = starty + LANE_W * (laneIndex + 0.5f); }
}

// Calculate target position at intersection edge
static void intersection_lane_center(int road, int logicalLane, float* outx, float* outy) {
    float cx = SCREEN_W / 2.0f;
    float cy = SCREEN_H / 2.0f;
    float startx = cx - ROAD_W / 2.0f;
    float starty = cy - ROAD_W / 2.0f;

    int idx = lane_index_for(road, logicalLane);
    if (road == 0) { *outx = startx + LANE_W * (idx + 0.5f); *outy = cy - ROAD_W / 2.0f - 8.0f; }
    else if (road == 1) { *outx = cx + ROAD_W / 2.0f + 8.0f; *outy = starty + LANE_W * (idx + 0.5f); }
    else if (road == 2) { *outx = startx + LANE_W * (idx + 0.5f); *outy = cy + ROAD_W / 2.0f + 8.0f; }
    else { *outx = cx - ROAD_W / 2.0f - 8.0f; *outy = starty + LANE_W * (idx + 0.5f); }
}

// Check if spawning at position would be too close to existing vehicles
static int checkTooCloseInLane(Lane* l, float x, float y, int skipIndex) {
    for (int i = 0; i < l->count; i++) {
        if (i == skipIndex) continue;
        Vehicle* other = getLaneVehicle(l, i);
        if (!other) continue;
        float dist = distance(x, y, other->x, other->y);
        if (dist < MIN_SPACING) return 1;
    }
    return 0;
}

// Calculate distance to next vehicle in same lane
static float getDistanceToVehicleAhead(Lane* L, int road, int vehicleIndex) {
    Vehicle* current = getLaneVehicle(L, vehicleIndex);
    if (!current) return 999999.0f;

    float minDist = 999999.0f;

    for (int i = vehicleIndex + 1; i < L->count; i++) {
        Vehicle* ahead = getLaneVehicle(L, i);
        if (!ahead) continue;

        // Calculate distance along road direction
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

// Get movement direction for Lane 3 (right turn - free flow)
static void l3_move_vector(int road, float* dx, float* dy) {
    if (road == 0) { *dx = 0.0f; *dy = -VEHICLE_SPEED; }
    else if (road == 1) { *dx = VEHICLE_SPEED; *dy = 0.0f; }
    else if (road == 2) { *dx = 0.0f; *dy = VEHICLE_SPEED; }
    else { *dx = -VEHICLE_SPEED; *dy = 0.0f; }
}

// Move Lane 3 vehicles (right turn - no traffic light)
static void moveLaneL3(Lane* L, int road) {
    float dx, dy;
    l3_move_vector(road, &dx, &dy);

    int i = 0;
    while (i < L->count) {
        Vehicle* v = getLaneVehicle(L, i);
        if (!v) {
            i++;
            continue;
        }

        float newx = v->x + dx;
        float newy = v->y + dy;

        // Remove if out of bounds
        if (newx < -100 || newx > SCREEN_W + 100 || newy < -100 || newy > SCREEN_H + 100) {
            Vehicle temp;
            dequeue(L, &temp);
            continue;
        }

        // Check collision with vehicles ahead
        int canMove = 1;
        for (int j = 0; j < L->count; j++) {
            if (i == j) continue;
            Vehicle* other = getLaneVehicle(L, j);
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

// Move vehicles toward intersection center (Lane 1 & 2)
void moveLaneTowardCenter(Lane* L, int road) {
    float mvx = 0.0f, mvy = 0.0f;
    if (road == 0) { mvy = VEHICLE_SPEED; }
    else if (road == 1) { mvx = -VEHICLE_SPEED; }
    else if (road == 2) { mvy = -VEHICLE_SPEED; }
    else { mvx = VEHICLE_SPEED; }

    for (int i = L->count - 1; i >= 0; i--) {
        Vehicle* v = getLaneVehicle(L, i);
        if (!v) continue;

        float cx = SCREEN_W / 2.0f;
        float cy = SCREEN_H / 2.0f;
        float distToIntersection;

        // Calculate distance to intersection
        if (road == 0) distToIntersection = cy - ROAD_W / 2.0f - v->y;
        else if (road == 1) distToIntersection = v->x - (cx + ROAD_W / 2.0f);
        else if (road == 2) distToIntersection = v->y - (cy + ROAD_W / 2.0f);
        else distToIntersection = cx - ROAD_W / 2.0f - v->x;

        // Stop at red light
        if (lightState == RED_LIGHT || currentGreen != road) {
            if (distToIntersection < STOPPING_DISTANCE && distToIntersection > 0) {
                v->isStopped = 1;
                continue;
            }
        }

        float distToAhead = getDistanceToVehicleAhead(L, road, i);

        // Stop behind stopped vehicle
        if (distToAhead < STOPPING_DISTANCE) {
            if (i + 1 < L->count) {
                Vehicle* ahead = getLaneVehicle(L, i + 1);
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

        if (!checkTooCloseInLane(L, newx, newy, i)) {
            v->x = newx;
            v->y = newy;
            v->isStopped = 0;
        }
        else {
            v->isStopped = 1;
        }

        // Remove if out of bounds
        if (v->x < -100 || v->x > SCREEN_W + 100 || v->y < -100 || v->y > SCREEN_H + 100) {
            for (int j = i; j < L->count - 1; j++) {
                L->data[(L->front + j) % MAX_QUEUE] = L->data[(L->front + j + 1) % MAX_QUEUE];
            }
            L->count--;
        }
    }
}

// Add vehicle to intersection transition zone
static void addTransition(Vehicle v, int targetRoad) {
    if (transitionCount >= MAX_QUEUE) return;
    v.isStopped = 0;
    transitions[transitionCount].v = v;
    transitions[transitionCount].targetRoad = targetRoad;
    transitions[transitionCount].waitingTime = 0;
    transitionCount++;
}

// Move vehicles through intersection using priority queue
static void moveTransitions() {
    // Sort by waiting time (priority queue)
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
        intersection_lane_center(tv->targetRoad, 3, &tx, &ty);
        float dx = tx - tv->v.x;
        float dy = ty - tv->v.y;
        float dist = sqrtf(dx * dx + dy * dy);

        float speed = VEHICLE_SPEED;

        // Reached target lane
        if (dist < speed) {
            tv->v.x = tx;
            tv->v.y = ty;
            tv->v.fromRoad = tv->targetRoad;

            if (!checkTooCloseInLane(&roads[tv->targetRoad].L3, tx, ty, -1)) {
                enqueue(&roads[tv->targetRoad].L3, tv->v);

                for (int j = i; j < transitionCount - 1; j++) {
                    transitions[j] = transitions[j + 1];
                }
                transitionCount--;
                i--;
            }
            else {
                tv->v.isStopped = 1;
                // Force merge after timeout
                if (tv->waitingTime > 50) {
                    enqueue(&roads[tv->targetRoad].L3, tv->v);

                    for (int j = i; j < transitionCount - 1; j++) {
                        transitions[j] = transitions[j + 1];
                    }
                    transitionCount--;
                    i--;
                }
            }
        }
        else {
            // Check collision in transition zone
            int canMove = 1;

            for (int j = 0; j < transitionCount; j++) {
                if (i == j) continue;
                float otherDist = distance(tv->v.x, tv->v.y,
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

// Remove stuck vehicles from transition zone
static void cleanupStuckTransitions() {
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

// Draw gradient green background
void drawGradientBackground(SDL_Renderer* renderer) {
    for (int y = 0; y < SCREEN_H; y++) {
        int r = 40 + (y * 30) / SCREEN_H;
        int g = 160 + (y * 20) / SCREEN_H;
        int b = 50 + (y * 20) / SCREEN_H;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_W, y);
    }
}

// Draw decorative trees around roads
void drawLeaves(SDL_Renderer* renderer) {
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;

    int leafPositions[][2] = {
        {80, 80}, {150, 120}, {120, 200},
        {SCREEN_W - 80, 80}, {SCREEN_W - 150, 120}, {SCREEN_W - 120, 200},
        {80, SCREEN_H - 80}, {150, SCREEN_H - 120}, {120, SCREEN_H - 200},
        {SCREEN_W - 80, SCREEN_H - 80}, {SCREEN_W - 150, SCREEN_H - 120}, {SCREEN_W - 120, SCREEN_H - 200},
        {50, cy - 250}, {50, cy + 250},
        {SCREEN_W - 50, cy - 250}, {SCREEN_W - 50, cy + 250},
        {cx - 250, 50}, {cx + 250, 50},
        {cx - 250, SCREEN_H - 50}, {cx + 250, SCREEN_H - 50}
    };

    for (int i = 0; i < sizeof(leafPositions) / sizeof(leafPositions[0]); i++) {
        int x = leafPositions[i][0];
        int y = leafPositions[i][1];

        // Tree trunk
        SDL_SetRenderDrawColor(renderer, 101, 67, 33, 255);
        SDL_Rect trunk = { x - 4, y + 10, 8, 20 };
        SDL_RenderFillRect(renderer, &trunk);

        // Main foliage
        SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
        for (int dy = -15; dy <= 10; dy++) {
            for (int dx = -15; dx <= 15; dx++) {
                if (dx * dx + dy * dy <= 225) {
                    SDL_RenderDrawPoint(renderer, x + dx, y + dy);
                }
            }
        }

        // Highlight
        SDL_SetRenderDrawColor(renderer, 50, 205, 50, 255);
        for (int dy = -10; dy <= 0; dy++) {
            for (int dx = -8; dx <= 8; dx++) {
                if (dx * dx + dy * dy <= 64) {
                    SDL_RenderDrawPoint(renderer, x + dx - 2, y + dy - 2);
                }
            }
        }

        // Shadow
        SDL_SetRenderDrawColor(renderer, 20, 100, 20, 100);
        for (int dy = 5; dy <= 12; dy++) {
            for (int dx = -10; dx <= 10; dx++) {
                if (dx * dx + (dy - 8) * (dy - 8) <= 100) {
                    SDL_RenderDrawPoint(renderer, x + dx + 1, y + dy + 1);
                }
            }
        }
    }
}

void drawRoads(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 45, 45, 48, 255);
    SDL_Rect vert = { (int)(SCREEN_W / 2.0f - ROAD_W / 2.0f), 0, ROAD_W, SCREEN_H };
    SDL_Rect horiz = { 0, (int)(SCREEN_H / 2.0f - ROAD_W / 2.0f), SCREEN_W, ROAD_W };
    SDL_RenderFillRect(renderer, &vert);
    SDL_RenderFillRect(renderer, &horiz);

    // Yellow lane markers
    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
    for (int i = 1; i < 3; i++) {
        int x = (int)(SCREEN_W / 2.0f - ROAD_W / 2.0f + i * LANE_W);
        for (int y = 0; y < SCREEN_H; y += 30) {
            SDL_RenderDrawLine(renderer, x, y, x, y + 15);
        }
        int yPos = (int)(SCREEN_H / 2.0f - ROAD_W / 2.0f + i * LANE_W);
        for (int x = 0; x < SCREEN_W; x += 30) {
            SDL_RenderDrawLine(renderer, x, yPos, x + 15, yPos);
        }
    }

    // Road edges
    SDL_SetRenderDrawColor(renderer, 35, 35, 38, 255);
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;
    int roadHalf = ROAD_W / 2;
    SDL_Rect edgeN = { cx - roadHalf - 2, 0, 2, cy - roadHalf };
    SDL_Rect edgeS = { cx + roadHalf, cy + roadHalf, 2, SCREEN_H - (cy + roadHalf) };
    SDL_Rect edgeE = { cx + roadHalf, cy - roadHalf - 2, SCREEN_W - (cx + roadHalf), 2 };
    SDL_Rect edgeW = { 0, cy + roadHalf, cx - roadHalf, 2 };
    SDL_RenderFillRect(renderer, &edgeN);
    SDL_RenderFillRect(renderer, &edgeS);
    SDL_RenderFillRect(renderer, &edgeE);
    SDL_RenderFillRect(renderer, &edgeW);
}

// Draw car with direction-aware shape
void drawVehicle(SDL_Renderer* renderer, Vehicle* v, int r, int g, int b) {
    if (!v) return;

    int width = 18;
    int height = 12;

    int isVertical = (v->fromRoad == 0 || v->fromRoad == 2);
    if (isVertical) {
        int temp = width;
        width = height;
        height = temp;
    }

    // Darker color if stopped
    if (v->isStopped) {
        SDL_SetRenderDrawColor(renderer, r / 2, g / 2, b / 2, 255);
    }
    else {
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    }

    SDL_Rect body = {
        (int)(v->x - width / 2),
        (int)(v->y - height / 2),
        width,
        height
    };
    SDL_RenderFillRect(renderer, &body);

    SDL_SetRenderDrawColor(renderer, r / 3, g / 3, b / 3, 255);
    SDL_RenderDrawRect(renderer, &body);

    // Windshield
    SDL_SetRenderDrawColor(renderer, 135, 206, 235, 180);
    if (isVertical) {
        SDL_Rect window = { body.x + 2, body.y + 2, body.w - 4, body.h / 3 };
        SDL_RenderFillRect(renderer, &window);
    }
    else {
        SDL_Rect window = { body.x + 2, body.y + 2, body.w / 3, body.h - 4 };
        SDL_RenderFillRect(renderer, &window);
    }
}

void drawLane(SDL_Renderer* renderer, Lane* L, int r, int g, int b) {
    for (int i = 0; i < L->count; i++) {
        Vehicle* v = getLaneVehicle(L, i);
        if (v) {
            drawVehicle(renderer, v, r, g, b);
        }
    }
}

// Draw vehicles in transition zone (yellow/orange)
void drawTransitions(SDL_Renderer* renderer) {
    for (int i = 0; i < transitionCount; i++) {
        Vehicle* v = &transitions[i].v;
        if (v->isStopped) {
            drawVehicle(renderer, v, 128, 90, 0);
        }
        else {
            drawVehicle(renderer, v, 255, 180, 0);
        }
    }
}

// Draw traffic lights with glow effect
void drawTrafficLights(SDL_Renderer* renderer) {
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;
    int roadHalf = ROAD_W / 2;

    int lightPositions[4][2] = {
        {cx - 20, cy - roadHalf - 45},
        {cx + roadHalf + 30, cy - 20},
        {cx - 20, cy + roadHalf + 30},
        {cx - roadHalf - 45, cy - 20}
    };

    for (int i = 0; i < 4; i++) {
        // Pole
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_Rect pole = { lightPositions[i][0] + 15, lightPositions[i][1] + 30, 3, 30 };
        SDL_RenderFillRect(renderer, &pole);

        // Housing
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect housing = { lightPositions[i][0], lightPositions[i][1], 35, 30 };
        SDL_RenderFillRect(renderer, &housing);

        // Light bulb
        if (i == currentGreen && lightState == GREEN_LIGHT) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        }

        int lightX = lightPositions[i][0] + 17;
        int lightY = lightPositions[i][1] + 15;
        for (int dy = -8; dy <= 8; dy++) {
            for (int dx = -8; dx <= 8; dx++) {
                if (dx * dx + dy * dy <= 64) {
                    SDL_RenderDrawPoint(renderer, lightX + dx, lightY + dy);
                }
            }
        }

        // Glow for green lights
                                                                                                                                                                                    if (i == currentGreen && lightState == GREEN_LIGHT) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
            for (int dy = -10; dy <= 10; dy++) {
                for (int dx = -10; dx <= 10; dx++) {
                    if (dx * dx + dy * dy <= 100 && dx * dx + dy * dy > 64) {
                        SDL_RenderDrawPoint(renderer, lightX + dx, lightY + dy);
                    }
                }
            }
        }
    }
}

// Read vehicles from input files (incremental read)
void readVehiclesFromFiles() {
    static int lastReadLines[4] = { 0, 0, 0, 0 };

    for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
        FILE* f = fopen(files[roadIdx], "r");
        if (!f) continue;

        char line[128];
        int currentLine = 0;

        // Skip already read lines
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
                    laneIndex = lane_index_for(roadIdx, 1);
                }
                else if (lane == 2) {
                    targetLane = &roads[roadIdx].L2;
                    laneIndex = lane_index_for(roadIdx, 2);
                }
                else if (lane == 3) {
                    targetLane = &roads[roadIdx].L3;
                    laneIndex = lane_index_for(roadIdx, 3);
                }

                if (targetLane) {
                    float sx, sy;
                    spawn_coords_for_fixed(roadIdx, laneIndex, &sx, &sy);
                    v.x = sx;
                    v.y = sy;

                    if (!checkTooCloseInLane(targetLane, sx, sy, -1)) {
                        enqueue(targetLane, v);
                    }
                }
            }

            lastReadLines[roadIdx]++;
        }

        fclose(f);
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() != 0) {
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Traffic Simulator - Enhanced",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 16);
    srand((unsigned)time(NULL));

    // Initialize all lanes
    for (int i = 0; i < 4; i++) {
        initLane(&roads[i].L1);
        initLane(&roads[i].L2);
        initLane(&roads[i].L3);
    }

    readVehiclesFromFiles();

    int running = 1;
    SDL_Event e;
    Uint32 lastFileCheck = 0;
    Uint32 lastLightChange = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        Uint32 now = SDL_GetTicks();

        // Check for new vehicles every 200ms
        if (now - lastFileCheck >= 200) {
            readVehiclesFromFiles();
            lastFileCheck = now;
        }

        // Change traffic lights every 5 seconds
        if (now - lastLightChange >= 5000) {
            currentGreen = (currentGreen + 1) % 4;
            lastLightChange = now;
        }

        // Update all vehicles
        for (int r = 0; r < 4; r++) {
            moveLaneTowardCenter(&roads[r].L1, r);
            moveLaneTowardCenter(&roads[r].L2, r);
            moveLaneL3(&roads[r].L3, r);
        }

        cleanupStuckTransitions();
        moveTransitions();

        // Transfer vehicles that reached intersection
        if (currentGreen >= 0 && currentGreen < 4 && lightState == GREEN_LIGHT) {
            {
                Lane* L = &roads[currentGreen].L1;
                int cnt = L->count;

                for (int i = 0; i < cnt; i++) {
                    Vehicle* v = getLaneVehicle(L, i);
                    if (!v) continue;

                    float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
                    int reached = 0;

                    if (currentGreen == 0 && v->y >= cy - ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 1 && v->x <= cx + ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 2 && v->y <= cy + ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 3 && v->x >= cx - ROAD_W / 2.0f) reached = 1;

                    if (reached) {
                        Vehicle temp;
                        dequeue(L, &temp);
                        int targetRoad = (currentGreen + 1) % 4;  // Turn left
                        addTransition(temp, targetRoad);
                        i--; cnt--;
                    }
                }
            }

            {
                Lane* L = &roads[currentGreen].L2;
                int cnt = L->count;

                for (int i = 0; i < cnt; i++) {
                    Vehicle* v = getLaneVehicle(L, i);
                    if (!v) continue;

                    float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
                    int reached = 0;

                    if (currentGreen == 0 && v->y >= cy - ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 1 && v->x <= cx + ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 2 && v->y <= cy + ROAD_W / 2.0f) reached = 1;
                    if (currentGreen == 3 && v->x >= cx - ROAD_W / 2.0f) reached = 1;

                    if (reached) {
                        Vehicle temp;
                        dequeue(L, &temp);
                        int targetRoad;
                        int opposite = getOppositeRoad(currentGreen);

                        // 50% straight, 50% turn
                        if (rand() % 2 == 0) {
                            targetRoad = opposite;
                        }
                        else {
                            targetRoad = (currentGreen + 3) % 4;
                        }

                        addTransition(temp, targetRoad);
                        i--; cnt--;
                    }
                }
            }
        }

        // Render scene
        drawGradientBackground(renderer);
        drawLeaves(renderer);
        drawRoads(renderer);

        // Draw vehicles by lane (different colors)
        for (int r = 0; r < 4; r++) {
            drawLane(renderer, &roads[r].L1, 220, 80, 80);   // Red - Left
            drawLane(renderer, &roads[r].L2, 80, 220, 80);   // Green - Straight
            drawLane(renderer, &roads[r].L3, 80, 120, 220);  // Blue - Right
        }

        drawTransitions(renderer);
        drawTrafficLights(renderer);

        SDL_RenderPresent(renderer);
        sleep_ms(16);  // ~60 FPS
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}