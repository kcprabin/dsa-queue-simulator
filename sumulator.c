#define _CRT_SECURE_NO_WARNINGS
#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_LINE_LENGTH 50
#define NAME_MAX 16
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 1000
#define ROAD_WIDTH 200
#define LANE_WIDTH 66
#define MAX_QUEUE_SIZE 100
#define PRIORITY_THRESHOLD 10
#define PRIORITY_MIN 5
#define MAIN_FONT "C:\\Windows\\Fonts\\Arial.ttf"
#define TARGET_FPS 60
#define FRAME_DELAY (1000 / TARGET_FPS)

#define FREE_FLOW_MAX_VEHICLES 8
#define LEFT_TURN_WAIT_TIME 2000
#define STRAIGHT_GREEN_TIME 4000
#define VEHICLE_PROCESS_TIME 500
#define VEHICLE_DOT_SIZE 8
#define VEHICLE_SPACING 45.0f
#define VEHICLE_SPEED 1.8f

const char* LANE_FILES[4] = {
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};

const char ROAD_MAP[4] = { 'A', 'B', 'C', 'D' };

typedef struct VehicleNode {
    char vehicleNumber[NAME_MAX];
    char road;
    int lane;
    float currentX;
    float currentY;
    float targetX;
    float targetY;
    bool isAnimating;
    ULONGLONG arrivalTime;
    struct VehicleNode* next;
} VehicleNode;

typedef struct {
    VehicleNode* front;
    VehicleNode* rear;
    int count;
} Queue;

typedef struct {
    Queue laneQueues[4][3];
    int currentGreenRoad;
    int nextGreenRoad;
    bool isPriorityMode;
    CRITICAL_SECTION cs;
    int totalVehiclesProcessed;
    int totalWaitTime;
    float avgWaitTime;
} TrafficState;

bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer);
void initQueues(TrafficState* state);
void enqueue(Queue* q, const char* vehicleNum, char road, int lane);
VehicleNode* dequeue(Queue* q);
void clearQueue(Queue* q);
void updateVehiclePositions(TrafficState* state);
void calculateVehiclePosition(int roadIdx, int laneIdx, int queuePosition, float* x, float* y);
static void clearFile(const char* filepath);
void drawBackground(SDL_Renderer* renderer);
void drawRoads(SDL_Renderer* renderer);
void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y, SDL_Color color);
void drawTrafficLights(SDL_Renderer* renderer, TrafficState* state);
void drawQueueInfo(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
void drawVehicles(SDL_Renderer* renderer, TrafficState* state);
void drawStatistics(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
void drawVehicleDot(SDL_Renderer* renderer, int x, int y);
void refreshDisplay(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
DWORD WINAPI trafficController(LPVOID arg);
DWORD WINAPI leftTurnController(LPVOID arg);
DWORD WINAPI freeFlowController(LPVOID arg);
DWORD WINAPI readAndParseFile(LPVOID arg);
int calculateOptimalVServed(TrafficState* state, int roadIdx);

int main() {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Event event;

    if (!initializeSDL(&window, &renderer)) return -1;

    TrafficState state;
    initQueues(&state);
    state.currentGreenRoad = -1;
    state.nextGreenRoad = -1;
    state.isPriorityMode = false;
    state.totalVehiclesProcessed = 0;
    state.totalWaitTime = 0;
    state.avgWaitTime = 0.0f;
    InitializeCriticalSection(&state.cs);

    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 14);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
    }

    printf("=== Enhanced 3-Lane Traffic Simulator @ 60 FPS ===\n");
    printf("Lane 1: Left Turn | Lane 2: Straight | Lane 3: Free Flow\n");
    printf("Vehicles: White dots with smooth animation\n");
    printf("=================================================\n\n");

    HANDLE hController = CreateThread(NULL, 0, trafficController, &state, 0, NULL);
    HANDLE hLeftTurn = CreateThread(NULL, 0, leftTurnController, &state, 0, NULL);
    HANDLE hFreeFlow = CreateThread(NULL, 0, freeFlowController, &state, 0, NULL);
    HANDLE hReader = CreateThread(NULL, 0, readAndParseFile, &state, 0, NULL);

    bool running = true;
    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        updateVehiclePositions(&state);
        refreshDisplay(renderer, font, &state);

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }

    if (hController) CloseHandle(hController);
    if (hLeftTurn) CloseHandle(hLeftTurn);
    if (hFreeFlow) CloseHandle(hFreeFlow);
    if (hReader) CloseHandle(hReader);

    DeleteCriticalSection(&state.cs);
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

void initQueues(TrafficState* state) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            state->laneQueues[i][j].front = NULL;
            state->laneQueues[i][j].rear = NULL;
            state->laneQueues[i][j].count = 0;
        }
    }
}

