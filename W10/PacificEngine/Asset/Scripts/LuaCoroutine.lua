local Co = {}

function Co.Wait(Time)
    return coroutine.yield("wait_time", Time)
end

function Co.WaitGameplay(Time, IsPaused)
    local elapsed = 0.0
    local target = math.max(0.0, Time or 0.0)

    if target <= 0.0 then
        return Co.WaitNextFrame()
    end

    while elapsed < target do
        local deltaTime = Co.WaitNextFrame() or 0.0
        local paused = false

        if type(IsPaused) == "function" then
            paused = IsPaused() == true
        else
            paused = IsPaused == true
        end

        if not paused then
            elapsed = elapsed + deltaTime
        end
    end
end

function Co.WaitNextFrame()
    return coroutine.yield("wait_next_frame")
end

function Co.WaitUntil(Predicate)
    return coroutine.yield("wait_until", Predicate)
end

return Co
