local Context = require("FinalGameJamScript/Character/CharacterContext")
local Equipment = require("FinalGameJamScript/Character/CharacterEquipment")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")

local Counter = {}

local function counter_debug_log(ctx, message)
    if ctx ~= nil and ctx.config ~= nil and ctx.config.COUNTER_DEBUG_LOGS then
        print(message)
    end
end

local function atan2_degrees(y, x)
    if math.atan2 ~= nil then
        return math.deg(math.atan2(y, x))
    end

    if x > 0.0 then
        return math.deg(math.atan(y / x))
    end

    if x < 0.0 and y >= 0.0 then
        return math.deg(math.atan(y / x)) + 180.0
    end

    if x < 0.0 and y < 0.0 then
        return math.deg(math.atan(y / x)) - 180.0
    end

    if y > 0.0 then
        return 90.0
    end

    if y < 0.0 then
        return -90.0
    end

    return 0.0
end

local function smooth_step(alpha)
    alpha = math.max(0.0, math.min(1.0, alpha))
    return alpha * alpha * (3.0 - 2.0 * alpha)
end

local function lerp_vec(a, b, alpha)
    return a + (b - a) * alpha
end

local function get_owner_location(ctx)
    if ctx ~= nil and ctx.obj ~= nil and ctx.obj.Location ~= nil then
        return ctx.obj.Location
    end

    return nil
end

local function load_counter_montage(ctx)
    if ctx.cache.counterMontage == nil then
        ctx.cache.counterMontage = Context.LoadMontage(ctx.config.COUNTER_MONTAGE_PATH)
    end
end

local function is_counter_input_pressed(ctx)
    return Input.GetKeyDown(ctx.config.RMB)
end

local function load_counter_sounds(ctx)
    if ctx == nil or ctx.cache == nil or ctx.config == nil then
        return
    end

    if ctx.cache.counterParrySoundLoaded then
        return
    end

    if Audio == nil or Audio.Load == nil then
        return
    end

    local key = ctx.config.COUNTER_PARRY_SOUND_KEY or "Counter_Parry1"
    local path = ctx.config.COUNTER_PARRY_SOUND_PATH or "Parry1.wav"
    Audio.Load(key, path, false)
    ctx.cache.counterParrySoundLoaded = true
end

local function play_counter_parry_sound(ctx)
    if ctx == nil or ctx.config == nil then
        return
    end

    if Audio == nil or Audio.Play == nil then
        return
    end

    load_counter_sounds(ctx)
    local key = ctx.config.COUNTER_PARRY_SOUND_KEY or "Counter_Parry1"
    local volume = ctx.config.COUNTER_PARRY_SOUND_VOLUME or 0.6
    Audio.Play(key, volume)
end

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

local function are_counter_particles_enabled(ctx)
    return ctx ~= nil and ctx.config ~= nil and ctx.config.COUNTER_PARTICLE_ENABLED == true
end

local function create_counter_impact_particle(ctx, location)
    if not are_counter_particles_enabled(ctx) then
        return nil
    end

    if ctx == nil or ctx.cache == nil or ctx.config == nil or Particle == nil then
        return nil
    end

    local particle = ctx.cache.counterImpactParticle
    if particle ~= nil then
        return particle
    end

    if Particle.SpawnSystemAtLocation == nil then
        return nil
    end

    local spawnLocation = location or get_owner_location(ctx)
    if spawnLocation == nil then
        return nil
    end

    particle = Particle.SpawnSystemAtLocation(ctx.obj, ctx.config.COUNTER_PARTICLE_PATH, spawnLocation)
    ctx.cache.counterImpactParticle = particle
    if particle == nil then
        counter_debug_log(ctx, "[CharacterCounter] impact particle spawn failed")
        return nil
    end

    if Particle.SetSystemWorldScale ~= nil then
        Particle.SetSystemWorldScale(particle, ctx.config.COUNTER_IMPACT_PARTICLE_SCALE)
    end
    set_particle_spawn_scale(particle, 0.0)
    if particle.ResetParticles ~= nil then
        particle:ResetParticles()
    end

    return particle
