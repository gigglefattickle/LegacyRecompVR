#include <os/user.h>

bool os::user::IsDarkTheme()
{
    // SDL2 does not expose Android's system theme consistently across API levels.
    return false;
}
