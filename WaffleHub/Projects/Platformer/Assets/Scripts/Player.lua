Public = {
    Speed       = 5.0,
    JumpForce   = 64.0,
    BulletSpeed = 15.0,
    BulletLife  = 2.0,
    IsGrounded  = false,
    FacingRight = true,
    CurrentAnim = "",
}

function OnCreate(entity)
    LogInfo("Player created!")
end

function OnUpdate(entity, ts)
    local vx, vy = GetLinearVelocity(entity)

    local moveInput = GetAxis("Horizontal")
    SetLinearVelocity(entity, moveInput * Public.Speed, vy)

    local sx, sy, sz = GetScale(entity)
    if moveInput > 0 and not Public.FacingRight then
        Public.FacingRight = true
        SetScale(entity, math.abs(sx), sy, sz)
    elseif moveInput < 0 and Public.FacingRight then
        Public.FacingRight = false
        SetScale(entity, -math.abs(sx), sy, sz)
    end

    local targetAnim = moveInput ~= 0 and "Running" or "Idle"
    if targetAnim ~= Public.CurrentAnim then
        Public.CurrentAnim = targetAnim
        PlayAnimation(entity, targetAnim)
    end

    local grass = FindEntityByName("Grass")
    Public.IsGrounded = false
    if grass ~= -1 then
        local hit, hitEntity = Raycast(entity, 0, -0.5, 0, -1, 0.65)
        if hit and hitEntity == grass then
            Public.IsGrounded = true
        end
    end

    if Public.IsGrounded and IsKeyJustPressed(Key.Space) then
        SetLinearVelocity(entity, vx, 0)
        ApplyLinearImpulse(entity, 0, Public.JumpForce)
    end

    if IsMouseJustPressed(Mouse.ButtonLeft) then
        PlaySound("Audio/test.mp3", 1.0, false)

        local px, py = GetPosition(entity)
        local mx, my = GetMousePosition()
        local wx, wy = ScreenToWorld(mx, my)

        local dx, dy = Vec2.Normalize(wx - px, wy - py)

        if dx == 0 and dy == 0 then
            dx = Public.FacingRight and 1.0 or -1.0
            dy = 0.0
        end

        local csx, csy, csz = GetScale(entity)
        if dx > 0 and not Public.FacingRight then
            Public.FacingRight = true
            SetScale(entity, math.abs(csx), csy, csz)
        elseif dx < 0 and Public.FacingRight then
            Public.FacingRight = false
            SetScale(entity, -math.abs(csx), csy, csz)
        end

        local bullet = InstantiatePrefab("Prefabs/Bullet.prefab", px, py)
        if bullet ~= -1 then
            SetLinearVelocity(bullet, dx * Public.BulletSpeed, dy * Public.BulletSpeed)
            SetRotation(bullet, math.atan(dy, dx))
        end
    end
end

function OnCollisionBegin(entity, other)
end

function OnCollisionEnd(entity, other)
end

function OnDestroy(entity)
    LogInfo("Player destroyed.")
end