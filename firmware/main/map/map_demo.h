#pragma once

// Self-contained on-device map demo (CONFIG_VROD_MAP_DEMO). Loads the embedded
// corridor tileset, shows the map screen, and animates the baked Tallinn route.
// Replaces the normal gauge boot when enabled; reflash a normal build to revert.
void map_demo_start(void);
