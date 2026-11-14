#include <os/media.h>

bool os::media::IsExternalMediaPlaying()
{
    // Android audio focus integration is handled by the SDL audio backend.
    return false;
}
