local Log = {}
------------------------------------------------------------------------------
-- Console 창 Log 출력 Helper
------------------------------------------------------------------------------

-- Log 출력 가능 검사
function Log.IsEnabled(config)
    if config == nil then
        return false
    end

    if config.enable_log ~= nil then
        return config.enable_log == true
    end

    return config.debug ~= nil and config.debug.enable_log == true
end

-- 실제 log 출력 함수
function Log.Write(config, message, prefix)
    if Log.IsEnabled(config) then
        print((prefix or "") .. tostring(message))
    end
end

-- lua 스크립트명으로 log 형식 만드는 함수
function Log.MakeLogger(config, prefix)
    return function(message)
        Log.Write(config, message, prefix)
    end
end

return Log
