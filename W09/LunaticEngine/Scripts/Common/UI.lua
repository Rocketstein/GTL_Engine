local Engine = require("Common.Engine")
local Math = require("Common.Math")

------------------------------------------------------------------------------
-- UI Helper
------------------------------------------------------------------------------

local UI = {}

-- 컴포넌트 보이기/숨기기 (유효성 체크 후 실행)
function UI.SetVisible(component, visible)
    if Engine.IsValidComponent(component) then
        component:SetVisible(visible)
    end
end

-- 텍스트 설정(nil 이면 빈 문자열)
function UI.SetText(component, text)
    if Engine.IsValidComponent(component) then
        component:SetText(text or "")
    end
end

-- 라벨 텍스트 설정 (SetText랑 비슷, 다른 타입 UI용)
function UI.SetLabel(component, text)
    if Engine.IsValidComponent(component) then
        component:SetLabel(text or "")
    end
end

-- 텍스처 변경 (경로가 있을 때만 적용)
function UI.SetTexture(component, texture_path)
    if Engine.IsValidComponent(component) and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
    end
end

-- 캐시 테이블에서 이름으로 컴포넌트 가져오기
function UI.GetCachedComponent(cache, name)
    if type(cache) ~= "table" then
        return nil
    end

    return cache[name]
end

-- 캐시에서 꺼내서 설정하는 함수 목록
-- 캐시로 빠르게 접근할 수 있다
function UI.SetCachedVisible(cache, name, visible)
    UI.SetVisible(UI.GetCachedComponent(cache, name), visible)
end

function UI.SetCachedText(cache, name, text)
    UI.SetText(UI.GetCachedComponent(cache, name), text)
end

function UI.SetCachedTint(cache, name, r, g, b, a)
    UI.SetTint(UI.GetCachedComponent(cache, name), r, g, b, a)
end

function UI.SetCachedTexture(cache, name, texture_path)
    UI.SetTexture(UI.GetCachedComponent(cache, name), texture_path)
end

-- 색상 설정
function UI.SetTint(component, r, g, b, a)
    if not Engine.IsValidComponent(component) then
        return
    end

    if type(r) == "table" then
        component:SetTint(r[1], r[2], r[3], r[4])
        return
    end

    component:SetTint(r, g, b, a)
end

-- 화면 좌표 설정
function UI.SetScreenPosition(component, x, y, z)
    if Engine.IsValidComponent(component) then
        component:SetScreenPositionXYZ(x, y, z or 0.0)
    end
end

-- UI 크기 설정
function UI.SetScreenSize(component, width, height, z)
    if Engine.IsValidComponent(component) then
        component:SetScreenSizeXYZ(width, height, z or 0.0)
    end
end

-- 두 색상을 t 비율로 보간해서 새로운 색상 반환
function UI.LerpTint(from_tint, to_tint, t)
    local alpha = Math.Clamp01(t)

    return {
        Math.Lerp(from_tint[1], to_tint[1], alpha),
        Math.Lerp(from_tint[2], to_tint[2], alpha),
        Math.Lerp(from_tint[3], to_tint[3], alpha),
        Math.Lerp(from_tint[4], to_tint[4], alpha),
    }
end

-- 색상을 시간에 따라 점진적으로 변경
function UI.AnimateTint(component, from_tint, to_tint, duration, steps)
    if not Engine.IsValidComponent(component) then
        return
    end

    local frame_count = steps or 5
    if duration <= 0.0 or frame_count <= 0 then
        UI.SetTint(component, to_tint)
        return
    end

    local step_duration = duration / frame_count
    local index = 1
    while index <= frame_count do
        local t = index / frame_count
        UI.SetTint(component, UI.LerpTint(from_tint, to_tint, t))
        wait(step_duration)
        index = index + 1
    end
end

return UI
