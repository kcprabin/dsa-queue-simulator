#define _CRT_SECURE_NO_WARNINGS
#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- CONSTANTS ---
#define MAX_LINE_LENGTH 50
#define NAME_MAX 16
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15
#define MAX_QUEUE_SIZE 100
#define PRIORITY_THRESHOLD 10
#define PRIORITY_MIN 5
#define MAIN_FONT "C:\\Windows\\Fonts\\Arial.ttf"
#define VEHICLE_WIDTH 35
#define VEHICLE_HEIGHT 20
                                                                                                                            
// --- FILE COMMUNICATION CONFIGURATION (Paths must match traffic.c) ---
const char* LANE_FILES[4] = {
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};
const char ROAD_MAP[4] = { 'A', 'B', 'C', 'D' };

// Vehicle colors (RGB)
const SDL_Color VEHICLE_COLORS[8] = {
    {255, 87, 51, 255},   // Red-Orange
    {51, 153, 255, 255},  // Blue
    {46, 204, 113, 255},  // Green
    {241, 196, 15, 255},  // Yellow
    {155, 89, 182, 255},  // Purple
    {230, 126, 34, 255},  // Orange
    {52, 152, 219, 255},  // Light Blue
    {231, 76, 60, 255}    // Red
};

// --- DATA STRUCTURES ---
typedef struct VehicleNode {
    char vehicleNumber[NAME_MAX];
    char road;
    int lane;
    int colorIndex; // For visual variety
    struct VehicleNode* next;
} VehicleNode;

typedef struct {
    VehicleNode* front;
    VehicleNode* rear;
    int count;
} Queue;

typedef struct {
    Queue roadQueues[4];
    int currentGreenRoad;
    int nextGreenRoad;
    bool isPriorityMode;
    CRITICAL_SECTION cs;
} TrafficState;

// --- FUNCTION DECLARATIONS ---
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
void refreshDisplay(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state);
DWORD WINAPI trafficController(LPVOID arg);
DWORD WINAPI readAndParseFile(LPVOID arg);

// --- MAIN FUNCTION ---
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
    InitializeCriticalSection(&state.cs);

    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 14);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
        font = NULL;
    }

    // Initial draw setup
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer, font);
    SDL_RenderPresent(renderer);

    printf("=== Traffic Simulator Started ===\n");
    printf("Monitoring: C:\\TrafficShared\\*.txt\n");
    printf("Traffic Light Duration: 4 seconds\n");
    printf("=================================\n\n");

    // Create threads
    HANDLE hController = CreateThread(NULL, 0, trafficController, &state, 0, NULL);
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
    if (hReader != NULL) CloseHandle(hReader);

    DeleteCriticalSection(&state.cs);

    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

// --- QUEUE & HELPER FUNCTIONS ---
void initQueues(TrafficState* state) {
    for (int i = 0; i < 4; i++) {
        state->roadQueues[i].front = NULL;
        state->roadQueues[i].rear = NULL;
        state->roadQueues[i].count = 0;
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
    
    // Assign color based on vehicle number hash
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
    if (f) {
        fclose(f);
    }
}

// --- SDL & DRAWING FUNCTIONS ---

bool initializeSDL(SDL_Window** window, SDL_Renderer** renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Traffic Junction Simulator",
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
    int temp = *a; *a = *b; *b = temp;
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
        sx1 += dx1; sx2 += dx2;
    }

    sx1 = (float)x2;
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3; sx2 += dx2;
    }
}

void drawTrafficLight(SDL_Renderer* renderer, int x, int y, bool isGreen) {
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_Rect lightBox = { x, y, 50, 30 };
    SDL_RenderFillRect(renderer, &lightBox);

    if (isGreen) SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    else SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    SDL_Rect light = { x + 5, y + 5, 20, 20 };
    SDL_RenderFillRect(renderer, &light);

    drawArrow(renderer, x + 35, y + 5, x + 35, y + 25, x + 45, y + 15);
}

void drawTrafficLights(SDL_Renderer* renderer, TrafficState* state) {
    EnterCriticalSection(&state->cs);
    int greenRoad = state->currentGreenRoad;
    LeaveCriticalSection(&state->cs);

    drawTrafficLight(renderer, 390, 290, greenRoad == 0);
    drawTrafficLight(renderer, 390, 490, greenRoad == 1);
    drawTrafficLight(renderer, 490, 390, greenRoad == 2);
    drawTrafficLight(renderer, 270, 390, greenRoad == 3);
}

