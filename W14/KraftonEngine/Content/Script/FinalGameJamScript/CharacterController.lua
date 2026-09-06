local Context = require("FinalGameJamScript/Character/CharacterContext")
local Equipment = require("FinalGameJamScript/Character/CharacterEquipment")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local Combat = require("FinalGameJamScript/Character/CharacterCombat")
local Guard = require("FinalGameJamScript/Character/CharacterGuard")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")
local HitReaction = require("FinalGameJamScript/Character/CharacterHitReaction")
local LockOn = require("FinalGameJamScript/Character/CharacterLockOn")
local PlayerGameState = require("FinalGameJamScript/Character/PlayerGameState")
local Execution = require("FinalGameJamScript/Character/CharacterExecution")
local Commands = require("FinalGameJamScript/Character/CharacterCommands")
local Posture = require("FinalGameJamScript/Character/CharacterPosture")
local AttackCamera = require("FinalGameJamScript/Character/CharacterAttackCamera")

local ctx = nil

function BeginPlay()
    ctx = Context.Create(obj)
--
    Context.GetAnimInstance(ctx)
    Locomotion.BeginPlay(ctx)
    Equipment.BeginPlay(ctx)
    Combat.BeginPlay(ctx)
    Guard.BeginPlay(ctx)
    Counter.BeginPlay(ctx)
    HitReaction.BeginPlay(ctx)
    LockOn.BeginPlay(ctx)
    Execution.BeginPlay(ctx)
    Posture.BeginPlay(ctx)
end

function Tick(dt)
    if ctx == nil then
        return
    end

    if PlayerGameState.Tick(ctx, dt) then
        return
    end

    Commands.Tick(ctx, dt)
end

function OnDamaged(damageSpec, damageReport)
    if ctx == nil then
        return
    end

    -- Little camera jolt on every incoming hit (guard/posture/stagger/death).
    if ctx.config.HIT_CAMERA_SHAKE_ENABLED then
        Context.PlayCameraShake(ctx.config.HIT_CAMERA_SHAKE_SCALE, ctx.config.HIT_CAMERA_SHAKE_PARAMS)
    end

    if PlayerGameState.HandleDamaged(ctx, damageSpec, damageReport) then
        return
    end

    if Posture.HandleDamaged(ctx) then
        return
    end

    HitReaction.Play(ctx, damageSpec)
end

function OnAttackHit(target, attacker, hitComponent, damageSpec, hitEventName)
    if ctx == nil then
        return
    end

    AttackCamera.OnCounterHit(ctx, target, attacker, hitComponent, damageSpec, hitEventName)
    AttackCamera.OnNormalAttackHit(ctx, target, attacker, hitComponent, damageSpec, hitEventName)
end
