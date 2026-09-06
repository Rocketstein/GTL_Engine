local DialogueUtils = {}

------------------------------------------------
-- UTF-8 문자 처리 함수들
------------------------------------------------

function DialogueUtils.split_utf8_chars(value)
    local chars = {}
    if type(value) ~= "string" or value == "" then
        return chars
    end

    local index = 1
    local length = #value

    while index <= length do
        local byte = string.byte(value, index)
        local char_length = 1

        if byte >= 0xF0 then
            char_length = 4
        elseif byte >= 0xE0 then
            char_length = 3
        elseif byte >= 0xC0 then
            char_length = 2
        end

        chars[#chars + 1] = value:sub(index, index + char_length - 1)
        index = index + char_length
    end

    return chars
end

function DialogueUtils.is_typing_sound_character(char)
    return char ~= nil and char ~= "" and char ~= " " and char ~= "\n" and char ~= "\t"
end

------------------------------------------------
-- 터미널 메시지 포맷 함수들
------------------------------------------------

-- 터미널풍 대화 텍스트를 폭 제한에 맞춰 줄바꿈하고,
-- 긴 줄은 공백 우선 분리와 들여쓰기 보정까지 한 번에 처리합니다.
function DialogueUtils.format_terminal_message(text, options)
    local source = type(text) == "string" and text or ""
    if source == "" then
        return ""
    end

    local settings = type(options) == "table" and options or {}
    local max_line_width_units = settings.max_line_width_units or 28
    local continuation_indent = settings.continuation_indent or "   "

    local function normalize_newlines(value)
        local result = {}
        local index = 1
        local length = #value

        while index <= length do
            local char = value:sub(index, index)
            if char == "\r" then
                result[#result + 1] = "\n"
                if index < length and value:sub(index + 1, index + 1) == "\n" then
                    index = index + 1
                end
            else
                result[#result + 1] = char
            end
            index = index + 1
        end

        return table.concat(result)
    end

    local function is_ascii_space(char)
        return char == " " or char == "\t"
    end

    local function char_width_units(char)
        local byte = string.byte(char, 1)
        if not byte then
            return 0
        end

        if byte < 0x80 then
            if is_ascii_space(char) then
                return 0.45
            end
            return 0.6
        end

        return 1.0
    end

    local function trim_spaces(value)
        local chars = DialogueUtils.split_utf8_chars(value)
        local start_index = 1
        local end_index = #chars

        while start_index <= end_index and is_ascii_space(chars[start_index]) do
            start_index = start_index + 1
        end

        while end_index >= start_index and is_ascii_space(chars[end_index]) do
            end_index = end_index - 1
        end

        if start_index > end_index then
            return ""
        end

        return table.concat(chars, "", start_index, end_index)
    end

    local function wrap_line(line)
        local normalized_line = trim_spaces(line)
        if normalized_line == "" then
            return { "" }
        end

        local chars = DialogueUtils.split_utf8_chars(normalized_line)
        local lines = {}
        local current = {}
        local last_space_index = nil
        local current_width = 0.0

        local function current_text(count)
            return table.concat(current, "", 1, count or #current)
        end

        local function push_current(text)
            lines[#lines + 1] = trim_spaces(text)
            current = {}
            last_space_index = nil
            current_width = 0.0
        end

        for _, char in ipairs(chars) do
            current[#current + 1] = char
            current_width = current_width + char_width_units(char)
            if char == " " then
                last_space_index = #current
            end

            if current_width >= max_line_width_units then
                if last_space_index and last_space_index > 1 then
                    local split_index = last_space_index
                    local snapshot = current
                    push_current(table.concat(snapshot, "", 1, split_index - 1))

                    local remaining = {}
                    for remaining_index = split_index + 1, #snapshot do
                        remaining[#remaining + 1] = snapshot[remaining_index]
                    end
                    current = remaining
                    current_width = 0.0
                    last_space_index = nil

                    for remaining_index, remaining_char in ipairs(current) do
                        current_width = current_width + char_width_units(remaining_char)
                        if remaining_char == " " then
                            last_space_index = remaining_index
                        end
                    end
                else
                    push_current(current_text())
                end
            end
        end

        if #current > 0 then
            push_current(current_text())
        end

        return lines
    end

    local normalized = normalize_newlines(source)
    local wrapped_lines = {}
    local current_line = {}

    local function flush_line()
        local raw_line = table.concat(current_line)
        current_line = {}

        if raw_line == "" and #wrapped_lines > 0 then
            wrapped_lines[#wrapped_lines + 1] = ""
        elseif raw_line ~= "" then
            local line_parts = wrap_line(raw_line)
            for _, line in ipairs(line_parts) do
                wrapped_lines[#wrapped_lines + 1] = line
            end
        end
    end

    local index = 1
    while index <= #normalized do
        local char = normalized:sub(index, index)
        if char == "\n" then
            flush_line()
        else
            current_line[#current_line + 1] = char
        end
        index = index + 1
    end

    if #current_line > 0 then
        flush_line()
    end

    if #wrapped_lines == 0 then
        return ""
    end

    local message = table.concat(wrapped_lines, "\n")
    if message == "" then
        return ""
    end

    return message:gsub("\n", "\n" .. continuation_indent)
end

------------------------------------------------
-- Typewriter 상태 함수들
------------------------------------------------

function DialogueUtils.create_typewriter_state(default_interval)
    return {
        active = false,
        chars = {},
        visible_count = 0,
        elapsed = 0.0,
        interval = default_interval or 0.028,
        full_text = "",
    }
end

function DialogueUtils.reset_typewriter(state, interval)
    state.active = false
    state.chars = {}
    state.visible_count = 0
    state.elapsed = 0.0
    state.interval = interval or state.interval or 0.028
    state.full_text = ""
end

function DialogueUtils.start_typewriter(state, text, interval)
    DialogueUtils.reset_typewriter(state, interval)
    state.full_text = type(text) == "string" and text or ""
    state.chars = DialogueUtils.split_utf8_chars(state.full_text)
    state.active = #state.chars > 0
end

function DialogueUtils.complete_typewriter(state)
    state.active = false
    state.visible_count = #state.chars
end

function DialogueUtils.update_typewriter(state, dt, on_char)
    if not state.active then
        return false
    end

    state.elapsed = state.elapsed + (dt or 0.0)
    local advanced = false

    while state.active
        and state.visible_count < #state.chars
        and state.elapsed >= state.interval do
        state.elapsed = state.elapsed - state.interval
        state.visible_count = state.visible_count + 1
        advanced = true

        if type(on_char) == "function" then
            on_char(state.chars[state.visible_count], state.visible_count)
        end
    end

    if state.visible_count >= #state.chars then
        state.active = false
    end

    return advanced
end

function DialogueUtils.get_visible_text(state, prefix)
    local lead = type(prefix) == "string" and prefix or ""
    if not state.full_text or state.full_text == "" or state.visible_count <= 0 then
        return lead
    end

    return lead .. table.concat(state.chars, "", 1, state.visible_count)
end

return DialogueUtils