void drawQueueCounts(SDL_Renderer* renderer, TTF_Font* font, TrafficState* state) {
    if (!font) return;

    char buffer[50];

    EnterCriticalSection(&state->cs);

    sprintf(buffer, "Queue: %d", state->roadQueues[0].count);
    displayText(renderer, font, buffer, 360, 45);

    sprintf(buffer, "Queue: %d", state->roadQueues[1].count);
    displayText(renderer, font, buffer, 360, 735);

    sprintf(buffer, "Queue: %d", state->roadQueues[2].count);
    displayText(renderer, font, buffer, 690, 375);

    sprintf(buffer, "Queue: %d", state->roadQueues[3].count);
    displayText(renderer, font, buffer, 20, 375);

    if (state->isPriorityMode) {
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
        SDL_Rect priorityBox = { 300, 350, 200, 30 };
        SDL_RenderFillRect(renderer, &priorityBox);
        displayText(renderer, font, "PRIORITY MODE", 320, 355);
    }

    LeaveCriticalSection(&state->cs);
}

// Draw a visual car representation
void drawVehicleVisual(SDL_Renderer* renderer, int x, int y, SDL_Color color, bool isHorizontal) {
    // Car body
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    if (isHorizontal) {
        SDL_Rect body = { x, y, VEHICLE_WIDTH, VEHICLE_HEIGHT };
        SDL_RenderFillRect(renderer, &body);
        
        // Car outline
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &body);
        
        // Windows (lighter color)
        SDL_SetRenderDrawColor(renderer, 
            (Uint8)(color.r * 0.7), 
            (Uint8)(color.g * 0.7), 
            (Uint8)(color.b * 0.7), 255);
        SDL_Rect window1 = { x + 5, y + 3, 10, 6 };
        SDL_Rect window2 = { x + 20, y + 3, 10, 6 };
        SDL_RenderFillRect(renderer, &window1);
        SDL_RenderFillRect(renderer, &window2);
        
        // Wheels
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_Rect wheel1 = { x + 5, y + 16, 6, 3 };
        SDL_Rect wheel2 = { x + 24, y + 16, 6, 3 };
        SDL_RenderFillRect(renderer, &wheel1);
        SDL_RenderFillRect(renderer, &wheel2);
    } else {
        // Vertical orientation
        SDL_Rect body = { x, y, VEHICLE_HEIGHT, VEHICLE_WIDTH };
        SDL_RenderFillRect(renderer, &body);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &body);
        
        SDL_SetRenderDrawColor(renderer, 
            (Uint8)(color.r * 0.7), 
            (Uint8)(color.g * 0.7), 
            (Uint8)(color.b * 0.7), 255);
        SDL_Rect window1 = { x + 3, y + 5, 6, 10 };
        SDL_Rect window2 = { x + 3, y + 20, 6, 10 };
        SDL_RenderFillRect(renderer, &window1);
        SDL_RenderFillRect(renderer, &window2);
        
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
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
        VehicleNode* current = state->roadQueues[roadIdx].front;
        int displayCount = 0;

        while (current != NULL && displayCount < 5) {
            SDL_Color vehicleColor = VEHICLE_COLORS[current->colorIndex];
            
            int x = 0, y = 0;
            bool isHorizontal = false;
            
            switch (roadIdx) {
            case 0: // Road A (vertical - coming from top)
                x = 365;
                y = 70 + displayCount * 45;
                isHorizontal = false;
                break;
            case 1: // Road B (vertical - coming from bottom)
                x = 365;
                y = 650 - displayCount * 45;
                isHorizontal = false;
                break;
            case 2: // Road C (horizontal - coming from right)
                x = 630 - displayCount * 45;
                y = 395;
                isHorizontal = true;
                break;
            case 3: // Road D (horizontal - coming from left)
                x = 140 + displayCount * 45;
                y = 395;
                isHorizontal = true;
                break;
            }

            // Draw visual car
            drawVehicleVisual(renderer, x, y, vehicleColor, isHorizontal);
            
            // Draw vehicle ID below/beside the car
            char displayStr[10];
            snprintf(displayStr, sizeof(displayStr), "%.5s", current->vehicleNumber);
            
            int textX = isHorizontal ? x + 5 : x - 10;
            int textY = isHorizontal ? y + 22 : y + 38;
            displayText(renderer, font, displayStr, textX, textY);

            current = current->next;
            displayCount++;
        }
    }

    LeaveCriticalSection(&state->cs);
}

