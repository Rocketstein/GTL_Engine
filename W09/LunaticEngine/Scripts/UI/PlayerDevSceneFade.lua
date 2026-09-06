local Engine = require("Common.Engine")
local Math = require("Common.Math")

local FadeSettings = {
    -- fade_duration: PlayerDev 씬 진입 흰색 flash fade 시간입니다.
    fade_duration = 0.42,
}

local fade_elapsed = 0.0
local fade_duration = FadeSettings.fade_duration
local flash = nil

------------------------------------------------
-- PlayerDev 씬 페이드 함수들
------------------------------------------------

function BeginPlay()
    flash = Engine.GetComponent(obj, "WhiteFlash")
    if not flash then
        return
    end

    flash:SetVisible(true)
    flash:SetTint(1.0, 1.0, 1.0, 1.0)
end

function Tick(dt)
    if not flash then
        return
    end

    fade_elapsed = fade_elapsed + (dt or 0.0)
    local progress = Math.Clamp01(fade_elapsed / fade_duration)
    local alpha = 1.0 - progress
    flash:SetTint(1.0, 1.0, 1.0, alpha)

    if progress >= 1.0 then
        flash:SetVisible(false)
        flash = nil
    end
end