void calculateVehiclePosition(int roadIdx, int laneIdx, int queuePosition, float* x, float* y) {
    float offset = queuePosition * VEHICLE_SPACING;
    int centerX = WINDOW_WIDTH / 2;
    int centerY = WINDOW_HEIGHT / 2;
    int laneOffset = (laneIdx - 1) * LANE_WIDTH;

    switch (roadIdx) {
    case 0:
        *x = centerX + laneOffset;
        *y = centerY - ROAD_WIDTH / 2 - 50 - offset;
        break;
    case 1:
        *x = centerX + laneOffset;
        *y = centerY + ROAD_WIDTH / 2 + 50 + offset;
        break;
    case 2:
        *x = centerX + ROAD_WIDTH / 2 + 50 + offset;
        *y = centerY + laneOffset;
        break;
    case 3:
        *x = centerX - ROAD_WIDTH / 2 - 50 - offset;
        *y = centerY + laneOffset;
        break;
    }
}

void enqueue(Queue* q, const char* vehicleNum, char road, int lane) {
    if (q->count >= MAX_QUEUE_SIZE) return;

    VehicleNode* newNode = (VehicleNode*)malloc(sizeof(VehicleNode));
    if (!newNode) return;

    strncpy(newNode->vehicleNumber, vehicleNum, NAME_MAX - 1);
    newNode->vehicleNumber[NAME_MAX - 1] = '\0';
    newNode->road = road;
    newNode->lane = lane;
    newNode->arrivalTime = GetTickCount64();
    newNode->currentX = 0.0f;
    newNode->currentY = 0.0f;
    newNode->targetX = 0.0f;
    newNode->targetY = 0.0f;
    newNode->isAnimating = false;
    newNode->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    }
    else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->count++;
}

VehicleNode* dequeue(Queue* q) {
    if (!q->front) return NULL;

    VehicleNode* temp = q->front;
    q->front = q->front->next;
    if (!q->front) q->rear = NULL;
    q->count--;
    return temp;
}

void clearQueue(Queue* q) {
    while (q->front) {
        VehicleNode* temp = dequeue(q);
        free(temp);
    }
}

void updateVehiclePositions(TrafficState* state) {
    EnterCriticalSection(&state->cs);

    for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
        for (int laneIdx = 0; laneIdx < 3; laneIdx++) {
            VehicleNode* current = state->laneQueues[roadIdx][laneIdx].front;
            int position = 0;

            while (current) {
                float targetX, targetY;
                calculateVehiclePosition(roadIdx, laneIdx, position, &targetX, &targetY);

                if (!current->isAnimating) {
                    current->currentX = targetX;
                    current->currentY = targetY;
                    current->isAnimating = true;
                }

                current->targetX = targetX;
                current->targetY = targetY;

                float dx = current->targetX - current->currentX;
                float dy = current->targetY - current->currentY;
                float distance = sqrtf(dx * dx + dy * dy);

                if (distance > 0.5f) {
                    float speed = VEHICLE_SPEED;
                    if (distance < 10.0f) speed *= (distance / 10.0f);

                    float dirX = dx / distance;
                    float dirY = dy / distance;

                    current->currentX += dirX * speed;
                    current->currentY += dirY * speed;
                }
                else {
                    current->currentX = current->targetX;
                    current->currentY = current->targetY;
                }

                current = current->next;
                position++;
            }
        }
    }

    LeaveCriticalSection(&state->cs);
}

static void clearFile(const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (f) fclose(f);
}