end

local function activate_counter_impact_particle(ctx, hitLocation)
    if not are_counter_particles_enabled(ctx) then
        return false
    end

    if hitLocation == nil or Particle == nil then
        return false
    end

    local particle = create_counter_impact_particle(ctx, hitLocation)
    if particle == nil then
        counter_debug_log(ctx, "[CharacterCounter] impact particle spawn failed")
        return false
    end

    if Particle.SetSystemWorldLocation ~= nil then
        Particle.SetSystemWorldLocation(particle, hitLocation)
    end
    if Particle.SetSystemWorldScale ~= nil then
        Particle.SetSystemWorldScale(particle, ctx.config.COUNTER_IMPACT_PARTICLE_SCALE)
    end
    set_particle_spawn_scale(particle, 1.0)
    if particle.ResetParticles ~= nil then
        particle:ResetParticles()
    end
    if particle.Activate ~= nil then
        particle:Activate(true)
    end

    return true
end

local function deactivate_counter_impact_particle(ctx)
    local particle = ctx.cache.counterImpactParticle
    if particle == nil then
        return
    end

    set_particle_spawn_scale(particle, 0.0)
    if particle.ResetParticles ~= nil then
        particle:ResetParticles()
    end
end

local function play_counter_particles(ctx, hitLocation)
    counter_debug_log(ctx, "[CharacterCounter] play counter particles")
    if not are_counter_particles_enabled(ctx) then
        play_counter_parry_sound(ctx)
        return
    end

    if activate_counter_impact_particle(ctx, hitLocation) then
        play_counter_parry_sound(ctx)
    end
    ctx.state.counterParticleTimeRemaining = ctx.config.COUNTER_PARTICLE_DURATION
end

local function play_counter_slomo(ctx)
    local action = Context.GetActionComponent(ctx)
    if action == nil or action.Slomo == nil then
        counter_debug_log(ctx, "[Slomo] : skipped, ActionComponent or Slomo binding missing")
        return
    end

    counter_debug_log(ctx, "[Slomo] : play duration=" .. string.format("%.3f", ctx.config.COUNTER_SLOMO_DURATION)
        .. " dilation=" .. string.format("%.3f", ctx.config.COUNTER_SLOMO_TIME_DILATION))
    action:Slomo(ctx.config.COUNTER_SLOMO_DURATION, ctx.config.COUNTER_SLOMO_TIME_DILATION)
end

local function begin_counter_invincible(ctx)
    if ctx.state.counterInvincibleApplied then
        return
    end

    local health = Context.GetHealthComponent(ctx)
    if health == nil or health.SetInvincible == nil then
        return
    end

    if health.IsInvincible ~= nil then
        ctx.state.counterPreviousInvincible = health:IsInvincible() == true
    else
        ctx.state.counterPreviousInvincible = false
    end

    health:SetInvincible(true)
    ctx.state.counterInvincibleApplied = true
end

local function get_root_primitive(ctx)
    if ctx.obj == nil or ctx.obj.GetRootPrimitiveComponent == nil then
        return nil
    end

    local primitive = ctx.obj:GetRootPrimitiveComponent()
    if primitive ~= nil and primitive.IsValid ~= nil and not primitive:IsValid() then
        return nil
    end

    return primitive
end

local function begin_pawn_overlap(ctx)
    if ctx.state.counterCollision ~= nil then
        return
    end

    local primitive = get_root_primitive(ctx)
    if primitive == nil or primitive.GetPawnCollisionResponse == nil or primitive.SetPawnCollisionResponse == nil then
        return
    end

    local originalResponse = primitive:GetPawnCollisionResponse()
    ctx.state.counterCollision = {
        primitive = primitive,
        pawnResponse = originalResponse,
    }

    --Let the counter reposition pass through Pawn collision, then restore it on counter end--
    primitive:SetPawnCollisionResponse(ctx.config.COUNTER_PAWN_OVERLAP_RESPONSE or 1)
