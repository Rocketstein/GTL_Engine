-- ============================================================
-- TestPlayer.lua
--   WASD: 평면 이동, Space: 위로, LCtrl: 아래로
--   Q/E:  Yaw 회전
--   F:    한 번 누르면 위치 초기화
--
-- 사용법: 에디터에서 임의의 가시(visible) 액터(예: Cube StaticMeshActor)를
--        선택 → Details에 UScriptComponent 추가 → ScriptPath를
--        "Scripts/TestPlayer.lua"로 설정 → 씬 저장 → 패키징.
-- ============================================================
local _start = vec3(0, 0, 0)
local cam = nil -- 카메라 캐싱

function BeginPlay()
    -- 시작 위치 기억 — F 키로 복귀
    _start = obj.Location
    print("[TestPlayer] BeginPlay at " .. tostring(_start.x) .. ", " .. tostring(_start.y) .. ", " .. tostring(_start.z))
    
    cam = obj:FindComponentByClass("CameraComponent")
    if cam and cam:IsValid() then
        print("[TestPlayer] Camera cached successfully!")
    else
        print("[TestPlayer] FAILED to find CameraComponent. Mouse pitch rotation will not work.")
    end
end

local _tick_count = 0

function Tick(dt)
    _tick_count = _tick_count + 1
    if _tick_count % 300 == 0 then
        print("[TestPlayer] Heartbeat - Tick " .. tostring(_tick_count))
    end

    local speed = 5.0
    local turnSpeed = 5.0

    -- 회전 처리 (Mouse + Q/E)
    local r = obj.Rotation

    -- 마우스 Yaw (Actor 회전)
    local mouse_dx = GetMouseDeltaX()
    if math.abs(mouse_dx) > 0.001 then
        r.yaw = r.yaw + mouse_dx * 0.15
    end
    
    if GetKey("E") then r.yaw = r.yaw + turnSpeed * dt end
    if GetKey("Q") then r.yaw = r.yaw - turnSpeed * dt end
    obj.Rotation = r

    -- 마우스 Pitch (Camera 회전)
    if cam and cam:IsValid() then
        local mouse_dy = GetMouseDeltaY()
        if math.abs(mouse_dy) > 0.001 then
            local cr = cam:GetLocalRotation()
            cr.pitch = cr.pitch + mouse_dy * 0.15
            if cr.pitch > 80 then cr.pitch = 80 end
            if cr.pitch < -80 then cr.pitch = -80 end
            cam:SetLocalRotation(cr)
        end
    end

    -- 2. 이동 처리 (카메라 방향 기준 상대 이동)
    local forward, right
    
    if cam and cam:IsValid() then
        forward = cam:GetForwardVector()
        right = cam:GetRightVector()
        
        -- 지면 이동을 위해 Z축 성분 제거
        forward.z = 0
        right.z = 0
        
        -- 정규화
        local f_len = math.sqrt(forward.x * forward.x + forward.y * forward.y)
        local r_len = math.sqrt(right.x * right.x + right.y * right.y)
        if f_len > 0.001 then 
            forward.x, forward.y = forward.x/f_len, forward.y/f_len 
        else
            forward = obj:GetForwardVector()
            forward.z = 0
        end
        
        if r_len > 0.001 then 
            right.x, right.y = right.x/r_len, right.y/r_len 
        else
            right = obj:GetRightVector()
            right.z = 0
        end
    else
        forward = obj:GetForwardVector()
        right = obj:GetRightVector()
    end

    local mx, my, mz = 0, 0, 0
    if GetKey("W") then
        mx = mx + forward.x
        my = my + forward.y
        mz = mz + forward.z
    end
    if GetKey("S") then
        mx = mx - forward.x
        my = my - forward.y
        mz = mz - forward.z
    end
    if GetKey("D") then
        mx = mx + right.x
        my = my + right.y
        mz = mz + right.z
    end
    if GetKey("A") then
        mx = mx - right.x
        my = my - right.y
        mz = mz - right.z
    end
    
    if GetKey("Space")    then mz = mz + 1 end
    if GetKey("LControl") then mz = mz - 1 end

    if mx ~= 0 or my ~= 0 or mz ~= 0 then
        obj:AddWorldOffset(mx * speed * dt, my * speed * dt, mz * speed * dt)
    end

    -- 위치 리셋
    if GetKeyDown("F") then
        obj.Location = _start
        print("[TestPlayer] reset to start")
    end

    if GetKeyDown("0") then
        print("[TestPlayer] '0' Key Down - Triggering Effects")
        if cam and cam:IsValid() then
            cam:StartCameraShake(2.0, 0.7)
            cam:AddHitEffect(5.0, 1.0)
        end
    end
end

function EndPlay()
    print("[TestPlayer] EndPlay")
end