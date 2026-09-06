local EquipmentModule = {}

local function set_particle_spawn_scale(particle, scale)
    if particle == nil then
        return
    end

    if Particle ~= nil and Particle.SetRuntimeSpawnRateScale ~= nil then
        Particle.SetRuntimeSpawnRateScale(particle, scale)
    elseif particle.SetRuntimeSpawnRateScale ~= nil then
        particle:SetRuntimeSpawnRateScale(scale)
    end
end

local function get_object_name(value)
    if value == nil or value.GetName == nil then
        return "nil"
    end

    return value:GetName()
end

local function attach_trail_particle(ctx, socketName)
    if Particle == nil or Particle.AttachSystemToSocket == nil then
        if not ctx.equipment.trailBindingWarned then
            --If this prints, rebuild C++ first--
            print("[CharacterController] Particle.AttachSystemToSocket binding missing")
            ctx.equipment.trailBindingWarned = true
        end
        return nil
    end

    local particle = Particle.AttachSystemToSocket(ctx.obj, ctx.config.SWORD_TRAIL_PARTICLE_PATH, socketName)
    if particle == nil then
        print("[CharacterController] sword trail attach failed: socket=" .. socketName
            .. " path=" .. ctx.config.SWORD_TRAIL_PARTICLE_PATH)
        return nil
    end

    --Keep sword trail ready, but do not spawn until attack--
    set_particle_spawn_scale(particle, 0.0)
    if particle.Activate ~= nil then
        particle:Activate(true)
    end
    if particle.ResetParticles ~= nil then
        particle:ResetParticles()
    end

    print("[CharacterController] sword trail attached: socket=" .. socketName
        .. " component=" .. get_object_name(particle))
    return particle
end

local function set_trail_particle_component_active(particle, active)
    if particle == nil then
        return
    end

    if active then
        if particle.Activate ~= nil then
            particle:Activate(false)
        end
        return
    end

    if particle.Deactivate ~= nil then
        particle:Deactivate()
    end
end

local function set_trail_particle_emission(particle, active)
    if particle == nil then
        return
    end

    if active and particle.Activate ~= nil then
        particle:Activate(false)
    end

    set_particle_spawn_scale(particle, active and 1.0 or 0.0)
end

local function get_trail_grace_time(ctx)
    if ctx == nil or ctx.config == nil then
        return 0.25
    end

    return ctx.config.SWORD_TRAIL_GRACE_TIME or 0.25
end

function EquipmentModule.BeginPlay(ctx)
    if ctx.equipment.RweaponComponent ~= nil or ctx.obj == nil then
        return
    end
    if ctx.equipment.LweaponComponent ~= nil or ctx.obj == nil then
        return
    end

    if Equipment == nil or Equipment.AttachStaticMeshToSocket == nil then
        if not ctx.equipment.weaponBindingWarned then
            --If this prints, rebuild C++ first--
            print("[CharacterController] Equipment.AttachStaticMeshToSocket binding missing")
            ctx.equipment.weaponBindingWarned = true
        end
        return
    end

    --Create weapon component and attach to hand socket--
    ctx.equipment.LweaponComponent = Equipment.AttachStaticMeshToSocket(ctx.obj, ctx.config.WEAPON_MESH_PATH, ctx.config.LWEAPON_SOCKET, Vec3(1.0,1.0,1.0))
    ctx.equipment.RweaponComponent = Equipment.AttachStaticMeshToSocket(ctx.obj, ctx.config.WEAPON_MESH_PATH, ctx.config.RWEAPON_SOCKET, Vec3(1.0,1.0,1.0))

    --Create sword trail particles without touching counter particles--
    ctx.equipment.LtrailParticle = attach_trail_particle(ctx, ctx.config.LSWORD_TRAIL_PARTICLE_SOCKET)
    ctx.equipment.RtrailParticle = attach_trail_particle(ctx, ctx.config.RSWORD_TRAIL_PARTICLE_SOCKET)
end

function EquipmentModule.ActivateTrail(ctx)
    if ctx == nil or ctx.equipment == nil then
        return
    end

    ctx.equipment.trailActive = true
    ctx.equipment.trailDeactivateRemaining = 0.0

    set_trail_particle_component_active(ctx.equipment.LtrailParticle, true)
    set_trail_particle_component_active(ctx.equipment.RtrailParticle, true)
    EquipmentModule.SetTrailEmission(ctx, true)
end

function EquipmentModule.SetTrailEmission(ctx, active)
    if ctx == nil or ctx.equipment == nil then
        return
    end

    if ctx.equipment.trailEmissionActive == active then
        return
    end

    ctx.equipment.trailEmissionActive = active
    set_trail_particle_emission(ctx.equipment.LtrailParticle, active)
    set_trail_particle_emission(ctx.equipment.RtrailParticle, active)
end

function EquipmentModule.RequestTrailDeactivate(ctx)
    if ctx == nil or ctx.equipment == nil then
        return
    end

    EquipmentModule.SetTrailEmission(ctx, false)
    ctx.equipment.trailDeactivateRemaining = math.max(
        ctx.equipment.trailDeactivateRemaining or 0.0,
        get_trail_grace_time(ctx)
    )
end

function EquipmentModule.DeactivateTrailNow(ctx)
    if ctx == nil or ctx.equipment == nil then
        return
    end

    EquipmentModule.SetTrailEmission(ctx, false)
    ctx.equipment.trailActive = false
    ctx.equipment.trailDeactivateRemaining = 0.0
    set_trail_particle_component_active(ctx.equipment.LtrailParticle, false)
    set_trail_particle_component_active(ctx.equipment.RtrailParticle, false)
end

function EquipmentModule.DeactivateTrail(ctx)
    EquipmentModule.RequestTrailDeactivate(ctx)
end

function EquipmentModule.Tick(ctx, dt)
    if ctx == nil or ctx.equipment == nil then
        return
    end

    if not ctx.equipment.trailActive then
        return
    end

    local remaining = ctx.equipment.trailDeactivateRemaining or 0.0
    if remaining <= 0.0 then
        return
    end

    remaining = remaining - (dt or 0.0)
    ctx.equipment.trailDeactivateRemaining = remaining
    if remaining <= 0.0 then
        EquipmentModule.DeactivateTrailNow(ctx)
    end
end

return EquipmentModule