end

local function restore_pawn_overlap(ctx)
    local collision = ctx.state.counterCollision
    if collision == nil then
        return
    end

    local primitive = collision.primitive
    if primitive ~= nil and primitive.SetPawnCollisionResponse ~= nil then
        if primitive.IsValid == nil or primitive:IsValid() then
            primitive:SetPawnCollisionResponse(collision.pawnResponse)
        end
    end

    ctx.state.counterCollision = nil
end

function Counter.RestoreCollision(ctx)
    restore_pawn_overlap(ctx)
end

function Counter.Reset(ctx)
    restore_pawn_overlap(ctx)
    State.ResetCounter(ctx)
end

local function face_target(ctx, target)
    if ctx.obj == nil or ctx.obj.Location == nil or target == nil or target.Location == nil then
        return
    end

    local direction = target.Location - ctx.obj.Location
    direction = Context.NormalizeFlatDirection(direction)
    if direction == nil then
        return
    end

    ctx.obj.Rotation = Vec3(0.0, 0.0, atan2_degrees(direction.Y, direction.X))
end

local function start_reposition_behind_target(ctx, target)
    if not ctx.config.COUNTER_REPOSITION_ENABLED then
        return
    end

    if ctx.obj == nil or ctx.obj.Location == nil or target == nil or target.Location == nil then
        return
    end

    local targetForward = Context.NormalizeFlatDirection(target.Forward)
    if targetForward == nil then
        local fromTargetToPlayer = ctx.obj.Location - target.Location
        targetForward = Context.NormalizeFlatDirection(fromTargetToPlayer)
    end

    if targetForward == nil then
        return
    end

    local distance = ctx.config.COUNTER_REPOSITION_DISTANCE or 120.0
    local duration = ctx.config.COUNTER_REPOSITION_DURATION or 0.18
    local startLocation = ctx.obj.Location
    local targetLocation = target.Location - targetForward * distance
    targetLocation.Z = startLocation.Z

    --Move player behind the attacker while the counter montage starts--
    ctx.state.counterMove = {
        target = target,
        startLocation = startLocation,
        targetLocation = targetLocation,
        elapsed = 0.0,
        duration = math.max(0.01, duration),
    }
end

local function update_reposition(ctx, dt)
    local move = ctx.state.counterMove
    if move == nil then
        return
    end

    if ctx.obj == nil or ctx.obj.Location == nil then
        ctx.state.counterMove = nil
        return
    end

    move.elapsed = move.elapsed + dt
    local alpha = smooth_step(move.elapsed / move.duration)
    ctx.obj.Location = lerp_vec(move.startLocation, move.targetLocation, alpha)

    if move.target ~= nil then
        face_target(ctx, move.target)
    end

    if move.elapsed >= move.duration then
        ctx.obj.Location = move.targetLocation
        ctx.state.counterMove = nil
    end
end

function Counter.Play(ctx, target, hitLocation)
    load_counter_montage(ctx)

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or ctx.cache.counterMontage == nil then
        State.ResetGuard(ctx)
        Counter.Reset(ctx)
        Locomotion.Unlock(ctx)
        return
    end

    Context.StopCurrentMontage(ctx)
    State.ResetAttack(ctx)
    State.ResetGuard(ctx)
    ctx.state.counterPlaying = true
    ctx.state.canCancelCounter = false
    begin_counter_invincible(ctx)
    Locomotion.Lock(ctx)
    --Clear old EnableAttack flag--
    Context.ConsumeEnableAttack(ctx)
    play_counter_particles(ctx, hitLocation)
    play_counter_slomo(ctx)
    begin_pawn_overlap(ctx)
    start_reposition_behind_target(ctx, target)
    face_target(ctx, target)
    Equipment.ActivateTrail(ctx)
    Context.PlayCameraShakeAsset(ctx.config.COUNTER_CAMERA_SHAKE_PATH, ctx.config.COUNTER_CAMERA_SHAKE_SCALE)
    anim:PlayMontage(ctx.cache.counterMontage, ctx.config.COUNTER_SECTION)
