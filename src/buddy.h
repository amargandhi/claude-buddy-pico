#pragma once

#include <cstdint>

namespace pimoroni {
class PicoGraphics;
}

// Multi-species ASCII buddy renderer. Each species lives in its own
// src/buddies/<name>.cpp file and exposes 7 state functions matching
// the PersonaState order: sleep, idle, busy, attention, celebrate,
// dizzy, heart.
void buddyInit();
void buddyInvalidate();
void buddyTick(uint8_t persona_state);
void buddySetPeek(bool peek);
void buddySetTarget(pimoroni::PicoGraphics* graphics, int origin_x, int origin_y);

void buddySetSpecies(const char* name);
void buddySetSpeciesIdx(uint8_t idx);
void buddyNextSpecies();
uint8_t buddySpeciesIdx();
uint8_t buddySpeciesCount();
const char* buddySpeciesName();
uint16_t buddySpeciesColor();

// Per-species state function: takes the global tickCount and renders
// the buddy + any overlays for the current state into the shared canvas.
typedef void (*StateFn)(uint32_t t);

struct Species {
  const char* name;
  uint16_t bodyColor;
  StateFn states[7];
};

struct PackState {
  char frames[8][24];
  uint8_t nFrames;
  uint16_t delayMs;
};

struct PackDefinition {
  bool valid;
  char name[24];
  uint16_t bodyColor;
  uint16_t bgColor;
  uint16_t textColor;
  uint16_t textDimColor;
  uint16_t inkColor;
  PackState states[7];
};

void buddySetCustomSpecies(const PackDefinition* pack);
void buddyClearCustomSpecies();
bool buddyHasCustomSpecies();
bool buddyCustomSpeciesSelected();
