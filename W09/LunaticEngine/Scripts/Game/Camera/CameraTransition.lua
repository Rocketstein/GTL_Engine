local CameraTransition = {}

function CameraTransition.To(actor, target, duration, blend_type, blend_exp, lock_outgoing)
    if not actor or not actor.SetViewTargetWithBlend then
        print("[CameraTransition] SetViewTargetWithBlend missing")
        return false
    end

    if not target then
        print("[CameraTransition] target is nil")
        return false
    end

    duration = duration or 1.0
    blend_type = blend_type or "EaseInOut"
    blend_exp = blend_exp or 2.0
    lock_outgoing = lock_outgoing or false

    return actor:SetViewTargetWithBlend(
        target,
        duration,
        blend_type,
        blend_exp,
        lock_outgoing
    )
end

function CameraTransition.Cut(actor, target)
    if not actor or not actor.SetViewTarget then
        print("[CameraTransition] SetViewTarget missing")
        return false
    end

    if not target then
        print("[CameraTransition] target is nil")
        return false
    end

    return actor:SetViewTarget(target)
end

return CameraTransition
