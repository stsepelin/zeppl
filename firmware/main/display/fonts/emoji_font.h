#pragma once

// Bring up the FreeType-backed color-emoji font and attach it as the
// fallback on jbm_bold_26 / jbm_bold_33. Call once at boot, after lv_init
// has set up the LVGL filesystem layer (i.e. after bsp_display_start_*).
// Silently no-ops if FreeType isn't configured in.
//
// Implementation notes:
//   - emoji.ttf (Noto-COLRv1 subset, ~1.2 MB) is embedded via EMBED_FILES
//     in main/CMakeLists.txt. We do not load from microSD.
//   - LVGL's memfs driver wraps that buffer as a virtual file, which the
//     FreeType create-font path then opens via the normal lv_fs_open API.
//   - The two jbm_bold_* fonts are patched at CMake configure time to
//     drop `const` on the lv_font_t struct so we can write the fallback
//     pointer here — see main/CMakeLists.txt.
void emoji_font_init(void);