int calculateOptimalVServed(TrafficState* state, int roadIdx) {
    int totalWaiting = 0;
    int maxQueue = 0;

    for (int i = 0; i < 4; i++) {
        int roadTotal = state->laneQueues[i][0].count + state->laneQueues[i][1].count;
        totalWaiting += roadTotal;
        if (roadTotal > maxQueue) maxQueue = roadTotal;
    }

    if (totalWaiting == 0) return 1;

    int currentRoadTotal = state->laneQueues[roadIdx][0].count + state->laneQueues[roadIdx][1].count;
    int baseServed = totalWaiting / 4;

    if (currentRoadTotal > maxQueue * 0.8) {
        baseServed = (int)(baseServed * 1.5);
    }

    return (baseServed < 1) ? 1 : (baseServed > 10 ? 10 : baseServed);
}

bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL Init Error: %s", SDL_GetError());
        return false;
    }

    if (TTF_Init() < 0) {
        SDL_Log("TTF Init Error: %s", TTF_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Enhanced Traffic Simulator @ 60 FPS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

    if (!*window) {
        SDL_Log("Window Error: %s", SDL_GetError());
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        SDL_Log("Renderer Error: %s", SDL_GetError());
        return false;
    }

    return true;
}

void drawBackground(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < WINDOW_HEIGHT; i += 20) {
        for (int j = 0; j < WINDOW_WIDTH; j += 20) {
            if ((i + j) % 40 == 0) {
                SDL_SetRenderDrawColor(renderer, 28, 120, 28, 255);
                SDL_Rect grass = { j, i, 10, 10 };
                SDL_RenderFillRect(renderer, &grass);
            }
        }
    }
}

