local Context = require("FinalGameJamScript/Character/CharacterContext")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")
local ExecutionCamera = require("FinalGameJamScript/Character/CharacterExecutionCamera")

local Execution = {}

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

local function execution_log(ctx, message)
    if ctx ~= nil and ctx.config ~= nil and ctx.config.EXECUTION_DEBUG_LOGS then
        print("[CharacterExecution] " .. message)
    end
end

local function load_montages(ctx)
    if ctx.cache.executionPlayerMontage == nil then
        ctx.cache.executionPlayerMontage = Context.LoadMontage(ctx.config.EXECUTION_PLAYER_MONTAGE_PATH)
    end

    if ctx.cache.executionBossMontage == nil then
        ctx.cache.executionBossMontage = Context.LoadMontage(ctx.config.EXECUTION_BOSS_MONTAGE_PATH)
    end
end

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end

    if actor.IsValid ~= nil then
        return actor:IsValid()
    end

    return true
end

local function get_execution_component(actor)
    if not is_valid_actor(actor) then
        return nil
    end

    if actor.GetExecution ~= nil then
        local execution = actor:GetExecution()
        if execution ~= nil then
            return execution
        end
    end

    if actor.GetExecutionComponent ~= nil then
        return actor:GetExecutionComponent()
    end

    if actor.CallFunction ~= nil then
        local execution = actor:CallFunction("GetExecution")
        if execution ~= nil then
            return execution
        end

        return actor:CallFunction("GetExecutionComponent")
    end

    return nil
end

local function is_deathblow_ready(actor)
    local execution = get_execution_component(actor)
    if execution == nil then
        return false
    end

    return Context.Call(execution, "IsDeathblowReady") == true
end

local function find_ready_boss_by_class(ctx, className)
    if World == nil or World.FindActorsByClass == nil then
        return nil
    end

    local actors = World.FindActorsByClass(className)
    if actors == nil then
        return nil
    end

    local bestActor = nil
    local bestDistanceSq = nil
    local ownerLocation = ctx.obj.Location
    local maxDistance = ctx.config.EXECUTION_MAX_DISTANCE or 5.0
    local maxDistanceSq = maxDistance * maxDistance

    for _, actor in ipairs(actors) do
        if actor ~= ctx.obj and is_deathblow_ready(actor) and actor.Location ~= nil then
            local delta = actor.Location - ownerLocation
            local distanceSq = delta:Dot(delta)
            if distanceSq <= maxDistanceSq and (bestDistanceSq == nil or distanceSq < bestDistanceSq) then
                bestActor = actor
                bestDistanceSq = distanceSq
            end
        end
    end

    return bestActor
end

local function find_ready_boss_by_tag(ctx)
    if World == nil or World.FindActorsByTag == nil then
        return nil
    end

    local tag = ctx.config.EXECUTION_BOSS_TAG
    if tag == nil or tag == "" then
        return nil
    end

    local actors = World.FindActorsByTag(tag)
    if actors == nil then
        return nil
    end

    local bestActor = nil
    local bestDistanceSq = nil
    local ownerLocation = ctx.obj.Location
    local maxDistance = ctx.config.EXECUTION_MAX_DISTANCE or 5.0
    local maxDistanceSq = maxDistance * maxDistance

    for _, actor in ipairs(actors) do
        if actor ~= ctx.obj and is_deathblow_ready(actor) and actor.Location ~= nil then
            local delta = actor.Location - ownerLocation
            local distanceSq = delta:Dot(delta)
            if distanceSq <= maxDistanceSq and (bestDistanceSq == nil or distanceSq < bestDistanceSq) then
                bestActor = actor
                bestDistanceSq = distanceSq
            end
        end
    end

    return bestActor
end

local function find_ready_boss(ctx)
    if ctx.obj == nil or ctx.obj.Location == nil then
        return nil
    end

    for _, className in ipairs(ctx.config.EXECUTION_BOSS_CLASSES or {}) do
        local actor = find_ready_boss_by_class(ctx, className)
        if actor ~= nil then
            return actor
        end
    end

    return find_ready_boss_by_tag(ctx)
end

local function get_actor_anim_instance(actor)
    if actor == nil or actor.GetSkeletalMeshComponent == nil then
        return nil
    end

    local mesh = actor:GetSkeletalMeshComponent()
    if mesh == nil or mesh.GetAnimInstance == nil then
        return nil
    end

    return mesh:GetAnimInstance()
end

local function face_actor_to(ctx, actor, target)
    if actor == nil or target == nil or actor.Location == nil or target.Location == nil then
        return
    end

    local direction = Context.NormalizeFlatDirection(target.Location - actor.Location)
    if direction == nil then
        return
    end

    if actor == ctx.obj then
        Context.FaceDirection(ctx, direction)
        return
    end

    local yaw = atan2_degrees(direction.Y, direction.X)
    actor.Rotation = Vec3(0.0, 0.0, yaw)
end

