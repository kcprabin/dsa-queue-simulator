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
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 900
#define SCALE 1
#define ROAD_WIDTH 180
#define LANE_WIDTH 60
#define MAX_QUEUE_SIZE 100
#define PRIORITY_THRESHOLD 10
#define PRIORITY_MIN 5
#define MAIN_FONT "C:\\Windows\\Fonts\\Arial.ttf"
#define VEHICLE_WIDTH 35
#define VEHICLE_HEIGHT 20

#define FREE_FLOW_MAX_VEHICLES 8
#define LEFT_TURN_WAIT_TIME 2000
#define STRAIGHT_GREEN_TIME 4000
#define VEHICLE_PROCESS_TIME 500

const char* LANE_FILES[4] = {
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};
const char ROAD_MAP[4] = { 'A', 'B', 'C', 'D' };

const SDL_Color VEHICLE_COLORS[8] = {
    {231, 76, 60, 255},
    {52, 152, 219, 255},
    {46, 204, 113, 255},
    {241, 196, 15, 255},
    {155, 89, 182, 255},
    {230, 126, 34, 255},
    {26, 188, 156, 255},
    {243, 156, 18, 255}
};

typedef struct VehicleNode {
    char vehicleNumber[NAME_MAX];
    char road;
    int lane;
    int colorIndex;
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
static void clearFile(const char* filepath);
void drawRoadsAndLane(SDL_Renderer* renderer, TTF_Font* font);
void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y);
void drawTrafficLights(SDL_Renderer* renderer, TrafficState* state);
void drawQueueCounts(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
void drawVehiclesInQueue(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
void drawVehicleVisual(SDL_Renderer* renderer, int x, int y, SDL_Color color, bool isHorizontal);
void drawStatistics(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
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

    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 13);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
        font = NULL;
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer, font);
    SDL_RenderPresent(renderer);

    printf("=== Optimized 3-Lane Traffic Simulator ===\n");
    printf("Lane 1: Left Turn (Controlled)\n");
    printf("Lane 2: Straight (V-Served + Priority)\n");
    printf("Lane 3: Right Turn (Free Flow - Max 8)\n");
    printf("=========================================\n\n");

    HANDLE hController = CreateThread(NULL, 0, trafficController, &state, 0, NULL);
    HANDLE hLeftTurn = CreateThread(NULL, 0, leftTurnController, &state, 0, NULL);
    HANDLE hFreeFlow = CreateThread(NULL, 0, freeFlowController, &state, 0, NULL);
    HANDLE hReader = CreateThread(NULL, 0, readAndParseFile, &state, 0, NULL);

    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        refreshDisplay(renderer, font, &state);
        SDL_Delay(100);
    }

    if (hController != NULL) CloseHandle(hController);
    if (hLeftTurn != NULL) CloseHandle(hLeftTurn);
    if (hFreeFlow != NULL) CloseHandle(hFreeFlow);
    if (hReader != NULL) CloseHandle(hReader);

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

void enqueue(Queue* q, const char* vehicleNum, char road, int lane) {
    if (q->count >= MAX_QUEUE_SIZE) return;

    VehicleNode* newNode = (VehicleNode*)malloc(sizeof(VehicleNode));
    if (newNode == NULL) return;

    strncpy(newNode->vehicleNumber, vehicleNum, NAME_MAX - 1);
    newNode->vehicleNumber[NAME_MAX - 1] = '\0';
    newNode->road = road;
    newNode->lane = lane;
    newNode->arrivalTime = GetTickCount64();

    int hash = 0;
    for (int i = 0; vehicleNum[i] != '\0'; i++) {
        hash += vehicleNum[i];
    }
    newNode->colorIndex = hash % 8;
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
    if (q->front == NULL) return NULL;

    VehicleNode* temp = q->front;
    q->front = q->front->next;

    if (q->front == NULL) q->rear = NULL;

    q->count--;
    return temp;
}

