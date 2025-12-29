#include "config.h"
#include "types.h"
#include "globals.h"
#include "queue.h"
#include "geometry.h"
#include "physics.h"
#include "transition.h"
#include "renderer.h"
#include "fileio.h"

// Define globals here
RoadData roads[4];
TransitionVehicle transitions[MAX_QUEUE];
int transitionCount = 0;
int currentGreen = 0;
int lightState = GREEN_LIGHT;

const char* basedir = "C:\\TrafficShared\\";
const char* files[4] = {
    "C:\\TrafficShared\\lanea.txt",
    "C:\\TrafficShared\\laneb.txt",
    "C:\\TrafficShared\\lanec.txt",
    "C:\\TrafficShared\\laned.txt"
};

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() != 0) {
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Traffic Simulator - Modular",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 16);
    srand((unsigned)time(NULL));

    for (int i = 0; i < 4; i++) {
        queueInitialize(&roads[i].L1);
        queueInitialize(&roads[i].L2);
        queueInitialize(&roads[i].L3);
    }

    loadVehiclesFromInputFiles();

    int running = 1;
    SDL_Event e;
    Uint32 lastFileCheck = 0;
    Uint32 lastLightChange = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        Uint32 now = SDL_GetTicks();

        if (now - lastFileCheck >= 200) {
            loadVehiclesFromInputFiles();
            lastFileCheck = now;
        }

        if (now - lastLightChange >= 5000) {
            currentGreen = (currentGreen + 1) % 4;
            lastLightChange = now;
        }

        for (int r = 0; r < 4; r++) {
            updateLaneVehiclesToIntersection(&roads[r].L1, r);
            updateLaneVehiclesToIntersection(&roads[r].L2, r);
            updateRightTurnLane(&roads[r].L3, r);
        }

        removeStuckTransitionVehicles();
        processIntersectionTransitions();

        // handle green light transitions for L1 and L2
        if (currentGreen >= 0 && currentGreen < 4 && lightState == GREEN_LIGHT) {
            // left-turn lane
            Lane* L = &roads[currentGreen].L1;
            int cnt = L->count;
            for (int i = 0; i < cnt; i++) {
                Vehicle* v = queueGetVehicleAt(L, i);
                if (!v) continue;

                float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
                int reached = 0;

                if (currentGreen == 0 && v->y >= cy - ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 1 && v->x <= cx + ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 2 && v->y <= cy + ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 3 && v->x >= cx - ROAD_W / 2.0f) reached = 1;

                if (reached) {
                    Vehicle temp;
                    queueRemove(L, &temp);
                    int targetRoad = (currentGreen + 1) % 4;
                    insertVehicleIntoTransition(temp, targetRoad);
                    i--; cnt--;
                }
            }

            // straight lane
            L = &roads[currentGreen].L2;
            cnt = L->count;
            for (int i = 0; i < cnt; i++) {
                Vehicle* v = queueGetVehicleAt(L, i);
                if (!v) continue;

                float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;
                int reached = 0;

                if (currentGreen == 0 && v->y >= cy - ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 1 && v->x <= cx + ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 2 && v->y <= cy + ROAD_W / 2.0f) reached = 1;
                if (currentGreen == 3 && v->x >= cx - ROAD_W / 2.0f) reached = 1;

                if (reached) {
                    Vehicle temp;
                    queueRemove(L, &temp);
                    int targetRoad;
                    int opposite = (currentGreen == 0) ? 2 : (currentGreen == 1) ? 3 : (currentGreen == 2) ? 0 : 1;

                    if (rand() % 2 == 0) {
                        targetRoad = opposite;
                    }
                    else {
                        targetRoad = (currentGreen + 3) % 4;
                    }

                    insertVehicleIntoTransition(temp, targetRoad);
                    i--; cnt--;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        renderGradientBackground(renderer);
        renderDecorativeTrees(renderer);
        renderRoadNetwork(renderer);

        for (int r = 0; r < 4; r++) {
            renderLaneVehicles(renderer, &roads[r].L1, 220, 80, 80);
            renderLaneVehicles(renderer, &roads[r].L2, 80, 220, 80);
            renderLaneVehicles(renderer, &roads[r].L3, 80, 120, 220);
        }

        renderTransitionVehicles(renderer);
        renderTrafficSignals(renderer);

        SDL_RenderPresent(renderer);
        sleep_ms(16);
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