local function align_for_execution(ctx, boss)
    if not ctx.config.EXECUTION_ALIGN_ENABLED or ctx.obj == nil or boss == nil then
        return
    end

    if boss.Location == nil then
        return
    end

    local backOffset = ctx.config.EXECUTION_ALIGN_BACK_OFFSET or 0.0
    if backOffset > 0.0 and ctx.obj.Location ~= nil then
        local awayFromBoss = Context.NormalizeFlatDirection(ctx.obj.Location - boss.Location)
        if awayFromBoss ~= nil then
            ctx.obj.Location = ctx.obj.Location + awayFromBoss * backOffset
        end
    end

    face_actor_to(ctx, ctx.obj, boss)
    face_actor_to(ctx, boss, ctx.obj)
end

local function play_boss_montage(ctx, boss)
    local montage = ctx.cache.executionBossMontage
    if boss == nil or montage == nil then
        return false
    end

    if boss.StopEnemyMovement ~= nil then
        boss:StopEnemyMovement()
    elseif boss.CallFunction ~= nil then
        boss:CallFunction("StopEnemyMovement")
    end

    if boss.PlayCombatMontage ~= nil then
        return boss:PlayCombatMontage(montage)
    end

    if boss.CallFunction ~= nil then
        return boss:CallFunction("PlayCombatMontage", montage)
    end

    local anim = get_actor_anim_instance(boss)
    if anim ~= nil and anim.PlayMontage ~= nil then
        anim:PlayMontage(montage)
        return true
    end

    return false
end

local function begin_boss_execution_priority(boss)
    Context.Call(boss, "BeginExecutionPriorityWindow")
end

local function end_boss_execution_priority(boss)
    Context.Call(boss, "EndExecutionPriorityWindow")
end

local function play_execution_rumble(ctx)
    if Input == nil or Input.PlayGamepadRumble == nil then
        return
    end

    if ctx.config.EXECUTION_GAMEPAD_RUMBLE_ENABLED == false then
        return
    end

    local left = ctx.config.EXECUTION_GAMEPAD_RUMBLE_LEFT or 0.65
    local right = ctx.config.EXECUTION_GAMEPAD_RUMBLE_RIGHT or 1.0
    local duration = ctx.config.EXECUTION_GAMEPAD_RUMBLE_DURATION or 0.22
    Input.PlayGamepadRumble(left, right, duration)
end

function Execution.Play(ctx, boss)
    load_montages(ctx)

    local anim = Context.GetAnimInstance(ctx)
    local playerMontage = ctx.cache.executionPlayerMontage
    if anim == nil or playerMontage == nil or boss == nil then
        execution_log(ctx, "failed: missing anim, player montage, or boss")
        return false
    end

    Context.StopCurrentMontage(ctx)
    State.ResetCombat(ctx)
    Counter.Reset(ctx)
    Locomotion.Lock(ctx)
    align_for_execution(ctx, boss)

    ctx.state.executionPlaying = true
    ctx.state.executionTarget = boss
    ctx.state.executionTargetMontage = ctx.cache.executionBossMontage

    ExecutionCamera.Begin(ctx, boss)
    play_execution_rumble(ctx)
    anim:PlayMontage(playerMontage, nil, ctx.config.EXECUTION_PLAY_RATE or 1.0)
    begin_boss_execution_priority(boss)
    local bossPlayed = play_boss_montage(ctx, boss)
    if not bossPlayed then
        end_boss_execution_priority(boss)
    end
    execution_log(ctx, "started bossMontage=" .. tostring(bossPlayed))
    return true
end

function Execution.CanExecuteInput(ctx)
    if ctx.state.executionPlaying then
        return false
    end

    if ctx.state.playerDead or ctx.state.playerDefeated then
        return false
    end

    return true
end

function Execution.ExecuteInput(ctx)
    if not Execution.CanExecuteInput(ctx) then
        return false
    end

    local boss = find_ready_boss(ctx)
    if boss == nil then
        execution_log(ctx, "input ignored: no ready boss")
        return false
    end

    return Execution.Play(ctx, boss)
end

function Execution.IsInputTriggered(ctx)
    if Input == nil or Input.GetKeyDown == nil then
        return false
    end
    return (ctx.config.EXECUTION_KEY ~= nil and Input.GetKeyDown(ctx.config.EXECUTION_KEY))
        or (ctx.config.GAMEPAD_EXECUTION ~= nil and Input.GetKeyDown(ctx.config.GAMEPAD_EXECUTION))
end

function Execution.UpdateSequence(ctx, dt)
    if not ctx.state.executionPlaying then
        return
    end

    ExecutionCamera.Update(ctx, ctx.state.executionTarget, dt)

    local anim = Context.GetAnimInstance(ctx)
    if anim ~= nil and ctx.cache.executionPlayerMontage ~= nil and anim:IsMontagePlaying(ctx.cache.executionPlayerMontage) then
        return
    end

    ExecutionCamera.End(ctx)
    end_boss_execution_priority(ctx.state.executionTarget)
    ctx.state.executionPlaying = false
    ctx.state.executionTarget = nil
    ctx.state.executionTargetMontage = nil
    Locomotion.Unlock(ctx)
end

function Execution.BeginPlay(ctx)
    load_montages(ctx)
end

function Execution.Tick(ctx, dt)
    Execution.UpdateSequence(ctx, dt)
end

return Execution
