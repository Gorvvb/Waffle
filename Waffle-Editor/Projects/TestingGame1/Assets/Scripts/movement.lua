local jumpRequested = false
local facingRight = true

function OnCreate(entity)
    LogInfo("=== Movement script OnCreate called for entity: " .. tostring(entity) .. " ===")
end

function OnUpdate(entity, ts)
    local moveSpeed = 10.0
    local speed = moveSpeed * ts
    local jumpForce = 15.0

    local isGrounded = Raycast(entity, 0, -1.0, 0, -1, 0.2)

    local keyA     = IsKeyPressed("A")     or IsKeyPressed("Left")
    local keyD     = IsKeyPressed("D")     or IsKeyPressed("Right")
    local keySpace = IsKeyPressed("Space") or IsKeyPressed("W") or IsKeyPressed("Up")

    -- Movement with sprite flipping via SetScale
    if keyA then
        SetLinearVelocity(entity, -moveSpeed, select(2, GetLinearVelocity(entity)))
        if facingRight then
            local sx, sy, sz = GetScale(entity)
            SetScale(entity, -math.abs(sx), sy, sz)
            facingRight = false
        end
    elseif keyD then
        SetLinearVelocity(entity, moveSpeed, select(2, GetLinearVelocity(entity)))
        if not facingRight then
            local sx, sy, sz = GetScale(entity)
            SetScale(entity, math.abs(sx), sy, sz)
            facingRight = true
        end
    else
        -- No horizontal input: bleed off horizontal velocity only
        local vx, vy = GetLinearVelocity(entity)
        SetLinearVelocity(entity, 0, vy)
    end

    -- Jump
    if keySpace and isGrounded and not jumpRequested then
        ApplyLinearImpulse(entity, 0, jumpForce)
        jumpRequested = true
    end

    if not keySpace then
        jumpRequested = false
    end
end

function OnCollisionBegin(entity, other)
    LogInfo("Entity " .. tostring(entity) .. " collided with " .. tostring(other))
end

function OnCollisionEnd(entity, other)
    LogInfo("Entity " .. tostring(entity) .. " stopped colliding with " .. tostring(other))
end

function OnDestroy(entity)
    LogInfo("=== Movement script OnDestroy called for entity: " .. tostring(entity) .. " ===")
end