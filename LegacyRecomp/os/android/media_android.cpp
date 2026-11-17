#include <os/media.h>

bool os::media::IsExternalMediaPlaying()
{
    // Android media-session integration is not part of the initial platform bring-up.
    return false;
}
