#pragma once

#include "theory.h"

void audioInit();
void audioPlayChord(mt::Note root, int typeIdx);
void audioPlayScale(mt::Note root, int typeIdx);
void audioStop();
bool audioBusy();