void drawRoads(SDL_Renderer* renderer) {
    int centerX = WINDOW_WIDTH / 2;
    int centerY = WINDOW_HEIGHT / 2;

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect verticalRoad = { centerX - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_RenderFillRect(renderer, &verticalRoad);

    SDL_Rect horizontalRoad = { 0, centerY - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &horizontalRoad);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect junction = { centerX - ROAD_WIDTH / 2, centerY - ROAD_WIDTH / 2, ROAD_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &junction);

    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
    for (int i = 0; i < WINDOW_HEIGHT; i += 40) {
        if (i < centerY - ROAD_WIDTH / 2 || i > centerY + ROAD_WIDTH / 2) {
            SDL_Rect dash = { centerX - 3, i, 6, 20 };
            SDL_RenderFillRect(renderer, &dash);
        }
    }

    for (int i = 0; i < WINDOW_WIDTH; i += 40) {
        if (i < centerX - ROAD_WIDTH / 2 || i > centerX + ROAD_WIDTH / 2) {
            SDL_Rect dash = { i, centerY - 3, 20, 6 };
            SDL_RenderFillRect(renderer, &dash);
        }
    }

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    for (int i = 1; i < 3; i++) {
        int yOffset = centerY - ROAD_WIDTH / 2 + i * LANE_WIDTH;
        SDL_RenderDrawLine(renderer, 0, yOffset, centerX - ROAD_WIDTH / 2, yOffset);
        SDL_RenderDrawLine(renderer, centerX + ROAD_WIDTH / 2, yOffset, WINDOW_WIDTH, yOffset);

        int xOffset = centerX - ROAD_WIDTH / 2 + i * LANE_WIDTH;
        SDL_RenderDrawLine(renderer, xOffset, 0, xOffset, centerY - ROAD_WIDTH / 2);
        SDL_RenderDrawLine(renderer, xOffset, centerY + ROAD_WIDTH / 2, xOffset, WINDOW_HEIGHT);
    }
}

void drawVehicleDot(SDL_Renderer* renderer, int x, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int dy = -VEHICLE_DOT_SIZE / 2; dy <= VEHICLE_DOT_SIZE / 2; dy++) {
        for (int dx = -VEHICLE_DOT_SIZE / 2; dx <= VEHICLE_DOT_SIZE / 2; dx++) {
            if (dx * dx + dy * dy <= (VEHICLE_DOT_SIZE / 2) * (VEHICLE_DOT_SIZE / 2)) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    for (int dy = -VEHICLE_DOT_SIZE / 2; dy <= VEHICLE_DOT_SIZE / 2; dy++) {
        for (int dx = -VEHICLE_DOT_SIZE / 2; dx <= VEHICLE_DOT_SIZE / 2; dx++) {
            int dist = dx * dx + dy * dy;
            if (dist <= (VEHICLE_DOT_SIZE / 2) * (VEHICLE_DOT_SIZE / 2) &&
                dist > ((VEHICLE_DOT_SIZE / 2) - 1) * ((VEHICLE_DOT_SIZE / 2) - 1)) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

void drawVehicles(SDL_Renderer* renderer, TrafficState* state) {
    EnterCriticalSection(&state->cs);

    for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
        for (int laneIdx = 0; laneIdx < 3; laneIdx++) {
            VehicleNode* current = state->laneQueues[roadIdx][laneIdx].front;
            int displayCount = 0;

            while (current && displayCount < 10) {
                int x = (int)current->currentX;
                int y = (int)current->currentY;
                drawVehicleDot(renderer, x, y);

                current = current->next;
                displayCount++;
            }
        }
    }

    LeaveCriticalSection(&state->cs);
}

void drawTrafficLight(SDL_Renderer* renderer, int x, int y, bool isGreen) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_Rect pole = { x + 18, y + 40, 4, 50 };
    SDL_RenderFillRect(renderer, &pole);

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect box = { x, y, 40, 40 };
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, isGreen ? 46 : 100, isGreen ? 204 : 20, isGreen ? 113 : 20, 255);
    SDL_Rect light = { x + 10, y + 10, 20, 20 };
    SDL_RenderFillRect(renderer, &light);

    if (isGreen) {
        SDL_SetRenderDrawColor(renderer, 100, 255, 150, 200);
        for (int i = 0; i < 3; i++) {
            SDL_Rect glow = { x + 10 - i, y + 10 - i, 20 + i * 2, 20 + i * 2 };
            SDL_RenderDrawRect(renderer, &glow);
        }
    }
}

void drawTrafficLights(SDL_Renderer* renderer, TrafficState* state) {
    EnterCriticalSection(&state->cs);
    int greenRoad = state->currentGreenRoad;
    LeaveCriticalSection(&state->cs);

    int centerX = WINDOW_WIDTH / 2;
    int centerY = WINDOW_HEIGHT / 2;

    drawTrafficLight(renderer, centerX - 20, centerY - ROAD_WIDTH / 2 - 120, greenRoad == 0);
    drawTrafficLight(renderer, centerX - 20, centerY + ROAD_WIDTH / 2 + 80, greenRoad == 1);
    drawTrafficLight(renderer, centerX + ROAD_WIDTH / 2 + 80, centerY - 20, greenRoad == 2);
    drawTrafficLight(renderer, centerX - ROAD_WIDTH / 2 - 120, centerY - 20, greenRoad == 3);
}

void drawQueueInfo(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    char buffer[100];
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Color black = { 0, 0, 0, 255 };

    EnterCriticalSection(&state->cs);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect boxA = { 20, 20, 250, 60 };
    SDL_RenderFillRect(renderer, &boxA);
    sprintf(buffer, "Road A: L1:%d L2:%d L3:%d",
        state->laneQueues[0][0].count,
        state->laneQueues[0][1].count,
        state->laneQueues[0][2].count);
    displayText(renderer, font, buffer, 30, 35, white);

    SDL_Rect boxB = { 20, WINDOW_HEIGHT - 80, 250, 60 };
    SDL_RenderFillRect(renderer, &boxB);
    sprintf(buffer, "Road B: L1:%d L2:%d L3:%d",
        state->laneQueues[1][0].count,
        state->laneQueues[1][1].count,
        state->laneQueues[1][2].count);
    displayText(renderer, font, buffer, 30, WINDOW_HEIGHT - 65, white);

    SDL_Rect boxC = { WINDOW_WIDTH - 270, WINDOW_HEIGHT / 2 - 30, 250, 60 };
    SDL_RenderFillRect(renderer, &boxC);
    sprintf(buffer, "Road C: L1:%d L2:%d L3:%d",
        state->laneQueues[2][0].count,
        state->laneQueues[2][1].count,
        state->laneQueues[2][2].count);
    displayText(renderer, font, buffer, WINDOW_WIDTH - 260, WINDOW_HEIGHT / 2 - 15, white);

    SDL_Rect boxD = { 20, WINDOW_HEIGHT / 2 - 30, 250, 60 };
    SDL_RenderFillRect(renderer, &boxD);
    sprintf(buffer, "Road D: L1:%d L2:%d L3:%d",
        state->laneQueues[3][0].count,
        state->laneQueues[3][1].count,
        state->laneQueues[3][2].count);
    displayText(renderer, font, buffer, 30, WINDOW_HEIGHT / 2 - 15, white);

    if (state->isPriorityMode) {
        SDL_SetRenderDrawColor(renderer, 243, 156, 18, 220);
        SDL_Rect priorityBox = { WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 - 25, 300, 50 };
        SDL_RenderFillRect(renderer, &priorityBox);
        displayText(renderer, font, "PRIORITY MODE ACTIVE", WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2 - 10, black);
    }

    LeaveCriticalSection(&state->cs);
}

void drawStatistics(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    char buffer[100];
    SDL_Color white = { 255, 255, 255, 255 };

    EnterCriticalSection(&state->cs);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect statsBox = { WINDOW_WIDTH - 280, 20, 260, 80 };
    SDL_RenderFillRect(renderer, &statsBox);

    sprintf(buffer, "Processed: %d vehicles", state->totalVehiclesProcessed);
    displayText(renderer, font, buffer, WINDOW_WIDTH - 270, 30, white);

    sprintf(buffer, "Avg Wait: %.1fs", state->avgWaitTime);
    displayText(renderer, font, buffer, WINDOW_WIDTH - 270, 55, white);

    sprintf(buffer, "FPS: 60 | Mode: Smooth");
    displayText(renderer, font, buffer, WINDOW_WIDTH - 270, 75, white);

    LeaveCriticalSection(&state->cs);
}

void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y, SDL_Color color) {
    if (!font) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void refreshDisplay(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    drawBackground(renderer);
    drawRoads(renderer);
    drawTrafficLights(renderer, state);
    drawVehicles(renderer, state);
    drawQueueInfo(renderer, font, state);
    drawStatistics(renderer, font, state);

    SDL_RenderPresent(renderer);
}

DWORD WINAPI trafficController(LPVOID arg) {
    TrafficState* state = (TrafficState*)arg;
    int last_served_road = 3;

    while (1) {
        EnterCriticalSection(&state->cs);

        int A_lane2_count = state->laneQueues[0][1].count;

        if (A_lane2_count > PRIORITY_THRESHOLD || (state->isPriorityMode && A_lane2_count > PRIORITY_MIN)) {
            state->isPriorityMode = true;
            state->currentGreenRoad = 0;

            printf("\n>>> PRIORITY MODE (Road A: %d) <<<\n", A_lane2_count);

            while (state->laneQueues[0][1].count > PRIORITY_MIN) {
                VehicleNode* vehicle = dequeue(&state->laneQueues[0][1]);
                if (vehicle) {
                    ULONGLONG waitTime = GetTickCount64() - vehicle->arrivalTime;
                    state->totalWaitTime += (int)(waitTime / 1000);
                    state->totalVehiclesProcessed++;
                    state->avgWaitTime = (float)state->totalWaitTime / state->totalVehiclesProcessed;

                    printf("[PRIORITY] %s served (%.1fs)\n", vehicle->vehicleNumber, waitTime / 1000.0f);
                    free(vehicle);
                }
                LeaveCriticalSection(&state->cs);
                Sleep(VEHICLE_PROCESS_TIME);
                EnterCriticalSection(&state->cs);
            }

            state->isPriorityMode = false;
            printf(">>> Priority ended <<<\n\n");
            last_served_road = 0;
        }
        else {
            last_served_road = (last_served_road + 1) % 4;
            state->currentGreenRoad = last_served_road;

            int V_served = calculateOptimalVServed(state, last_served_road);
            int lane2Count = state->laneQueues[last_served_road][1].count;
            int toServe = (lane2Count < V_served) ? lane2Count : V_served;

            for (int k = 0; k < toServe; k++) {
                VehicleNode* vehicle = dequeue(&state->laneQueues[last_served_road][1]);
                if (vehicle) {
                    ULONGLONG waitTime = GetTickCount64() - vehicle->arrivalTime;
                    state->totalWaitTime += (int)(waitTime / 1000);
                    state->totalVehiclesProcessed++;
                    state->avgWaitTime = (float)state->totalWaitTime / state->totalVehiclesProcessed;

                    printf("[Road %c L2] %s (%.1fs)\n", ROAD_MAP[last_served_road], vehicle->vehicleNumber, waitTime / 1000.0f);
                    free(vehicle);
                }
                LeaveCriticalSection(&state->cs);
                Sleep(VEHICLE_PROCESS_TIME);
                EnterCriticalSection(&state->cs);
            }

            LeaveCriticalSection(&state->cs);
            Sleep(STRAIGHT_GREEN_TIME);
            EnterCriticalSection(&state->cs);
        }

        LeaveCriticalSection(&state->cs);
        Sleep(100);
    }

    return 0;
}

DWORD WINAPI leftTurnController(LPVOID arg) {
    TrafficState* state = (TrafficState*)arg;

    while (1) {
        Sleep(LEFT_TURN_WAIT_TIME);

        EnterCriticalSection(&state->cs);

        for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
            Queue* lane1 = &state->laneQueues[roadIdx][0];

            if (lane1->count > 0 && state->currentGreenRoad == roadIdx && !state->isPriorityMode) {
                VehicleNode* vehicle = dequeue(lane1);
                if (vehicle) {
                    ULONGLONG waitTime = GetTickCount64() - vehicle->arrivalTime;
                    state->totalWaitTime += (int)(waitTime / 1000);
                    state->totalVehiclesProcessed++;
                    state->avgWaitTime = (float)state->totalWaitTime / state->totalVehiclesProcessed;

                    printf("[Road %c L1] %s (%.1fs)\n", ROAD_MAP[roadIdx], vehicle->vehicleNumber, waitTime / 1000.0f);
                    free(vehicle);
                }
            }
        }

        LeaveCriticalSection(&state->cs);
    }

    return 0;
}

DWORD WINAPI freeFlowController(LPVOID arg) {
    TrafficState* state = (TrafficState*)arg;

    while (1) {
        EnterCriticalSection(&state->cs);

        for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
            Queue* lane3 = &state->laneQueues[roadIdx][2];

            while (lane3->count > FREE_FLOW_MAX_VEHICLES) {
                VehicleNode* vehicle = dequeue(lane3);
                if (vehicle) {
                    ULONGLONG waitTime = GetTickCount64() - vehicle->arrivalTime;
                    state->totalWaitTime += (int)(waitTime / 1000);
                    state->totalVehiclesProcessed++;
                    state->avgWaitTime = (float)state->totalWaitTime / state->totalVehiclesProcessed;

                    printf("[Road %c L3 FREE] %s (%.1fs)\n", ROAD_MAP[roadIdx], vehicle->vehicleNumber, waitTime / 1000.0f);
                    free(vehicle);
                }
            }
        }

        LeaveCriticalSection(&state->cs);
        Sleep(300);
    }

    return 0;
}

DWORD WINAPI readAndParseFile(LPVOID arg) {
    TrafficState* state = (TrafficState*)arg;

    int id;
    char name[NAME_MAX];
    int lane;
    char line[MAX_LINE_LENGTH];

    while (1) {
        for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
            FILE* file = fopen(LANE_FILES[roadIdx], "r");
            if (!file) continue;

            int newVehicles = 0;

            while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;

                if (sscanf(line, "%d %s %d", &id, name, &lane) == 3) {
                    if (lane >= 1 && lane <= 3) {
                        EnterCriticalSection(&state->cs);
                        enqueue(&state->laneQueues[roadIdx][lane - 1], name, ROAD_MAP[roadIdx], lane);
                        LeaveCriticalSection(&state->cs);
                        newVehicles++;
                    }
                }
            }

            fclose(file);

            if (newVehicles > 0) {
                clearFile(LANE_FILES[roadIdx]);
                printf("[READ] Road %c: +%d\n", ROAD_MAP[roadIdx], newVehicles);
            }
        }

        Sleep(1000);
    }

    return 0;
}