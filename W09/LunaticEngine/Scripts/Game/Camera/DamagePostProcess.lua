local CameraConfig = require("Game.Config.Camera")

local DamagePostProcess = {}

local function get_config()
    return CameraConfig.hit_feedback or {}
end

function DamagePostProcess:Begin(params)
    params = params or {}
    local config = get_config()

    self.elapsed = 0.0
    self.duration = params.duration or config.post_process_duration or 0.7
    self.intensity = params.intensity or config.post_process_intensity or 1.0
    self.material = config.post_process_material or "Game/PostProcess/Damage"
end

function DamagePostProcess:Update(deltaTime, view)
    self.elapsed = self.elapsed + deltaTime

    local alpha = math.max(0.0, 1.0 - self.elapsed / self.duration)
    local intensity = self.intensity * alpha
    local chromaticAberration = 0.2 * alpha

    view.postProcess:AddMaterial(self.material, 1.0)
    view.postProcess:SetMaterialScalar(self.material, "HitEffectIntensity", intensity)
    view.postProcess:SetMaterialScalar(self.material, "ChromaticAberration", chromaticAberration)

    return self.elapsed >= self.duration
end

return DamagePostProcess
