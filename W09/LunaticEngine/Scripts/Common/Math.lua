local Math = {}
------------------------------------------------------------------------------
-- sol의 Math Library 제외하고 필요한 math helper
------------------------------------------------------------------------------

function Math.Clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

-- 0~1 사이로 Clamping (많이 필요)
function Math.Clamp01(value)
    return Math.Clamp(value, 0.0, 1.0)
end

-- type 검사해서 숫자 반환하는 함수
function Math.SafeNumber(value, fallback)
    if type(value) == "number" then
        return value
    end

    return fallback
end

-- 양수 받아오는 함수
function Math.SafeNonNegative(value, fallback)
    local number = Math.SafeNumber(value, fallback or 0)
    if number < 0 then
        return 0
    end

    return number
end

-- 선형 보간
function Math.Lerp(a, b, t)
    return a + (b - a) * t
end

-- 빠르게 시작해서 점점 느려지면서 끝나는 보간 함수
function Math.EaseOutQuad(t)
    return 1.0 - (1.0 - t) * (1.0 - t)
end

-- 느리게 시작 중간에 빠름 나중에 느려지는 함수
function Math.EaseInOutQuad(t)
    if t < 0.5 then
        return 2.0 * t * t
    end

    return 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0)) * 0.5
end

-- 부호 판정 함수
function Math.Sign(value)
    if value < 0.0 then
        return -1.0
    end

    if value > 0.0 then
        return 1.0
    end

    return 0.0
end

-- 반올림
function Math.Round(value)
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end

    return math.ceil(value - 0.5)
end

return Math
