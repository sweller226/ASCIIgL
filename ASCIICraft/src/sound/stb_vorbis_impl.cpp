/// The one and only translation unit that compiles stb_vorbis.
///
/// This lived inside SoundSystem.cpp until music streaming needed the decoder
/// in MusicStream.cpp (and the tests needed it for a ground-truth comparison).
/// Everything else includes <ASCIICraft/sound/StbVorbis.hpp> for declarations.
///
/// Kept free of other includes so the decoder's own configuration macros cannot
/// be perturbed by whatever a neighbouring header happens to define.

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"
