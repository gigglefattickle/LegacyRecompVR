#include "locomotion.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    float ApplyDeadzone(float value, float deadzone)
    {
        const float magnitude = std::abs(value);
        if (magnitude <= deadzone)
            return 0.0f;

        const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
        return std::copysign(std::clamp(scaled, 0.0f, 1.0f), value);
    }

    void ApplyRadialDeadzone(float& x, float& y, float deadzone)
    {
        const float magnitude = std::sqrt(x * x + y * y);
        if (magnitude <= deadzone)
        {
            x = 0.0f;
            y = 0.0f;
            return;
        }

        const float scaledMagnitude = std::clamp((magnitude - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
        const float scale = scaledMagnitude / magnitude;
        x *= scale;
        y *= scale;
    }

    float WrapRadians(float angle)
    {
        while (angle > Pi)
            angle -= 2.0f * Pi;
        while (angle < -Pi)
            angle += 2.0f * Pi;
        return angle;
    }
}

vr::LocomotionOutput vr::Locomotion::Update(const LocomotionInput& input, const LocomotionSettings& settings)
{
    LocomotionOutput output{};

    float moveX = input.moveX;
    float moveY = input.moveY;
    ApplyRadialDeadzone(moveX, moveY, std::clamp(settings.moveDeadzone, 0.0f, 0.95f));

    const float speed = settings.walkSpeed * (input.sprint ? settings.sprintMultiplier : 1.0f);
    const float movementYaw = input.headYawRadians + m_yawOffsetRadians;

    // Rotate the left-stick vector by the headset's horizontal facing.
    const float sinYaw = std::sin(movementYaw);
    const float cosYaw = std::cos(movementYaw);
    output.worldMoveX = (moveX * cosYaw) + (moveY * sinYaw);
    output.worldMoveZ = (moveX * -sinYaw) + (moveY * cosYaw);
    output.worldMoveX *= speed;
    output.worldMoveZ *= speed;

    const float turn = ApplyDeadzone(input.turnX, std::clamp(settings.turnDeadzone, 0.0f, 0.95f));
    if (settings.snapTurning)
    {
        if (std::abs(input.turnX) <= settings.snapTurnReleaseThreshold)
            m_snapTurnArmed = true;

        if (m_snapTurnArmed && std::abs(turn) >= settings.snapTurnThreshold)
        {
            output.turnDeltaRadians = std::copysign(settings.snapTurnDegrees * Pi / 180.0f, turn);
            m_yawOffsetRadians = WrapRadians(m_yawOffsetRadians + output.turnDeltaRadians);
            m_snapTurnArmed = false;
        }
    }
    else
    {
        output.turnDeltaRadians = turn * settings.smoothTurnRadiansPerSecond *
            std::clamp(input.deltaSeconds, 0.0f, 0.1f);
        m_yawOffsetRadians = WrapRadians(m_yawOffsetRadians + output.turnDeltaRadians);
    }

    output.yawOffsetRadians = m_yawOffsetRadians;
    output.jump = input.jump;
    output.sprint = input.sprint;
    output.crouch = input.crouch;
    return output;
}

void vr::Locomotion::Reset()
{
    m_yawOffsetRadians = 0.0f;
    m_snapTurnArmed = true;
}
