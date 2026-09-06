local ScenarioLoader = {}

local CHARACTER_TEXTURES = {
    profile_baek = "Asset/Content/Texture/Story/profile_baek.png",
    profile_lim = "Asset/Content/Texture/Story/profile_lim.png",

    portrait_baek_normal = "Asset/Content/Texture/UI/portrait_baek_normal_0.png",

    portrait_lim_normal = "Asset/Content/Texture/UI/portrait_lim_normal_0.png",
}

------------------------------------------------
-- 문자열 / 라인 처리 함수들
------------------------------------------------

local function starts_with(value, prefix)
    return type(value) == "string" and value:sub(1, #prefix) == prefix
end

local function copy_lines(lines)
    local result = {}
    if type(lines) ~= "table" then
        return result
    end

    for index = 1, #lines do
        result[#result + 1] = tostring(lines[index] or "")
    end

    return result
end

-- JSON 시나리오의 긴 대사를 UI 줄 수와 byte 제한에 맞춰 나눕니다.
-- 이미 줄바꿈이 들어온 텍스트는 작성자가 지정한 줄 구성을 우선 보존합니다.
local function wrap_text(text, max_lines, max_bytes)
    if type(text) ~= "string" or text == "" then
        return {}
    end

    if text:find("\n", 1, true) then
        local result = {}
        for line in text:gmatch("([^\n]+)") do
            result[#result + 1] = line
            if #result >= max_lines then
                break
            end
        end
        return result
    end

    local result = {}
    local current = ""
    local limit = max_bytes or 84

    for word in text:gmatch("%S+") do
        local candidate = current == "" and word or (current .. " " .. word)
        if current ~= "" and #candidate > limit then
            result[#result + 1] = current
            current = word
            if #result >= max_lines - 1 then
                break
            end
        else
            current = candidate
        end
    end

    if current ~= "" and #result < max_lines then
        result[#result + 1] = current
    end

    if #result == 0 then
        result[1] = text
    end

    return result
end

------------------------------------------------
-- 시나리오 로드 / Asset 조회 함수들
------------------------------------------------

function ScenarioLoader.load(path, json_loader)
    local loader = json_loader
    if type(loader) ~= "function" and type(load_json_file) == "function" then
        loader = load_json_file
    end

    if type(loader) ~= "function" then
        return nil
    end

    local data = loader(path)
    if type(data) ~= "table" then
        return nil
    end

    data.characters = data.characters or {}
    data.assets = data.assets or {}
    data.sequence = data.sequence or {}
    return data
end

function ScenarioLoader.resolve_asset(data, category, key)
    if type(data) ~= "table" or type(data.assets) ~= "table" then
        return nil
    end

    local section = data.assets[category]
    if type(section) ~= "table" then
        return nil
    end

    local value = section[key]
    if type(value) ~= "string" or value == "" then
        return nil
    end

    if starts_with(value, "Asset/") or value:find("%.") then
        return value
    end

    return nil
end

function ScenarioLoader.resolve_texture(image_id)
    if type(image_id) ~= "string" or image_id == "" then
        return nil
    end

    if starts_with(image_id, "Asset/") then
        return image_id
    end

    return CHARACTER_TEXTURES[image_id]
end

function ScenarioLoader.resolve_speaker_name(data, speaker_id)
    local character = type(data) == "table" and data.characters and data.characters[speaker_id]
    if type(character) ~= "table" then
        return speaker_id or ""
    end

    return character.displayName or character.shortName or speaker_id or ""
end

function ScenarioLoader.resolve_dialogue_texture(data, speaker_id, emotion)
    local character = type(data) == "table" and data.characters and data.characters[speaker_id]
    if type(character) ~= "table" then
        return nil
    end

    local image_id = nil
    if type(character.emotions) == "table" and type(emotion) == "string" and emotion ~= "" then
        image_id = character.emotions[emotion]
    end

    if type(image_id) ~= "string" or image_id == "" then
        image_id = character.defaultProfile or character.defaultPortrait or character.defaultImage
    end

    return ScenarioLoader.resolve_texture(image_id)
end

------------------------------------------------
-- 대화 줄 변환 함수들
------------------------------------------------

function ScenarioLoader.dialogue_lines(step, max_lines)
    local message = type(step) == "table" and step.message or nil
    return wrap_text(message, max_lines or 4, 84)
end

function ScenarioLoader.line_array(step, key)
    if type(step) ~= "table" then
        return {}
    end

    local value = step[key]
    if type(value) == "table" then
        return copy_lines(value)
    end

    if type(value) == "string" then
        return wrap_text(value, 4, 84)
    end

    return {}
end

return ScenarioLoader
