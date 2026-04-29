#pragma once

#include <tracy/Tracy.hpp>

#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_FRAME_MARK() FrameMark
#define PROFILE_SCOPE_DEBUG(name) ZoneScopedN(name)
