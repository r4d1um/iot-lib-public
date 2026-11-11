// StampDisplay: small driver + graphics library for the 1.77" 160x128
// ST7735S TFT module used by the stamp machine (and the parking-lot display).
#pragma once

#include "Panel.h"
#include "Gfx.h"
#include "BandPanel.h"
#include "MonoPanel.h"
#include "St7735sPanel.h"

#ifdef ARDUINO
#include "St7735s.h"
#else
#include "FramebufferPanel.h"
#endif