void clearQueue(Queue* q) {
    while (q->front != NULL) {
        VehicleNode* temp = dequeue(q);
        free(temp);
    }
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
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Optimized 3-Lane Traffic Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN);
    if (!*window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetScale(*renderer, SCALE, SCALE);

    if (!*renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void drawArrow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = (float)x1, sx2 = (float)x1;

    for (int y = y1; y < y2; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1;
        sx2 += dx2;
    }

    sx1 = (float)x2;
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3;
        sx2 += dx2;
    }
}

void drawTrafficLight(SDL_Renderer* renderer, int x, int y, bool isGreen) {
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_Rect outerBox = { x - 2, y - 2, 54, 34 };
    SDL_RenderFillRect(renderer, &outerBox);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect lightBox = { x, y, 50, 30 };
    SDL_RenderFillRect(renderer, &lightBox);

    if (isGreen) SDL_SetRenderDrawColor(renderer, 46, 204, 113, 255);
    else SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);

    SDL_Rect light = { x + 5, y + 5, 20, 20 };
    SDL_RenderFillRect(renderer, &light);

    drawArrow(renderer, x + 35, y + 5, x + 35, y + 25, x + 45, y + 15);
}

void drawTrafficLights(SDL_Renderer* renderer, TrafficState* state) {
    EnterCriticalSection(&state->cs);
    int greenRoad = state->currentGreenRoad;
    LeaveCriticalSection(&state->cs);

    drawTrafficLight(renderer, 455, 330, greenRoad == 0);
    drawTrafficLight(renderer, 455, 550, greenRoad == 1);
    drawTrafficLight(renderer, 560, 440, greenRoad == 2);
    drawTrafficLight(renderer, 350, 440, greenRoad == 3);
}

void drawQueueCounts(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    char buffer[100];

    EnterCriticalSection(&state->cs);

    SDL_SetRenderDrawColor(renderer, 236, 240, 241, 255);
    SDL_Rect boxA = { 380, 90, 240, 50 };
    SDL_RenderFillRect(renderer, &boxA);
    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
    SDL_RenderDrawRect(renderer, &boxA);

    sprintf(buffer, "A: L1:%d L2:%d L3:%d",
        state->laneQueues[0][0].count,
        state->laneQueues[0][1].count,
        state->laneQueues[0][2].count);
    displayText(renderer, font, buffer, 390, 105);

    SDL_SetRenderDrawColor(renderer, 236, 240, 241, 255);
    SDL_Rect boxB = { 380, 810, 240, 50 };
    SDL_RenderFillRect(renderer, &boxB);
    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
    SDL_RenderDrawRect(renderer, &boxB);

    sprintf(buffer, "B: L1:%d L2:%d L3:%d",
        state->laneQueues[1][0].count,
        state->laneQueues[1][1].count,
        state->laneQueues[1][2].count);
    displayText(renderer, font, buffer, 390, 825);

    SDL_SetRenderDrawColor(renderer, 236, 240, 241, 255);
    SDL_Rect boxC = { 745, 420, 240, 50 };
    SDL_RenderFillRect(renderer, &boxC);
    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
    SDL_RenderDrawRect(renderer, &boxC);

    sprintf(buffer, "C: L1:%d L2:%d L3:%d",
        state->laneQueues[2][0].count,
        state->laneQueues[2][1].count,
        state->laneQueues[2][2].count);
    displayText(renderer, font, buffer, 755, 435);

    SDL_SetRenderDrawColor(renderer, 236, 240, 241, 255);
    SDL_Rect boxD = { 15, 420, 240, 50 };
    SDL_RenderFillRect(renderer, &boxD);
    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
    SDL_RenderDrawRect(renderer, &boxD);

    sprintf(buffer, "D: L1:%d L2:%d L3:%d",
        state->laneQueues[3][0].count,
        state->laneQueues[3][1].count,
        state->laneQueues[3][2].count);
    displayText(renderer, font, buffer, 25, 435);

    if (state->isPriorityMode) {
        SDL_SetRenderDrawColor(renderer, 243, 156, 18, 255);
        SDL_Rect priorityBox = { 360, 400, 280, 50 };
        SDL_RenderFillRect(renderer, &priorityBox);
        SDL_SetRenderDrawColor(renderer, 230, 126, 34, 255);
        SDL_RenderDrawRect(renderer, &priorityBox);

        displayText(renderer, font, "! PRIORITY MODE ACTIVE !", 390, 418);
    }

    LeaveCriticalSection(&state->cs);
}