end

function Counter.StopForCancel(ctx)
    if not ctx.state.counterPlaying or not ctx.state.canCancelCounter then
        return false
    end

    Context.StopCurrentMontage(ctx)
    Counter.Reset(ctx)
    Locomotion.Unlock(ctx)
    Context.ConsumeEnableAttack(ctx)
    return true
end

function Counter.UpdateOpportunity(ctx)
    if not ctx.state.defensePlaying and not ctx.state.attackPlaying then
        return
    end

    local opportunityTarget, hitLocation = Context.ConsumeCounterOpportunity(ctx)
    if opportunityTarget ~= nil then
        counter_debug_log(ctx, "[CounterInput] opportunity consumed")
        if opportunityTarget == true then
            Counter.Play(ctx, nil, hitLocation)
        else
            Counter.Play(ctx, opportunityTarget, hitLocation)
        end
    end
end

function Counter.UpdateInputWindow(ctx, dt)
    if is_counter_input_pressed(ctx) then
        ctx.state.counterInputBufferRemaining = ctx.config.COUNTER_INPUT_BUFFER_SECONDS or 0.2
        counter_debug_log(ctx, "[CounterInput] pressed")
    elseif ctx.state.counterInputBufferRemaining > 0.0 then
        ctx.state.counterInputBufferRemaining = math.max(0.0, ctx.state.counterInputBufferRemaining - (dt or 0.0))
    end

    if not ctx.state.attackPlaying or ctx.state.counterPlaying then
        ctx.state.counterInputBufferRemaining = 0.0
        return
    end

    if ctx.state.counterInputBufferRemaining <= 0.0 then
        return
    end

    if not Context.IsCounterInputWindowActive(ctx) then
        counter_debug_log(ctx, "[CounterInput] buffered, waiting for CounterInputWindow")
        return
    end

    if Context.OpenCounterInputDeflect(ctx) then
        counter_debug_log(ctx, "[CounterInput] deflect opened")
    else
        counter_debug_log(ctx, "[CounterInput] deflect failed")
    end
    ctx.state.counterInputBufferRemaining = 0.0
end

function Counter.UpdateCancelWindow(ctx)
    if not ctx.state.counterPlaying or ctx.state.canCancelCounter then
        return
    end

    if Context.ConsumeEnableAttack(ctx) then
        ctx.state.canCancelCounter = true
        Locomotion.Unlock(ctx)
    end
end

function Counter.UpdateSequence(ctx, dt)
    if not ctx.state.counterPlaying then
        return
    end

    update_reposition(ctx, dt or 0.0)

    if ctx.state.canCancelCounter and Locomotion.IsMoveKeyDown(ctx) then
        Counter.StopForCancel(ctx)
        return
    end

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or ctx.cache.counterMontage == nil or not anim:IsMontagePlaying(ctx.cache.counterMontage) then
        Counter.Reset(ctx)
        Locomotion.Unlock(ctx)
    end
end

function Counter.UpdateParticles(ctx, dt)
    if ctx.state.counterParticleTimeRemaining <= 0.0 then
        return
    end

    ctx.state.counterParticleTimeRemaining = ctx.state.counterParticleTimeRemaining - dt
    if ctx.state.counterParticleTimeRemaining > 0.0 then
        return
    end

    deactivate_counter_impact_particle(ctx)
end

function Counter.BeginPlay(ctx)
    load_counter_montage(ctx)
    load_counter_sounds(ctx)
    create_counter_impact_particle(ctx, get_owner_location(ctx))
end

function Counter.Tick(ctx, dt)
    Counter.UpdateInputWindow(ctx, dt)
    Counter.UpdateOpportunity(ctx)
    Counter.UpdateCancelWindow(ctx)
    Counter.UpdateSequence(ctx, dt)
    Counter.UpdateParticles(ctx, dt)
end

return Counter
