#include "renderer.h"
#include "config.h"
#include "globals.h"
#include "queue.h"

void renderGradientBackground(SDL_Renderer* renderer) {
    for (int y = 0; y < SCREEN_H; y++) {
        int r = 40 + (y * 30) / SCREEN_H;
        int g = 160 + (y * 20) / SCREEN_H;
        int b = 50 + (y * 20) / SCREEN_H;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_W, y);
    }
}

void renderDecorativeTrees(SDL_Renderer* renderer) {
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

        SDL_SetRenderDrawColor(renderer, 101, 67, 33, 255);
        SDL_Rect trunk = { x - 4, y + 10, 8, 20 };
        SDL_RenderFillRect(renderer, &trunk);

        SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
        for (int dy = -15; dy <= 10; dy++) {
            for (int dx = -15; dx <= 15; dx++) {
                if (dx * dx + dy * dy <= 225) {
                    SDL_RenderDrawPoint(renderer, x + dx, y + dy);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 50, 205, 50, 255);
        for (int dy = -10; dy <= 0; dy++) {
            for (int dx = -8; dx <= 8; dx++) {
                if (dx * dx + dy * dy <= 64) {
                    SDL_RenderDrawPoint(renderer, x + dx - 2, y + dy - 2);
                }
            }
        }

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

void renderRoadNetwork(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 45, 45, 48, 255);
    SDL_Rect vert = { (int)(SCREEN_W / 2.0f - ROAD_W / 2.0f), 0, ROAD_W, SCREEN_H };
    SDL_Rect horiz = { 0, (int)(SCREEN_H / 2.0f - ROAD_W / 2.0f), SCREEN_W, ROAD_W };
    SDL_RenderFillRect(renderer, &vert);
    SDL_RenderFillRect(renderer, &horiz);

    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
    for (int i = 1; i < 3; i++) {
        int x = (int)(SCREEN_W / 2.0f - ROAD_W / 2.0f + i * LANE_W);
        for (int y = 0; y < SCREEN_H; y += 30) {
            SDL_RenderDrawLine(renderer, x, y, x, y + 15);
        }
        int yPos = (int)(SCREEN_H / 2.0f - ROAD_W / 2.0f + i * LANE_W);
        for (int x2 = 0; x2 < SCREEN_W; x2 += 30) {
            SDL_RenderDrawLine(renderer, x2, yPos, x2 + 15, yPos);
        }
    }

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

void renderSingleVehicle(SDL_Renderer* renderer, Vehicle* v, int r, int g, int b) {
    if (!v) return;

    int width = 18;
    int height = 12;

    int isVertical = (v->fromRoad == 0 || v->fromRoad == 2);
    if (isVertical) {
        int temp = width;
        width = height;
        height = temp;
    }

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

void renderLaneVehicles(SDL_Renderer* renderer, Lane* L, int r, int g, int b) {
    for (int i = 0; i < L->count; i++) {
        Vehicle* v = queueGetVehicleAt(L, i);
        if (v) {
            renderSingleVehicle(renderer, v, r, g, b);
        }
    }
}

void renderTransitionVehicles(SDL_Renderer* renderer) {
    for (int i = 0; i < transitionCount; i++) {
        Vehicle* v = &transitions[i].v;
        if (v->isStopped) {
            renderSingleVehicle(renderer, v, 128, 90, 0);
        }
        else {
            renderSingleVehicle(renderer, v, 255, 180, 0);
        }
    }
}

void renderTrafficSignals(SDL_Renderer* renderer) {
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
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_Rect pole = { lightPositions[i][0] + 15, lightPositions[i][1] + 30, 3, 30 };
        SDL_RenderFillRect(renderer, &pole);

        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect housing = { lightPositions[i][0], lightPositions[i][1], 35, 30 };
        SDL_RenderFillRect(renderer, &housing);

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