void drawStatistics(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    char buffer[100];

    EnterCriticalSection(&state->cs);

    SDL_SetRenderDrawColor(renderer, 44, 62, 80, 255);
    SDL_Rect statsBox = { 10, 10, 280, 70 };
    SDL_RenderFillRect(renderer, &statsBox);
    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
    SDL_RenderDrawRect(renderer, &statsBox);

    SDL_Color white = { 255, 255, 255, 255 };

    sprintf(buffer, "Processed: %d vehicles", state->totalVehiclesProcessed);
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, buffer, white);
    if (textSurface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_Rect textRect = { 20, 20, textSurface->w, textSurface->h };
        SDL_RenderCopy(renderer, texture, NULL, &textRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(textSurface);
    }

    sprintf(buffer, "Avg Wait: %.1fs", state->avgWaitTime);
    textSurface = TTF_RenderText_Solid(font, buffer, white);
    if (textSurface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_Rect textRect = { 20, 45, textSurface->w, textSurface->h };
        SDL_RenderCopy(renderer, texture, NULL, &textRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(textSurface);
    }

    LeaveCriticalSection(&state->cs);
}

void drawVehicleVisual(SDL_Renderer* renderer, int x, int y, SDL_Color color, bool isHorizontal) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    if (isHorizontal) {
        SDL_Rect body = { x, y, VEHICLE_WIDTH, VEHICLE_HEIGHT };
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer,
            (Uint8)(color.r * 0.6),
            (Uint8)(color.g * 0.6),
            (Uint8)(color.b * 0.6), 255);
        SDL_Rect window1 = { x + 5, y + 3, 10, 6 };
        SDL_Rect window2 = { x + 20, y + 3, 10, 6 };
        SDL_RenderFillRect(renderer, &window1);
        SDL_RenderFillRect(renderer, &window2);

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_Rect wheel1 = { x + 5, y + 16, 6, 3 };
        SDL_Rect wheel2 = { x + 24, y + 16, 6, 3 };
        SDL_RenderFillRect(renderer, &wheel1);
        SDL_RenderFillRect(renderer, &wheel2);
    }
    else {
        SDL_Rect body = { x, y, VEHICLE_HEIGHT, VEHICLE_WIDTH };
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer,
            (Uint8)(color.r * 0.6),
            (Uint8)(color.g * 0.6),
            (Uint8)(color.b * 0.6), 255);
        SDL_Rect window1 = { x + 3, y + 5, 6, 10 };
        SDL_Rect window2 = { x + 3, y + 20, 6, 10 };
        SDL_RenderFillRect(renderer, &window1);
        SDL_RenderFillRect(renderer, &window2);

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_Rect wheel1 = { x + 16, y + 5, 3, 6 };
        SDL_Rect wheel2 = { x + 16, y + 24, 3, 6 };
        SDL_RenderFillRect(renderer, &wheel1);
        SDL_RenderFillRect(renderer, &wheel2);
    }
}

