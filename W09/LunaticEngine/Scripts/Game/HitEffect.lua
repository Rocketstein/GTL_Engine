local Engine = require("Common.Engine")
local Math = require("Common.Math")
local Vector = require("Common.Vector")

local HitEffects = {}

local active_time_token = 0
local active_squash_token = 0

-- GlobalTimeDilation 설정 함수
local function set_time_dilation(scale)
    if set_global_time_dilation then
        set_global_time_dilation(scale)
    end
end

local function wait_real_seconds(seconds)
    if wait_real then
        wait_real(Math.SafeNumber(seconds, 0.0))
    end
end

local function get_raw_delta_time()
    if raw_delta_time then
        local dt = raw_delta_time()
        if type(dt) == "number" and dt > 0.0 then
            return dt
        end
    end

    return 1.0 / 60.0 -- 실패한다면 60fps 가정하고 return
end

local function apply_scale(use_component, component, actor, scale)
    if use_component then
        if Engine.IsValidComponent(component) then
            component:SetLocalScale(scale)
        end
        return
    end

    if Engine.IsValidActor(actor) then
        actor:SetWorldScale(scale)
    end
end

------------------------------------------------
-- 특수효과 모음
------------------------------------------------

-- 안전하게 값을 복구해주는 함수
function HitEffects.StopTimeEffects()
    active_time_token = active_time_token + 1
    set_time_dilation(1.0)
end

-- HitStop
function HitEffects.HitStop(duration)
    duration = Math.SafeNumber(duration, 0.055)

    active_time_token = active_time_token + 1
    local token = active_time_token

    if duration > 0.0 then
        set_time_dilation(0.0)
        wait_real_seconds(duration)
    end

    if token == active_time_token then
        set_time_dilation(1.0)
    end
end

-- Slomo
function HitEffects.Slomo(scale, duration)
    scale = Math.Clamp(Math.SafeNumber(scale, 0.25), 0.05, 1.0)
    duration = Math.SafeNumber(duration, 0.18)

    active_time_token = active_time_token + 1
    local token = active_time_token

    if duration > 0.0 then
        set_time_dilation(scale)
        wait_real_seconds(duration)
    end

    if token == active_time_token then
        set_time_dilation(1.0)
    end
end
-- Hit stop 이후 Slomo 진행 함수
function HitEffects.HitStopAndSlomo(hit_stop_duration, slomo_scale, slomo_duration)
    hit_stop_duration = Math.SafeNumber(hit_stop_duration, 0.055)
    slomo_scale = Math.Clamp(Math.SafeNumber(slomo_scale, 0.25), 0.05, 1.0)
    slomo_duration = Math.SafeNumber(slomo_duration, 0.18)

    active_time_token = active_time_token + 1
    local token = active_time_token

    if hit_stop_duration > 0.0 then
        set_time_dilation(0.0)
        wait_real_seconds(hit_stop_duration)
    end

    if token ~= active_time_token then
        return
    end

    if slomo_duration > 0.0 then
        set_time_dilation(slomo_scale)
        wait_real_seconds(slomo_duration)
    end

    if token == active_time_token then
        set_time_dilation(1.0)
    end
end

------------------------------------------------
-- Hit Squash
------------------------------------------------

-- 피격 순간 actor 또는 mesh scale을 squash 목표값으로 보간한 뒤,
-- 원래 scale로 되돌려 짧은 타격감을 만드는 연출 함수입니다.
function HitEffects.PlayHitSquash(actor, squash_x, squash_y, squash_z, squash_duration, recover_duration)
    if not Engine.IsValidActor(actor) then
        return
    end

    -- Default값과 같이 넣기
    squash_x = Math.SafeNumber(squash_x, 1.25)
    squash_y = Math.SafeNumber(squash_y, 0.75)
    squash_z = Math.SafeNumber(squash_z, 1.0)
    squash_duration = Math.SafeNumber(squash_duration, 0.045)
    recover_duration = Math.SafeNumber(recover_duration, 0.085)

    active_squash_token = active_squash_token + 1
    local token = active_squash_token

    local comp = Engine.GetStaticMeshComponent(actor)
    local use_component = Engine.IsValidComponent(comp)
    local original_scale = nil

    if use_component then
        original_scale = comp:GetLocalScale()
    elseif Engine.IsValidActor(actor) then
        original_scale = actor:GetWorldScale()
    end

    if not original_scale then
        return
    end

    local target_scale = Vector.Make(
        Vector.GetX(original_scale) * squash_x,
        Vector.GetY(original_scale) * squash_y,
        Vector.GetZ(original_scale) * squash_z
    )

    local elapsed = 0.0

    while elapsed < squash_duration do
        if token ~= active_squash_token then
            apply_scale(use_component, comp, actor, original_scale)
            return
        end

        local alpha = Math.Clamp(elapsed / squash_duration, 0.0, 1.0)
        alpha = Math.EaseOutQuad(alpha)
        apply_scale(use_component, comp, actor, Vector.Lerp(original_scale, target_scale, alpha))

        wait_real_seconds(0.0)
        elapsed = elapsed + get_raw_delta_time()
    end

    apply_scale(use_component, comp, actor, target_scale)
    elapsed = 0.0

    while elapsed < recover_duration do
        if token ~= active_squash_token then
            apply_scale(use_component, comp, actor, original_scale)
            return
        end

        local alpha = Math.Clamp(elapsed / recover_duration, 0.0, 1.0)
        alpha = Math.EaseInOutQuad(alpha)
        apply_scale(use_component, comp, actor, Vector.Lerp(target_scale, original_scale, alpha))

        wait_real_seconds(0.0)
        elapsed = elapsed + get_raw_delta_time()
    end

    apply_scale(use_component, comp, actor, original_scale)
end

return HitEffects
