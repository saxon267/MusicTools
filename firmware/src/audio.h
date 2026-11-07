#pragma once

#include "theory.h"

void audioInit();
void audioPlayChord(mt::Note root, int typeIdx);
bool audioBusy();