void drawVehiclesInQueue(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    EnterCriticalSection(&state->cs);

    for (int roadIdx = 0; roadIdx < 4; roadIdx++) {
        for (int laneIdx = 0; laneIdx < 3; laneIdx++) {
            VehicleNode* current = state->laneQueues[roadIdx][laneIdx].front;
            int displayCount = 0;

            while (current != NULL && displayCount < 6) {
                SDL_Color vehicleColor = VEHICLE_COLORS[current->colorIndex];

                int x = 0, y = 0;
                bool isHorizontal = false;
                int laneOffset = (laneIdx - 1) * 22;

                switch (roadIdx) {
                case 0:
                    x = 478 + laneOffset;
                    y = 150 + displayCount * 48;
                    isHorizontal = false;
                    break;
                case 1:
                    x = 478 + laneOffset;
                    y = 720 - displayCount * 48;
                    isHorizontal = false;
                    break;
                case 2:
                    x = 720 - displayCount * 48;
                    y = 458 + laneOffset;
                    isHorizontal = true;
                    break;
                case 3:
                    x = 160 + displayCount * 48;
                    y = 458 + laneOffset;
                    isHorizontal = true;
                    break;
                }

                drawVehicleVisual(renderer, x, y, vehicleColor, isHorizontal);

                char displayStr[10];
                snprintf(displayStr, sizeof(displayStr), "%.5s", current->vehicleNumber);

                SDL_Color textColor = { 44, 62, 80, 255 };
                SDL_Surface* textSurface = TTF_RenderText_Solid(font, displayStr, textColor);
                if (textSurface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    int textX = isHorizontal ? x + 5 : x - 15;
                    int textY = isHorizontal ? y + 22 : y + 37;
                    SDL_Rect textRect = { textX, textY, textSurface->w, textSurface->h };
                    SDL_RenderCopy(renderer, texture, NULL, &textRect);
                    SDL_DestroyTexture(texture);
                    SDL_FreeSurface(textSurface);
                }

                current = current->next;
                displayCount++;
            }
        }
    }

    LeaveCriticalSection(&state->cs);
}

