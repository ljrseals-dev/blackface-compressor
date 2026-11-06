// Blackface Compressor - plug-in identity.
#pragma once

#include "pluginterfaces/base/funknown.h"

#define BF_PLUGIN_NAME "Blackface Compressor"
#define BF_VENDOR "theRealla"
#define BF_URL "https://github.com/ljrseals-dev"
#define BF_EMAIL "mailto:lewis@example.com"
#define BF_VERSION_STR "1.0.0"

namespace bf {
// Generated once for this plug-in; keep them stable across releases.
inline const Steinberg::FUID kProcessorUID(0x3D5B1C42, 0x9A7E4F18, 0xB0C2D6E1, 0x58A7F3C9);
inline const Steinberg::FUID kControllerUID(0x7E91A3D5, 0x24C84B6F, 0x91D7E0A2, 0x6B3F5C48);
}  // namespace bf
