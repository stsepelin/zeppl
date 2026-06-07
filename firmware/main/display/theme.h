#pragma once

// V-Rod cluster color palette. Roles, not hues — so retheming is one file.

// Brand accents
#define VROD_ORANGE          0xFF6600   // primary accent: cursor, fuel fill, neutral gear
#define VROD_RED             0xFF1100   // tach redline (band segments, "10" label)
#define VROD_RED_BRIGHT      0xFF2200   // warning fill (low fuel, hot engine)
#define VROD_RED_TICK_DIM    0xCC2211   // dimmed redline label ("10" when needle is far)

// Neutrals
#define VROD_TEXT            0xFFFFFF   // primary readout color
#define VROD_TEXT_DIM        0x888888   // captions, units, low-importance text
#define VROD_ICON            0xCCCCCC   // monochrome icon glyphs (fuel pump, thermo)
#define VROD_RAIL            0x555555   // empty fuel-scale ticks
#define VROD_TICK_MINOR                                                                            \
    0x555555  // tach minor (quarter) ticks — clearly gray vs the white integer/half ticks
#define VROD_ARROW_OFF       0x222222   // turn-signal arrow when off

// Status / warning lamps
#define VROD_GREEN_SIGNAL    0x33CC22   // turn-signal arrow when on
#define VROD_GREEN_LOW_BEAM  0x33CC22   // low-beam lamp lit
#define VROD_BLUE_HIGH_BEAM  0x2299FF   // high-beam lamp lit
#define VROD_AMBER_WARNING   0xFFAA00   // ABS / check-engine lamp lit
#define VROD_RED_WARNING     0xFF2200   // oil / battery / immobiliser lamp lit
#define VROD_LAMP_OFF        0x222222   // any indicator lamp when inactive
