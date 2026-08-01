local jumpRequested = false

function OnCreate(entity)
    LogInfo("=== Movement script OnCreate called for entity: " .. tostring(entity) .. " ===")
end

function OnUpdate(entity, ts)
    local moveSpeed = 10.0
    local speed = moveSpeed * ts
    local jumpForce = 10.0

    local isGrounded = Raycast(entity, 0, -0.5, 0, -1, 0.2)

    local keyA     = IsKeyPressed("A")     or IsKeyPressed("Left")
    local keyD     = IsKeyPressed("D")     or IsKeyPressed("Right")
    local keySpace = IsKeyPressed("Space") or IsKeyPressed("W") or IsKeyPressed("Up")

    if keyA then
        Translate(entity, -speed, 0)
    elseif keyD then
        Translate(entity, speed, 0)
    end

    -- Only jump on the initial keydown, not every frame while held
    if keySpace and isGrounded and not jumpRequested then
        ApplyLinearImpulse(entity, 0, jumpForce)
        jumpRequested = true
    end

    -- Reset when key is released
    if not keySpace then
        jumpRequested = false
    end
end

function OnDestroy(entity)
    LogInfo("=== Movement script OnDestroy called for entity: " .. tostring(entity) .. " ===")
end