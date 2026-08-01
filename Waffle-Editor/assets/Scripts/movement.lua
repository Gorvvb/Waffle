-- Waffle Player Movement Script
-- Works with A / D / Space, Arrow Keys, or W / S

function OnCreate(entity)
    LogInfo("=== Movement script OnCreate called for entity: " .. tostring(entity) .. " ===")
end

function OnUpdate(entity, ts)
    local moveSpeed = 10.0
    local speed = moveSpeed * ts

    local keyA = IsKeyPressed("A") or IsKeyPressed("Left") or IsKeyPressed(Key.A) or IsKeyPressed(Key.Left)
    local keyD = IsKeyPressed("D") or IsKeyPressed("Right") or IsKeyPressed(Key.D) or IsKeyPressed(Key.Right)
    local keySpace = IsKeyPressed("Space") or IsKeyPressed("W") or IsKeyPressed("Up") or IsKeyPressed(Key.Space)

    if keyA then
        LogInfo("KEY A / LEFT DETECTED! Moving entity " .. tostring(entity) .. " left by " .. tostring(-speed))
        Translate(entity, -speed, 0)
    elseif keyD then
        LogInfo("KEY D / RIGHT DETECTED! Moving entity " .. tostring(entity) .. " right by " .. tostring(speed))
        Translate(entity, speed, 0)
    end

    if keySpace then
        LogInfo("KEY SPACE / JUMP DETECTED! Applying impulse to entity " .. tostring(entity))
        ApplyLinearImpulse(entity, 0, 10.0)
    end
end

function OnDestroy(entity)
    LogInfo("=== Movement script OnDestroy called for entity: " .. tostring(entity) .. " ===")
end
