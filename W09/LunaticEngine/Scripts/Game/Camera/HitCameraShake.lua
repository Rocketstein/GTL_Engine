local HitCameraShake = {}

function HitCameraShake:Begin(params)
    params = params or {}

    self.elapsed = 0.0
    self.duration = params.duration or 0.7
    self.intensity = params.intensity or 2.0
    self.frequency = params.frequency or 25.0
end

function HitCameraShake:Update(deltaTime, view)
    self.elapsed = self.elapsed + deltaTime

    local alpha = math.max(0.0, 1.0 - self.elapsed / self.duration)
    local wave = math.sin(self.elapsed * self.frequency) * self.intensity * alpha

    view.location.y = view.location.y + wave
    view.location.z = view.location.z + wave * 0.5
    view.rotation.roll = view.rotation.roll + wave * 0.2
    view.rotation.yaw = view.rotation.yaw + wave * 0.1

    return self.elapsed >= self.duration
end

return HitCameraShake
