local CameraFade = {}

function CameraFade.FadeOut(actor, duration, r, g, b, a)
    if not actor or not actor.StartCameraFade then
        print("[CameraFade] StartCameraFade missing")
        return false
    end

    duration = duration or 0.5
    r = r or 0.0
    g = g or 0.0
    b = b or 0.0
    a = a or 1.0

    return actor:StartCameraFade(0.0, 1.0, duration, r, g, b, a)
end

function CameraFade.FadeIn(actor, duration, r, g, b, a)
    if not actor or not actor.StartCameraFade then
        print("[CameraFade] StartCameraFade missing")
        return false
    end

    duration = duration or 0.5
    r = r or 0.0
    g = g or 0.0
    b = b or 0.0
    a = a or 1.0

    return actor:StartCameraFade(1.0, 0.0, duration, r, g, b, a)
end

function CameraFade.Clear(actor)
    if actor and actor.EndCameraFade then
        return actor:EndCameraFade()
    end

    return false
end

return CameraFade