void drawRoadsAndLane(SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 211, 211, 211, 255);
    SDL_Rect verticalRoad = { WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT };
    SDL_RenderFillRect(renderer, &verticalRoad);

    SDL_Rect horizontalRoad = { 0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH };
    SDL_RenderFillRect(renderer, &horizontalRoad);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i <= 3; i++) {
        SDL_RenderDrawLine(renderer, 0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
            WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
            WINDOW_WIDTH / 2 + ROAD_WIDTH / 2, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);

        SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, 0,
            WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2);
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT,
            WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2);
    }

    // Draw dashed center lines
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < WINDOW_HEIGHT; i += 20) {
        SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2, i, WINDOW_WIDTH / 2, i + 10);
    }
    for (int i = 0; i < WINDOW_WIDTH; i += 20) {
        SDL_RenderDrawLine(renderer, i, WINDOW_HEIGHT / 2, i + 10, WINDOW_HEIGHT / 2);
    }

    if (font) {
        displayText(renderer, font, "Road A", 365, 10);
        displayText(renderer, font, "Road B", 365, 770);
        displayText(renderer, font, "Road D", 10, 390);
        displayText(renderer, font, "Road C", 720, 390);
    }
}

void displayText(SDL_Renderer* renderer, TTF_Font* font, char* text, int x, int y) {
    if (!font) return;

    SDL_Color textColor = { 0, 0, 0, 255 };
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
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    drawRoadsAndLane(renderer, font);
    drawTrafficLights(renderer, state);
    drawQueueCounts(renderer, font, state);
    drawVehiclesInQueue(renderer, font, state);

    SDL_RenderPresent(renderer);
}

// --- THREAD 1: TRAFFIC CONTROLLER (V-Served & Priority Logic) ---
DWORD WINAPI trafficController(LPVOID arg) {
    TrafficState* state = (TrafficState*)arg;
    int last_served_road = 3;

    while (1) {
        EnterCriticalSection(&state->cs);

        int A_count = state->roadQueues[0].count;

        // 1. PRIORITY CHECK (Road A has high volume)
        if (A_count > PRIORITY_THRESHOLD || (state->isPriorityMode && A_count > PRIORITY_MIN)) {
            state->isPriorityMode = true;
            state->currentGreenRoad = 0;

            printf("\n>>> PRIORITY MODE ACTIVATED (Road A: %d vehicles) <<<\n", A_count);

            while (state->roadQueues[0].count > PRIORITY_MIN) {
                VehicleNode* vehicle = dequeue(&state->roadQueues[0]);
                if (vehicle) {
                    printf("[PRIORITY] Served: %s from Road A (Remaining: %d)\n", 
                           vehicle->vehicleNumber, state->roadQueues[0].count);
                    free(vehicle);
                }
                LeaveCriticalSection(&state->cs);
                Sleep(500);
                EnterCriticalSection(&state->cs);
            }

            state->isPriorityMode = false;
            printf(">>> Priority mode ended (A count: %d) <<<\n", state->roadQueues[0].count);

            last_served_road = 0;
        }

        // 2. NORMAL CONDITION SERVING (V-served Round-Robin)
        else {
            last_served_road = (last_served_road + 1) % 4;
            state->currentGreenRoad = last_served_road;

            int current_road_idx = state->currentGreenRoad;
            Queue* current_q = &state->roadQueues[current_road_idx];

            int total_waiting = 0;
            for (int i = 0; i < 4; i++) {
                total_waiting += state->roadQueues[i].count;
            }

            int V_served = (4 > 0) ? (total_waiting / 4) : 0;
            int vehicles_to_dequeue = (current_q->count > 0) ? (V_served > 0 ? V_served : 1) : 0;

            for (int k = 0; k < vehicles_to_dequeue; k++) {
                VehicleNode* vehicle = dequeue(current_q);
                if (vehicle) {
                    printf("[Road %c GREEN] %s served [Queue: %d]\n", 
                           ROAD_MAP[current_road_idx], vehicle->vehicleNumber, current_q->count);
                    free(vehicle);
                }
                else {
                    break;
                }
                LeaveCriticalSection(&state->cs);
                Sleep(500);
                EnterCriticalSection(&state->cs);
            }

            LeaveCriticalSection(&state->cs);
            Sleep(4000); // 4 SECONDS green light duration
            EnterCriticalSection(&state->cs);
        }

        LeaveCriticalSection(&state->cs);
        Sleep(100);
    }

    return 0;
}

// --- THREAD 2: FILE READER ---
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
                    if (lane == 2) {
                        char roadChar = ROAD_MAP[roadIdx];

                        EnterCriticalSection(&state->cs);
                        enqueue(&state->roadQueues[roadIdx], name, roadChar, lane);
                        LeaveCriticalSection(&state->cs);

                        newVehicles++;
                    }
                }
            }

            fclose(file);

            if (newVehicles > 0) {
                clearFile(filepath);
                printf("[FILE READ] Road %c: Added %d new vehicles\n", 
                       ROAD_MAP[roadIdx], newVehicles);
            }
        }

        Sleep(1000);
    }

    return 0;
}