void drawRoadsAndLane(SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
    SDL_Rect verticalRoad = { WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 80, ROAD_WIDTH, WINDOW_HEIGHT - 80 };
    SDL_RenderFillRect(renderer, &verticalRoad);

    SDL_Rect horizontalRoad = { 0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &horizontalRoad);

    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_Rect junction = { WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, ROAD_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &junction);

    SDL_SetRenderDrawColor(renderer, 240, 230, 140, 255);
    for (int i = 80; i < WINDOW_HEIGHT; i += 30) {
        if (i < WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 || i > WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2) {
            SDL_Rect dash = { WINDOW_WIDTH / 2 - 2, i, 4, 15 };
            SDL_RenderFillRect(renderer, &dash);
        }
    }

    for (int i = 0; i < WINDOW_WIDTH; i += 30) {
        if (i < WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 || i > WINDOW_WIDTH / 2 + ROAD_WIDTH / 2) {
            SDL_Rect dash = { i, WINDOW_HEIGHT / 2 - 2, 15, 4 };
            SDL_RenderFillRect(renderer, &dash);
        }
    }

    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    for (int i = 0; i <= 3; i++) {
        int yPos = WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i;
        SDL_RenderDrawLine(renderer, 0, yPos, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, yPos);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH, yPos, WINDOW_WIDTH / 2 + ROAD_WIDTH / 2, yPos);

        int xPos = WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i;
        SDL_RenderDrawLine(renderer, xPos, 80, xPos, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2);
        SDL_RenderDrawLine(renderer, xPos, WINDOW_HEIGHT, xPos, WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2);
    }

    if (font) {
        SDL_Color roadLabelColor = { 255, 255, 255, 255 };
        SDL_Surface* textSurface;
        SDL_Texture* texture;

        textSurface = TTF_RenderText_Solid(font, "Road A", roadLabelColor);
        if (textSurface) {
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = { 478, 85, textSurface->w, textSurface->h };
            SDL_RenderCopy(renderer, texture, NULL, &textRect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(textSurface);
        }

        textSurface = TTF_RenderText_Solid(font, "Road B", roadLabelColor);
        if (textSurface) {
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = { 478, 865, textSurface->w, textSurface->h };
            SDL_RenderCopy(renderer, texture, NULL, &textRect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(textSurface);
        }

        textSurface = TTF_RenderText_Solid(font, "Road D", roadLabelColor);
        if (textSurface) {
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = { 25, 458, textSurface->w, textSurface->h };
            SDL_RenderCopy(renderer, texture, NULL, &textRect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(textSurface);
        }

        textSurface = TTF_RenderText_Solid(font, "Road C", roadLabelColor);
        if (textSurface) {
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = { 930, 458, textSurface->w, textSurface->h };
            SDL_RenderCopy(renderer, texture, NULL, &textRect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(textSurface);
        }
    }
}

void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y) {
    if (!font) return;

    SDL_Color textColor = { 44, 62, 80, 255 };
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (!textSurface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);

    if (!texture) return;

    SDL_Rect textRect = { x, y, 0, 0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    SDL_DestroyTexture(texture);
}

void refreshDisplay(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    SDL_RenderClear(renderer);

    drawRoadsAndLane(renderer, font);
    drawTrafficLights(renderer, state);
    drawQueueCounts(renderer, font, state);
    drawVehiclesInQueue(renderer, font, state);
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

            printf("\n>>> PRIORITY MODE (Road A Lane 2: %d vehicles) <<<\n", A_lane2_count);

            while (state->laneQueues[0][1].count > PRIORITY_MIN) {
                VehicleNode* vehicle = dequeue(&state->laneQueues[0][1]);
                if (vehicle) {
                    ULONGLONG waitTime = GetTickCount64() - vehicle->arrivalTime;
                    state->totalWaitTime += (int)(waitTime / 1000);
                    state->totalVehiclesProcessed++;
                    state->avgWaitTime = (float)state->totalWaitTime / state->totalVehiclesProcessed;

                    printf("[PRIORITY] %s served (waited %.1fs)\n",
                        vehicle->vehicleNumber, waitTime / 1000.0f);
                    free(vehicle);
                }
                LeaveCriticalSection(&state->cs);
                Sleep(VEHICLE_PROCESS_TIME);
                EnterCriticalSection(&state->cs);
            }

            state->isPriorityMode = false;
            printf(">>> Priority mode ended <<<\n\n");
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

                    printf("[Road %c L2] %s served (waited %.1fs)\n",
                        ROAD_MAP[last_served_road], vehicle->vehicleNumber, waitTime / 1000.0f);
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

                    printf("[Road %c L1 LEFT] %s served (waited %.1fs)\n",
                        ROAD_MAP[roadIdx], vehicle->vehicleNumber, waitTime / 1000.0f);
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

                    printf("[Road %c L3 FREE] %s passed (waited %.1fs)\n",
                        ROAD_MAP[roadIdx], vehicle->vehicleNumber, waitTime / 1000.0f);
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
            const char* filepath = LANE_FILES[roadIdx];
            FILE* file = fopen(filepath, "r");

            if (!file) continue;

            int newVehicles = 0;

            while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;

                if (sscanf(line, "%d %s %d", &id, name, &lane) == 3) {
                    if (lane >= 1 && lane <= 3) {
                        char roadChar = ROAD_MAP[roadIdx];

                        EnterCriticalSection(&state->cs);
                        enqueue(&state->laneQueues[roadIdx][lane - 1], name, roadChar, lane);
                        LeaveCriticalSection(&state->cs);

                        newVehicles++;
                    }
                }
            }

            fclose(file);

            if (newVehicles > 0) {
                clearFile(filepath);
                printf("[FILE READ] Road %c: +%d vehicles\n", ROAD_MAP[roadIdx], newVehicles);
            }
        }

        Sleep(1000);
    }

    return 0;
}