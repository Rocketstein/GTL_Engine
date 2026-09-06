local DebugConfig = require("Game.Config.Debug")
local PlayerStatusConfig = require("Game.Config.PlayerStatus")
local GameManager = require("Game.GameManager")
local AudioManager = require("Game.AudioManager")
local Log = require("Common.Log")
local Math = require("Common.Math")

-- PlayerStatus는 플레이어 생존 상태를 관리
-- 외부 Lua 코드는 Stability API를 사용하고, 내부 hp/max_hp 값은 실제 저장 슬롯으로만 다룸
local PlayerStatus = {
    -- 포드 안정도 최대치(max_stability)
    max_hp = PlayerStatusConfig.max_hp,
    -- 포드 안정도 현재값(stability)
    hp = PlayerStatusConfig.max_hp,
    -- 피격/방어막 발동 직후 중복 충돌을 막는 시간
    invincible_time = PlayerStatusConfig.invincible_time,
    -- 현재 남은 무적 시간입니다.
    invincible_timer = 0.0,
    is_dead = false,
    -- 디버그용 무적 log 한번만 찍기
    invincible_end_logged = true,
}

local log = Log.MakeLogger(DebugConfig)

------------------------------------------------
-- 내부 동기화 함수들
------------------------------------------------

-- 실제 안정도 값 동기화
local function sync_stability_to_manager()
    GameManager.SetStabilitySnapshot(PlayerStatus.hp, PlayerStatus.max_hp)
end

-- invincible 동기화 함수
local function start_invincible_window()
    PlayerStatus.invincible_timer = PlayerStatus.invincible_time
    PlayerStatus.invincible_end_logged = false
end

------------------------------------------------
-- 생명주기 함수들
------------------------------------------------

-- 새 게임 시작 시 상태값 초기화
-- GameManager도 같은 안정도 값을 동기화
function PlayerStatus.ResetForStart()
    PlayerStatus.max_hp = PlayerStatusConfig.max_hp
    PlayerStatus.hp = PlayerStatus.max_hp
    PlayerStatus.invincible_time = PlayerStatusConfig.invincible_time
    PlayerStatus.invincible_timer = 0.0
    PlayerStatus.is_dead = false
    PlayerStatus.invincible_end_logged = true
    sync_stability_to_manager()
end


function PlayerStatus.Tick(dt)
    if PlayerStatus.invincible_timer <= 0.0 then
        return
    end

    PlayerStatus.invincible_timer = PlayerStatus.invincible_timer - (dt or 0.0)
    if PlayerStatus.invincible_timer <= 0.0 then
        PlayerStatus.invincible_timer = 0.0
        if not PlayerStatus.invincible_end_logged then
            log("[PlayerStatus] Invincible ended")
            PlayerStatus.invincible_end_logged = true
        end
    end
end

------------------------------------------------
-- 안정도 변경 함수들
------------------------------------------------

-- 포드 안정도 피해를 적용 단일 지점
function PlayerStatus.DamageStability(amount)
    local damage = Math.SafeNonNegative(amount, 1)

    if PlayerStatus.is_dead then
        log("[PlayerStatus] DamageStability ignored: already dead damage=" .. tostring(damage))
        return false
    end

    if GameManager.IsGameOver() then
        log("[PlayerStatus] DamageStability ignored: GameOver damage=" .. tostring(damage))
        return false
    end

    if PlayerStatus.invincible_timer > 0.0 then
        log(
            "[PlayerStatus] Damage ignored due to invincible damage=" .. tostring(damage) ..
            " remaining=" .. tostring(PlayerStatus.invincible_timer)
        )
        return false
    end

    GameManager.OnPlayerHit()

    -- shield가 있으면 충돌 1회를 안정도 피해 없이 막음
    if GameManager.ConsumeShield() then
        start_invincible_window()
        log("[PlayerStatus] Shield blocked damage=" .. tostring(damage))
        return true
    end

    PlayerStatus.hp = PlayerStatus.hp - damage
    start_invincible_window()
    AudioManager.PlayHit()
    sync_stability_to_manager()

    log(
        "[PlayerStatus] Stability damaged damage=" .. tostring(damage) ..
        " stability=" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp)
    )

    if PlayerStatus.hp <= 0 then
        PlayerStatus.hp = 0
        PlayerStatus.is_dead = true
        sync_stability_to_manager()
        log("[PlayerStatus] Stability empty")
        GameManager.GameOver("StabilityZero")
    end

    return true
end

-- 즉시 GameOver가 필요할 때 쓰는 함수 (낙사용)
function PlayerStatus.Kill(reason)
    if PlayerStatus.is_dead then
        log("[PlayerStatus] Kill ignored: already dead reason=" .. tostring(reason))
        return false
    end

    PlayerStatus.hp = 0
    PlayerStatus.is_dead = true
    sync_stability_to_manager()

    log("[PlayerStatus] Kill reason=" .. tostring(reason or "Unknown"))
    GameManager.GameOver(reason or "Kill")
    return true
end

------------------------------------------------
-- 상태 조회 함수들
------------------------------------------------

function PlayerStatus.IsDead()    
    return PlayerStatus.is_dead
end

function PlayerStatus.IsInvincible()
    return PlayerStatus.invincible_timer > 0.0
end

function PlayerStatus.GetStability()
    return PlayerStatus.hp
end

function PlayerStatus.GetMaxStability()
    return PlayerStatus.max_hp
end

function PlayerStatus.GetStabilityPercent()
    if PlayerStatus.max_hp <= 0 then
        return 0
    end
    return PlayerStatus.hp / PlayerStatus.max_hp
end

function PlayerStatus.IsStabilityEmpty()
    return PlayerStatus.hp <= 0
end

return PlayerStatus
