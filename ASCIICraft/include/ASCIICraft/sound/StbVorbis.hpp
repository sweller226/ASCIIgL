#pragma once

/// Declarations-only view of the vendored stb_vorbis.
///
/// stb_vorbis ships as a single .c file that is both header and implementation,
/// switched by STB_VORBIS_HEADER_ONLY. The implementation is compiled exactly
/// once, in src/sound/stb_vorbis_impl.cpp; every other consumer includes this
/// header instead so the incantation lives in one place.
///
/// Include this rather than "stb_vorbis.c" directly - including that file
/// without the guard below drags a second copy of the decoder into the
/// translation unit and the link fails on duplicate symbols.

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
