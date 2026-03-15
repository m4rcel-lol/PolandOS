// PolandOS — Sterownik brzeczyka PC
#pragma once
#include "../../include/types.h"

void speaker_init(void);
void speaker_tone(u32 frequency);
void speaker_off(void);
void speaker_beep(u32 freq, u32 ms);
void play_mazurek_dabrowskiego(void);
