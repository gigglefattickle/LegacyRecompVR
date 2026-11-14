#pragma once

namespace vr
{
    struct LocomotionInput
    {
        float moveX = 0.0f;       // Left stick: -1..1
        float moveY = 0.0f;       // Left stick: -1..1 (positive is forward)
        float turnX = 0.0f;       // Right stick horizontal: -1..1
        float headYawRadians = 0.0f;
        float deltaSeconds = 0.0f;
        bool sprint = false;
        bool jump = false;
        bool crouch = false;
    };

    struct LocomotionSettings
    {
        float moveDeadzone = 0.18f;
        float turnDeadzone = 0.20f;
        float snapTurnThreshold = 0.70f;
        float snapTurnDegrees = 30.0f;
        float snapTurnReleaseThreshold = 0.25f;
        float smoothTurnRadiansPerSecond = 1.8f;
        float walkSpeed = 1.0f;
        float sprintMultiplier = 1.5f;
        bool snapTurning = true;
    };

    struct LocomotionOutput
    {
        float worldMoveX = 0.0f;
        float worldMoveZ = 0.0f;
        float turnDeltaRadians = 0.0f;
        float yawOffsetRadians = 0.0f;
        bool jump = false;
        bool sprint = false;
        bool crouch = false;
    };

    // Converts Quest-style stick input into head-relative world movement.
    // The caller applies turnDeltaRadians to the XR play-space yaw and uses
    // yawOffsetRadians when calculating movement relative to the headset.
    class Locomotion
    {
    public:
        LocomotionOutput Update(const LocomotionInput& input, const LocomotionSettings& settings);
        void Reset();

    private:
        float m_yawOffsetRadians = 0.0f;
        bool m_snapTurnArmed = true;
    };
}
