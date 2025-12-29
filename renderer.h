#ifndef RENDERER_H
#define RENDERER_H

#include "types.h"

void renderGradientBackground(SDL_Renderer* renderer);
void renderDecorativeTrees(SDL_Renderer* renderer);
void renderRoadNetwork(SDL_Renderer* renderer);
void renderSingleVehicle(SDL_Renderer* renderer, Vehicle* v, int r, int g, int b);
void renderLaneVehicles(SDL_Renderer* renderer, Lane* L, int r, int g, int b);
void renderTransitionVehicles(SDL_Renderer* renderer);
void renderTrafficSignals(SDL_Renderer* renderer);

#endif // RENDERER_H
