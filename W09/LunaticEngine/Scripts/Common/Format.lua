local Math = require("Common.Math")
------------------------------------------------------------------------------
-- 문자열에서 사용하는 Formatting Helper
------------------------------------------------------------------------------
local Format = {}

-- 문자를 숫자로 바꿔주는 함수
function Format.Number(value)
    local formatted = tostring(math.floor(value or 0))
    local sign = ""
    if string.sub(formatted, 1, 1) == "-" then
        sign = "-"
        formatted = string.sub(formatted, 2)
    end

    local parts = {}
    while #formatted > 3 do
        table.insert(parts, 1, string.sub(formatted, -3))
        formatted = string.sub(formatted, 1, #formatted - 3)
    end
    table.insert(parts, 1, formatted)

    return sign .. table.concat(parts, ",")
end

-- 소수점 자리수 맞춰서 string formatting 하는 함수
function Format.Decimal(value, digits)
    return string.format("%." .. tostring(digits or 1) .. "f", value or 0)
end
-- percentage로 formatting 하는 함수
function Format.Percent(current, max_value)
    local safe_max = max_value or 0
    if safe_max <= 0 then
        return "0%"
    end

    local ratio = Math.Clamp((current or 0) / safe_max, 0.0, 1.0)
    return tostring(Math.Round(ratio * 100)) .. "%"
end

return Format
