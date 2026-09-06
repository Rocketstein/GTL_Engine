local HUD_DOCUMENT = "Content/Game/UI/HUD.rml"

local HUD_ROOT_ID = "hud-screen"
local HUD_Z_ORDER = 0
local HUD_FORCE_HIDE_FLAG = "BossIntroHUDHidden"
local PLAYER_HP_MASK_ID = "player-hp-mask"
local PLAYER_HP_DELAY_MASK_ID = "player-hp-delay-mask"
local BOSS_HP_MASK_ID = "boss-hp-mask"
local BOSS_HP_DELAY_MASK_ID = "boss-hp-delay-mask"
local BOSS_POSTURE_LEFT_MASK_ID = "boss-posture-left-mask"
local BOSS_POSTURE_RIGHT_MASK_ID = "boss-posture-right-mask"
local BOSS_PANEL_ID = "boss-panel"
local BOSS_POSTURE_PANEL_ID = "boss-posture-panel"
local ITEM_PANEL_ID = "item-panel"
local BOSS_POSTURE_LEFT_FILL_ID = "boss-posture-left-fill"
local BOSS_POSTURE_RIGHT_FILL_ID = "boss-posture-right-fill"
local BOSS_POSTURE_LEFT_WARNING_FILL_ID = "boss-posture-left-warning-fill"
local BOSS_POSTURE_RIGHT_WARNING_FILL_ID = "boss-posture-right-warning-fill"
local BOSS_POSTURE_LEFT_CRITICAL_FILL_ID = "boss-posture-left-critical-fill"
local BOSS_POSTURE_RIGHT_CRITICAL_FILL_ID = "boss-posture-right-critical-fill"
local BOSS_POSTURE_WARNING_GLOW_ID = "boss-posture-warning-glow"
local BOSS_POSTURE_CRITICAL_GLOW_ID = "boss-posture-critical-glow"
local PLAYER_POSTURE_LEFT_MASK_ID = "player-posture-left-mask"
local PLAYER_POSTURE_RIGHT_MASK_ID = "player-posture-right-mask"
local PLAYER_POSTURE_PANEL_ID = "player-posture-panel"
local PLAYER_POSTURE_LEFT_FILL_ID = "player-posture-left-fill"
local PLAYER_POSTURE_RIGHT_FILL_ID = "player-posture-right-fill"
local PLAYER_POSTURE_LEFT_WARNING_FILL_ID = "player-posture-left-warning-fill"
local PLAYER_POSTURE_RIGHT_WARNING_FILL_ID = "player-posture-right-warning-fill"
local PLAYER_POSTURE_LEFT_CRITICAL_FILL_ID = "player-posture-left-critical-fill"
local PLAYER_POSTURE_RIGHT_CRITICAL_FILL_ID = "player-posture-right-critical-fill"
local PLAYER_POSTURE_WARNING_GLOW_ID = "player-posture-warning-glow"
local PLAYER_POSTURE_CRITICAL_GLOW_ID = "player-posture-critical-glow"

local PLAYER_TOKEN_ID = "player-icon-right"
local HUD_CONTAINER_IDS = {
    "enemy-bars",
    BOSS_PANEL_ID,
    BOSS_POSTURE_PANEL_ID,
    PLAYER_POSTURE_PANEL_ID,
    ITEM_PANEL_ID,
    "player-hud",
}

local ENEMY_BAR_MAX = 16
local ENEMY_BAR_WIDTH = 120.0
local ENEMY_BAR_HEIGHT = 19.0
local ENEMY_BAR_ID_PREFIX = "enemy-bar-"
local ENEMY_HP_MASK_ID_PREFIX = "enemy-hp-mask-"
local ENEMY_HP_DELAY_MASK_ID_PREFIX = "enemy-hp-delay-mask-"
local ENEMY_POSTURE_MASK_ID_PREFIX = "enemy-posture-mask-"
local ENEMY_BAR_ACTOR_TAG = "Enemy"
local ENEMY_BAR_HEAD_PADDING_Z = 0.15
local ENEMY_BAR_FALLBACK_HEAD_OFFSET_Z = 1.05
local ENEMY_BAR_MAX_DISTANCE = 40.0
local ENEMY_BAR_SCREEN_EDGE_PADDING = 6.0
local ENEMY_BAR_UPDATE_INTERVAL = 1.0 / 60.0
local ENEMY_ACTOR_REFRESH_INTERVAL = 0.5
local ENEMY_BAR_CHANGE_REVEAL_SECONDS = 3.0
local ENEMY_BAR_COMBAT_VISIBILITY_GRACE_SECONDS = 0.5
local ENEMY_BAR_RATIO_CHANGE_EPSILON = 0.0001
local POSTURE_VISIBILITY_EPSILON = 0.0001
local POSTURE_WARNING_THRESHOLD = 0.65
local POSTURE_CRITICAL_THRESHOLD = 0.9
local POSTURE_WARNING_PULSE_SPEED = 4.0
local POSTURE_CRITICAL_PULSE_SPEED = 8.0
local HP_DAMAGE_TRAIL_HOLD_SECONDS = 0.5
local HP_DAMAGE_TRAIL_SHRINK_PER_SECOND = 0.5
local ENEMY_COMBAT_VISIBLE_STATES = {
    Chase = true,
    Strafe = true,
    Reposition = true,
    Attack = true,
    Recover = true,
    Guard = true,
    Deflect = true,
    Hit = true,
    Staggered = true,
}
local widget = nil
local playerHealth = nil
local playerHealthBindingId = nil
local playerCombat = nil
local playerPostureBindingId = nil
local bossActor = nil
local bossHealth = nil
local bossHealthBindingId = nil
local bossCombat = nil
local bossPostureBindingId = nil
local playerPawnChangedBindingId = nil
local lastHpRatio = nil
local playerHpDelayRatio = nil
local playerHpDelayRemaining = 0.0
local lastBossHpRatio = nil
local bossHpDelayRatio = nil
local bossHpDelayRemaining = 0.0
local lastBossPostureRatio = nil
local lastBossHudVisible = nil
local lastItemHudVisible = false
local lastPlayerPostureRatio = nil
local playerPostureVisualState = "hidden"
local bossPostureVisualState = "hidden"
local lastTokenVisible = nil
local enemyBarStates = {}
local enemySlotById = {}
local slotEnemyIdByIndex = {}
local enemyVisibilityById = {}
local lockOnEnemyId = nil
local enemyBarUpdateElapsed = 0.0
local enemyActorRefreshElapsed = ENEMY_ACTOR_REFRESH_INTERVAL
local cachedEnemyActors = {}
local cachedEnemyIds = {}
local hudTimeSeconds = 0.0
local hudVisible = true
local refresh_hud_document_state = nil

local function clamp01(value)
    value = tonumber(value) or 0.0

    if value < 0.0 then
        return 0.0
    end

    if value > 1.0 then
        return 1.0
    end

    return value
end

local function posture_fill_ratio(postureRatio)
    return clamp01(postureRatio)
end

local function set_element_visible(element_id, visible)
    if widget == nil then
        return
    end

    if visible then
        widget:SetProperty(element_id, "display", "block")
    else
        widget:SetProperty(element_id, "display", "none")
    end
end

local function apply_hud_visibility()
    if widget == nil then
        return
    end

    if not hudVisible then
        widget:SetProperty(HUD_ROOT_ID, "display", "none")

        for _, elementId in ipairs(HUD_CONTAINER_IDS) do
            set_element_visible(elementId, false)
        end

        return
    end

    widget:SetProperty(HUD_ROOT_ID, "display", "block")
    set_element_visible("enemy-bars", true)
    set_element_visible(ITEM_PANEL_ID, lastItemHudVisible == true)
    set_element_visible("player-hud", true)
    set_element_visible(PLAYER_POSTURE_PANEL_ID, true)
    set_element_visible(BOSS_PANEL_ID, lastBossHudVisible == true)
    set_element_visible(BOSS_POSTURE_PANEL_ID, lastBossHudVisible == true)
end

local function is_hud_force_hidden()
    return _G ~= nil and _G[HUD_FORCE_HIDE_FLAG] == true
end

local function set_width_ratio(element_id, ratio)
    local width = string.format("%.1f%%", clamp01(ratio) * 100.0)
    widget:SetProperty(element_id, "width", width)
end

local function set_element_opacity(elementId, opacity)
    widget:SetProperty(elementId, "opacity", string.format("%.3f", clamp01(opacity)))
end

local function set_player_posture_visual_state(ratio)
    local state = "normal"

    if ratio <= POSTURE_VISIBILITY_EPSILON then
        state = "hidden"
    elseif ratio >= POSTURE_CRITICAL_THRESHOLD then
        state = "critical"
    elseif ratio >= POSTURE_WARNING_THRESHOLD then
        state = "warning"
    end

    if playerPostureVisualState == state then
        return
    end

    playerPostureVisualState = state

    local normalOpacity = state == "normal" and 1.0 or 0.0
    local warningOpacity = state == "warning" and 1.0 or 0.0
    local criticalOpacity = state == "critical" and 1.0 or 0.0

    set_element_opacity(PLAYER_POSTURE_LEFT_FILL_ID, normalOpacity)
    set_element_opacity(PLAYER_POSTURE_RIGHT_FILL_ID, normalOpacity)
    set_element_opacity(PLAYER_POSTURE_LEFT_WARNING_FILL_ID, warningOpacity)
    set_element_opacity(PLAYER_POSTURE_RIGHT_WARNING_FILL_ID, warningOpacity)
    set_element_opacity(PLAYER_POSTURE_LEFT_CRITICAL_FILL_ID, criticalOpacity)
    set_element_opacity(PLAYER_POSTURE_RIGHT_CRITICAL_FILL_ID, criticalOpacity)
    set_element_opacity(PLAYER_POSTURE_WARNING_GLOW_ID, state == "warning" and 0.45 or 0.0)
    set_element_opacity(PLAYER_POSTURE_CRITICAL_GLOW_ID, state == "critical" and 0.65 or 0.0)
end

local function set_boss_posture_visual_state(ratio)
    local state = "normal"

    if ratio <= POSTURE_VISIBILITY_EPSILON then
        state = "hidden"
    elseif ratio >= POSTURE_CRITICAL_THRESHOLD then
        state = "critical"
    elseif ratio >= POSTURE_WARNING_THRESHOLD then
        state = "warning"
    end

    if bossPostureVisualState == state then
        return
    end

    bossPostureVisualState = state

    local normalOpacity = state == "normal" and 1.0 or 0.0
    local warningOpacity = state == "warning" and 1.0 or 0.0
    local criticalOpacity = state == "critical" and 1.0 or 0.0

    set_element_opacity(BOSS_POSTURE_LEFT_FILL_ID, normalOpacity)
    set_element_opacity(BOSS_POSTURE_RIGHT_FILL_ID, normalOpacity)
    set_element_opacity(BOSS_POSTURE_LEFT_WARNING_FILL_ID, warningOpacity)
    set_element_opacity(BOSS_POSTURE_RIGHT_WARNING_FILL_ID, warningOpacity)
    set_element_opacity(BOSS_POSTURE_LEFT_CRITICAL_FILL_ID, criticalOpacity)
    set_element_opacity(BOSS_POSTURE_RIGHT_CRITICAL_FILL_ID, criticalOpacity)
    set_element_opacity(BOSS_POSTURE_WARNING_GLOW_ID, state == "warning" and 0.45 or 0.0)
    set_element_opacity(BOSS_POSTURE_CRITICAL_GLOW_ID, state == "critical" and 0.65 or 0.0)
end

local function update_player_posture_effect()
    if widget == nil then
        return
    end

    if playerPostureVisualState == "warning" then
        local pulse = 0.5 + 0.5 * math.sin(hudTimeSeconds * POSTURE_WARNING_PULSE_SPEED)
        local fillOpacity = 0.78 + 0.22 * pulse
        set_element_opacity(PLAYER_POSTURE_LEFT_WARNING_FILL_ID, fillOpacity)
        set_element_opacity(PLAYER_POSTURE_RIGHT_WARNING_FILL_ID, fillOpacity)
        set_element_opacity(PLAYER_POSTURE_WARNING_GLOW_ID, 0.28 + 0.32 * pulse)
    elseif playerPostureVisualState == "critical" then
        local pulse = 0.5 + 0.5 * math.sin(hudTimeSeconds * POSTURE_CRITICAL_PULSE_SPEED)
        local fillOpacity = 0.62 + 0.38 * pulse
        set_element_opacity(PLAYER_POSTURE_LEFT_CRITICAL_FILL_ID, fillOpacity)
        set_element_opacity(PLAYER_POSTURE_RIGHT_CRITICAL_FILL_ID, fillOpacity)
        set_element_opacity(PLAYER_POSTURE_CRITICAL_GLOW_ID, 0.45 + 0.5 * pulse)
    end
end

local function update_boss_posture_effect()
    if widget == nil then
        return
    end

    if bossPostureVisualState == "warning" then
        local pulse = 0.5 + 0.5 * math.sin(hudTimeSeconds * POSTURE_WARNING_PULSE_SPEED)
        local fillOpacity = 0.78 + 0.22 * pulse
        set_element_opacity(BOSS_POSTURE_LEFT_WARNING_FILL_ID, fillOpacity)
        set_element_opacity(BOSS_POSTURE_RIGHT_WARNING_FILL_ID, fillOpacity)
        set_element_opacity(BOSS_POSTURE_WARNING_GLOW_ID, 0.28 + 0.32 * pulse)
    elseif bossPostureVisualState == "critical" then
        local pulse = 0.5 + 0.5 * math.sin(hudTimeSeconds * POSTURE_CRITICAL_PULSE_SPEED)
        local fillOpacity = 0.62 + 0.38 * pulse
        set_element_opacity(BOSS_POSTURE_LEFT_CRITICAL_FILL_ID, fillOpacity)
        set_element_opacity(BOSS_POSTURE_RIGHT_CRITICAL_FILL_ID, fillOpacity)
        set_element_opacity(BOSS_POSTURE_CRITICAL_GLOW_ID, 0.45 + 0.5 * pulse)
    end
end

local function format_px(value)
    return string.format("%.1fpx", tonumber(value) or 0.0)
end

local function same_number(lhs, rhs)
    return lhs ~= nil and math.abs(lhs - rhs) <= 0.0001
end

local function update_delayed_hp_ratio(elementId, delayRatio, delayRemaining, targetRatio, dt)
    if delayRatio == nil or targetRatio == nil then
        return delayRatio, delayRemaining
    end

    if delayRemaining > 0.0 then
        return delayRatio, math.max(0.0, delayRemaining - dt)
    end

    if delayRatio <= targetRatio + 0.0001 then
        return targetRatio, 0.0
    end

    delayRatio = math.max(
        targetRatio,
        delayRatio - HP_DAMAGE_TRAIL_SHRINK_PER_SECOND * dt
    )
    set_width_ratio(elementId, delayRatio)
    return delayRatio, 0.0
end

local function update_hp_damage_trails(dt)
    if widget == nil then
        return
    end

    playerHpDelayRatio, playerHpDelayRemaining = update_delayed_hp_ratio(
        PLAYER_HP_DELAY_MASK_ID,
        playerHpDelayRatio,
        playerHpDelayRemaining,
        lastHpRatio,
        dt
    )

    bossHpDelayRatio, bossHpDelayRemaining = update_delayed_hp_ratio(
        BOSS_HP_DELAY_MASK_ID,
        bossHpDelayRatio,
        bossHpDelayRemaining,
        lastBossHpRatio,
        dt
    )

    for index = 0, ENEMY_BAR_MAX - 1 do
        local state = enemyBarStates[index]

        if state ~= nil then
            state.delayRatio, state.delayRemaining = update_delayed_hp_ratio(
                ENEMY_HP_DELAY_MASK_ID_PREFIX .. index,
                state.delayRatio,
                state.delayRemaining,
                state.hpRatio,
                dt
            )
        end
    end
end

local function normalize_enemy_bar_index(index)
    index = tonumber(index)

    if index == nil then
        return nil
    end

    index = math.floor(index)

    if index < 0 or index >= ENEMY_BAR_MAX then
        return nil
    end

    return index
end

local function get_enemy_bar_state(index)
    local state = enemyBarStates[index]

    if state == nil then
        state = {}
        enemyBarStates[index] = state
    end

    return state
end

local function safe_call(target, function_name, ...)
    if target == nil then
        return nil
    end

    local fn = target[function_name]

    if fn == nil then
        return nil
    end

    local ok, result = pcall(fn, target, ...)

    if not ok then
        return nil
    end

    return result
end

local is_valid_object

local function unbind_player_health()
    if playerHealthBindingId ~= nil and is_valid_object(playerHealth) then
        safe_call(playerHealth, "UnbindOnHealthChanged", playerHealthBindingId)
    end

    playerHealth = nil
    playerHealthBindingId = nil
end

local function unbind_player_posture()
    if playerPostureBindingId ~= nil and is_valid_object(playerCombat) then
        safe_call(playerCombat, "UnbindOnPostureChanged", playerPostureBindingId)
    end

    playerCombat = nil
    playerPostureBindingId = nil
end

local function unbind_boss_health()
    if bossHealthBindingId ~= nil and is_valid_object(bossHealth) then
        safe_call(bossHealth, "UnbindOnHealthChanged", bossHealthBindingId)
    end

    bossHealth = nil
    bossHealthBindingId = nil
end

local function unbind_boss_posture()
    if bossPostureBindingId ~= nil and is_valid_object(bossCombat) then
        safe_call(bossCombat, "UnbindOnPostureChanged", bossPostureBindingId)
    end

    bossCombat = nil
    bossPostureBindingId = nil
end

local function unbind_boss_actor()
    unbind_boss_health()
    unbind_boss_posture()
    bossActor = nil
end

local function bind_player_posture_from_pawn(pawn)
    unbind_player_posture()

    if not is_valid_object(pawn) then
        return false
    end

    local combat = safe_call(pawn, "GetCombatStateComponent")

    if not is_valid_object(combat) then
        return false
    end

    playerCombat = combat
    playerPostureBindingId = safe_call(
        combat,
        "BindOnPostureChanged",
        function(_component, _previousPoise, currentPoise, maxPoise)
            maxPoise = tonumber(maxPoise) or 0.0

            if maxPoise <= 0.0 then
                SetPlayerPostureRatio(0.0)
                return
            end

            SetPlayerPostureRatio((maxPoise - (tonumber(currentPoise) or maxPoise)) / maxPoise)
        end
    )

    if playerPostureBindingId == nil or tonumber(playerPostureBindingId) == 0 then
        playerCombat = nil
        playerPostureBindingId = nil
        return false
    end

    SetPlayerPostureRatio(safe_call(combat, "GetPostureRatio") or 0.0)
    return true
end

local function bind_player_health_from_pawn(pawn)
    unbind_player_health()

    if not is_valid_object(pawn) then
        return false
    end

    local health = safe_call(pawn, "GetHealthComponent")

    if not is_valid_object(health) then
        return false
    end

    playerHealth = health
    playerHealthBindingId = safe_call(
        health,
        "BindOnHealthChanged",
        function(_component, _previousHealth, currentHealth, maxHealth)
            maxHealth = tonumber(maxHealth) or 0.0

            if maxHealth <= 0.0 then
                SetPlayerHpRatio(0.0)
                return
            end

            SetPlayerHpRatio((tonumber(currentHealth) or 0.0) / maxHealth)
        end
    )

    if playerHealthBindingId == nil or tonumber(playerHealthBindingId) == 0 then
        playerHealth = nil
        playerHealthBindingId = nil
        return false
    end

    SetPlayerHpRatio(safe_call(health, "GetHealthRatio") or 0.0)
    return true
end

local function bind_boss_health_from_actor(actor)
    unbind_boss_health()

    if not is_valid_object(actor) then
        return false
    end

    local health = safe_call(actor, "GetHealthComponent")

    if not is_valid_object(health) then
        return false
    end

    bossHealth = health
    bossHealthBindingId = safe_call(
        health,
        "BindOnHealthChanged",
        function(_component, _previousHealth, currentHealth, maxHealth)
            maxHealth = tonumber(maxHealth) or 0.0

            if maxHealth <= 0.0 then
                SetBossHpRatio(0.0)
                SetBossHUDVisible(false)
                return
            end

            local ratio = (tonumber(currentHealth) or 0.0) / maxHealth
            SetBossHpRatio(ratio)

            if ratio <= 0.0 then
                SetBossHUDVisible(false)
            end
        end
    )

    if bossHealthBindingId == nil or tonumber(bossHealthBindingId) == 0 then
        bossHealth = nil
        bossHealthBindingId = nil
        return false
    end

    SetBossHpRatio(safe_call(health, "GetHealthRatio") or 0.0)
    return true
end

local function bind_boss_posture_from_actor(actor)
    unbind_boss_posture()

    if not is_valid_object(actor) then
        return false
    end

    local combat = safe_call(actor, "GetCombatStateComponent")

    if not is_valid_object(combat) then
        return false
    end

    bossCombat = combat
    bossPostureBindingId = safe_call(
        combat,
        "BindOnPostureChanged",
        function(_component, _previousPoise, currentPoise, maxPoise)
            maxPoise = tonumber(maxPoise) or 0.0

            if maxPoise <= 0.0 then
                SetBossPostureRatio(0.0)
                return
            end

            SetBossPostureRatio((maxPoise - (tonumber(currentPoise) or maxPoise)) / maxPoise)
        end
    )

    if bossPostureBindingId == nil or tonumber(bossPostureBindingId) == 0 then
        bossCombat = nil
        bossPostureBindingId = nil
        return false
    end

    SetBossPostureRatio(safe_call(combat, "GetPostureRatio") or 0.0)
    return true
end

local function bind_player_pawn_events()
    if World == nil or World.BindOnPlayerPawnChanged == nil then
        return false
    end

    playerPawnChangedBindingId = World.BindOnPlayerPawnChanged(
        function(_controller, _oldPawn, newPawn)
            bind_player_health_from_pawn(newPawn)
            bind_player_posture_from_pawn(newPawn)
        end
    )

    if playerPawnChangedBindingId == nil or tonumber(playerPawnChangedBindingId) == 0 then
        playerPawnChangedBindingId = nil
        return false
    end

    local controller = World.GetFirstPlayerController and World.GetFirstPlayerController() or nil
    local pawn = safe_call(controller, "GetPossessedPawn")
    bind_player_health_from_pawn(pawn)
    bind_player_posture_from_pawn(pawn)
    return true
end

local function unbind_player_pawn_events()
    if playerPawnChangedBindingId ~= nil
        and World ~= nil
        and World.UnbindOnPlayerPawnChanged ~= nil then
        World.UnbindOnPlayerPawnChanged(playerPawnChangedBindingId)
    end

    playerPawnChangedBindingId = nil
end

is_valid_object = function(target)
    if target == nil then
        return false
    end

    local valid = safe_call(target, "IsValid")

    if valid == nil then
        return true
    end

    return valid == true
end

local function get_actor_id(actor)
    local id = tonumber(actor.UUID)

    if id ~= nil then
        return id
    end

    return tonumber(safe_call(actor, "GetUUID"))
end

local function get_actor_location(actor)
    local ok, location = pcall(function()
        return actor.Location
    end)

    if ok then
        return location
    end

    return nil
end

local function get_enemy_anchor(enemy)
    local location = get_actor_location(enemy)

    if location == nil then
        return nil
    end

    local offsetZ = ENEMY_BAR_FALLBACK_HEAD_OFFSET_Z
    local capsule = safe_call(enemy, "GetCapsuleComponent")

    if is_valid_object(capsule) then
        local halfHeight = tonumber(safe_call(capsule, "GetScaledCapsuleHalfHeight"))

        if halfHeight ~= nil and halfHeight > 0.0 then
            offsetZ = halfHeight + ENEMY_BAR_HEAD_PADDING_Z
        end
    end

    return Vec3(location.X, location.Y, location.Z + offsetZ)
end

local function is_boss_enemy(enemy)
    return is_valid_object(safe_call(enemy, "GetEncounterComponent"))
end

local function cache_enemy_actor(enemy)
    if not is_valid_object(enemy) then
        return
    end

    local enemyId = get_actor_id(enemy)

    if enemyId == nil or cachedEnemyIds[enemyId] == true then
        return
    end

    cachedEnemyIds[enemyId] = true
    cachedEnemyActors[#cachedEnemyActors + 1] = enemy
end

local function refresh_enemy_actor_cache()
    if enemyActorRefreshElapsed < ENEMY_ACTOR_REFRESH_INTERVAL then
        return cachedEnemyActors
    end

    enemyActorRefreshElapsed = 0.0
    cachedEnemyActors = {}
    cachedEnemyIds = {}

    if World == nil or World.FindActorsByTag == nil then
        return cachedEnemyActors
    end

    local enemies = World.FindActorsByTag(ENEMY_BAR_ACTOR_TAG)

    if enemies ~= nil then
        for _, enemy in ipairs(enemies) do
            cache_enemy_actor(enemy)
        end
    end

    return cachedEnemyActors
end

local function get_enemy_visibility_state(enemyId)
    local state = enemyVisibilityById[enemyId]

    if state == nil then
        state = {}
        enemyVisibilityById[enemyId] = state
    end

    return state
end

local function resolve_enemy_visibility_id(enemyOrId)
    local id = tonumber(enemyOrId)

    if id ~= nil then
        return id
    end

    local valueType = type(enemyOrId)
    if (valueType == "userdata" or valueType == "table") and is_valid_object(enemyOrId) then
        return get_actor_id(enemyOrId)
    end

    return nil
end

local function is_enemy_in_combat_visibility_state(enemy)
    local stateMachine = safe_call(enemy, "GetStateMachine")

    if is_valid_object(stateMachine) then
        local stateName = safe_call(stateMachine, "GetState")

        if ENEMY_COMBAT_VISIBLE_STATES[stateName] == true then
            return true
        end
    end

    local brain = safe_call(enemy, "GetEnemyAIBrainComponent")

    if not is_valid_object(brain) then
        brain = safe_call(enemy, "GetAIBrainComponent")
    end

    if is_valid_object(brain) and safe_call(brain, "HasValidTarget") == true then
        return true
    end

    local combat = safe_call(enemy, "GetCombatStateComponent")

    if is_valid_object(combat) and safe_call(combat, "IsStaggered") == true then
        return true
    end

    if is_valid_object(combat) and safe_call(combat, "IsAttacking") == true then
        return true
    end

    return false
end

local function update_enemy_visibility_ratios(enemyId, hpRatio, postureRatio)
    local state = get_enemy_visibility_state(enemyId)
    hpRatio = clamp01(hpRatio)
    postureRatio = clamp01(postureRatio)

    if state.hasRatioSample == true then
        local hpChanged = math.abs((state.lastHpRatio or hpRatio) - hpRatio) > ENEMY_BAR_RATIO_CHANGE_EPSILON
        local postureChanged = math.abs((state.lastPostureRatio or postureRatio) - postureRatio) > ENEMY_BAR_RATIO_CHANGE_EPSILON

        if hpChanged or postureChanged then
            state.revealUntil = math.max(state.revealUntil or 0.0, hudTimeSeconds + ENEMY_BAR_CHANGE_REVEAL_SECONDS)
        end
    end

    if postureRatio > ENEMY_BAR_RATIO_CHANGE_EPSILON then
        state.revealUntil = math.max(state.revealUntil or 0.0, hudTimeSeconds + ENEMY_BAR_CHANGE_REVEAL_SECONDS)
    end

    state.lastHpRatio = hpRatio
    state.lastPostureRatio = postureRatio
    state.hasRatioSample = true
end

local function is_enemy_visibility_requested(enemy, enemyId)
    local state = enemyVisibilityById[enemyId]

    if state ~= nil then
        if state.manualVisible == true then
            return true
        end

        if state.manualUntil ~= nil and state.manualUntil > hudTimeSeconds then
            return true
        end

        if state.revealUntil ~= nil and state.revealUntil > hudTimeSeconds then
            return true
        end
    end

    if is_enemy_in_combat_visibility_state(enemy) then
        state = state or get_enemy_visibility_state(enemyId)
        state.combatVisibleUntil = hudTimeSeconds + ENEMY_BAR_COMBAT_VISIBILITY_GRACE_SECONDS
        return true
    end

    return state ~= nil
        and state.combatVisibleUntil ~= nil
        and state.combatVisibleUntil > hudTimeSeconds
end

local function is_enemy_bar_too_far(screen)
    if ENEMY_BAR_MAX_DISTANCE <= 0.0 then
        return false
    end

    local distance = tonumber(screen.Distance)
    return distance ~= nil and distance > ENEMY_BAR_MAX_DISTANCE
end

local function is_enemy_bar_inside_safe_area(screen)
    local width = tonumber(screen.ViewportWidth)
    local height = tonumber(screen.ViewportHeight)

    if (width == nil or height == nil or width <= 1.0 or height <= 1.0)
        and Engine ~= nil and Engine.GetViewportSize ~= nil then
        local viewport = Engine.GetViewportSize()
        width = viewport and tonumber(viewport.Width) or width
        height = viewport and tonumber(viewport.Height) or height
    end

    if width == nil or height == nil or width <= 1.0 or height <= 1.0 then
        return true
    end

    local x = tonumber(screen.X) or 0.0
    local y = tonumber(screen.Y) or 0.0
    local minX = ENEMY_BAR_WIDTH * 0.5 + ENEMY_BAR_SCREEN_EDGE_PADDING
    local maxX = width - minX
    local minY = ENEMY_BAR_HEIGHT * 0.5 + ENEMY_BAR_SCREEN_EDGE_PADDING
    local maxY = height - minY

    return x >= minX and x <= maxX and y >= minY and y <= maxY
end

local function release_enemy_slot(slot)
    local enemyId = slotEnemyIdByIndex[slot]

    if enemyId ~= nil then
        enemySlotById[enemyId] = nil
    end

    slotEnemyIdByIndex[slot] = nil
    SetEnemyHealthBarVisible(slot, false)
    enemyBarStates[slot] = nil
end

local function clear_enemy_slot_mapping()
    enemySlotById = {}
    slotEnemyIdByIndex = {}
end

local function clear_enemy_visibility_state()
    enemyVisibilityById = {}
    lockOnEnemyId = nil
end

local function find_free_enemy_slot()
    for i = 0, ENEMY_BAR_MAX - 1 do
        if slotEnemyIdByIndex[i] == nil then
            return i
        end
    end

    return nil
end

local function acquire_enemy_slot(enemyId)
    local existing = enemySlotById[enemyId]

    if existing ~= nil then
        return existing
    end

    local slot = find_free_enemy_slot()

    if slot == nil then
        return nil
    end

    enemySlotById[enemyId] = slot
    slotEnemyIdByIndex[slot] = enemyId
    return slot
end

local function collect_enemy_bar_candidates()
    local candidates = {}

    if World == nil or World.FindActorsByTag == nil or Engine == nil or Engine.ProjectWorldToScreen == nil then
        return candidates
    end

    local enemies = refresh_enemy_actor_cache()

    local viewport = Engine.GetViewportSize and Engine.GetViewportSize() or nil
    local centerX = viewport and (tonumber(viewport.Width) or 0.0) * 0.5 or 0.0
    local centerY = viewport and (tonumber(viewport.Height) or 0.0) * 0.5 or 0.0

    for _, enemy in ipairs(enemies) do
        if is_valid_object(enemy) and not is_boss_enemy(enemy) then
            local enemyId = get_actor_id(enemy)
            local health = safe_call(enemy, "GetHealthComponent")

            if enemyId ~= nil and is_valid_object(health) and safe_call(health, "IsDead") ~= true then
                local combat = safe_call(enemy, "GetCombatStateComponent")
                local hpRatio = safe_call(health, "GetHealthRatio") or 1.0
                local postureRatio = 0.0

                if is_valid_object(combat) then
                    postureRatio = safe_call(combat, "GetPostureRatio") or 0.0
                end

                update_enemy_visibility_ratios(enemyId, hpRatio, postureRatio)

                if is_enemy_visibility_requested(enemy, enemyId) then
                    local anchor = get_enemy_anchor(enemy)
                    local screen = anchor and Engine.ProjectWorldToScreen(anchor) or nil

                    if screen ~= nil
                        and screen.Visible == true
                        and not is_enemy_bar_too_far(screen)
                        and is_enemy_bar_inside_safe_area(screen) then
                        local dx = (tonumber(screen.X) or 0.0) - centerX
                        local dy = (tonumber(screen.Y) or 0.0) - centerY

                        candidates[#candidates + 1] = {
                            id = enemyId,
                            x = tonumber(screen.X) or 0.0,
                            y = tonumber(screen.Y) or 0.0,
                            hp = hpRatio,
                            posture = postureRatio,
                            score = dx * dx + dy * dy,
                            hasSlot = enemySlotById[enemyId] ~= nil,
                        }
                    end
                end
            end
        end
    end

    table.sort(candidates, function(lhs, rhs)
        if lhs.hasSlot ~= rhs.hasSlot then
            return lhs.hasSlot
        end

        return lhs.score < rhs.score
    end)

    return candidates
end

local function update_enemy_health_bars()
    if widget == nil then
        return
    end

    local candidates = collect_enemy_bar_candidates()
    local selectedIds = {}
    local selectedCount = math.min(#candidates, ENEMY_BAR_MAX)

    for i = 1, selectedCount do
        selectedIds[candidates[i].id] = true
    end

    local releaseSlots = {}

    for enemyId, slot in pairs(enemySlotById) do
        if selectedIds[enemyId] ~= true then
            releaseSlots[#releaseSlots + 1] = slot
        end
    end

    for _, slot in ipairs(releaseSlots) do
        release_enemy_slot(slot)
    end

    for i = 1, selectedCount do
        local candidate = candidates[i]
        local slot = acquire_enemy_slot(candidate.id)

        if slot ~= nil then
            SetEnemyHealthBar(slot, candidate.x, candidate.y, candidate.hp, candidate.posture, true)
        end
    end
end

function SetPlayerHpRatio(ratio)
    if widget == nil then
        return
    end

    ratio = clamp01(ratio)

    if lastHpRatio ~= nil and math.abs(lastHpRatio - ratio) <= 0.0001 then
        return
    end

    if lastHpRatio == nil then
        playerHpDelayRatio = ratio
        playerHpDelayRemaining = 0.0
        set_width_ratio(PLAYER_HP_DELAY_MASK_ID, ratio)
    elseif ratio < lastHpRatio then
        playerHpDelayRatio = math.max(playerHpDelayRatio or lastHpRatio, lastHpRatio)
        playerHpDelayRemaining = HP_DAMAGE_TRAIL_HOLD_SECONDS
        set_width_ratio(PLAYER_HP_DELAY_MASK_ID, playerHpDelayRatio)
    else
        playerHpDelayRatio = ratio
        playerHpDelayRemaining = 0.0
        set_width_ratio(PLAYER_HP_DELAY_MASK_ID, ratio)
    end

    set_width_ratio(PLAYER_HP_MASK_ID, ratio)

    lastHpRatio = ratio
end

function SetBossHpRatio(ratio)
    if widget == nil then
        return
    end

    ratio = clamp01(ratio)

    if lastBossHpRatio ~= nil and math.abs(lastBossHpRatio - ratio) <= 0.0001 then
        return
    end

    if lastBossHpRatio == nil then
        bossHpDelayRatio = ratio
        bossHpDelayRemaining = 0.0
        set_width_ratio(BOSS_HP_DELAY_MASK_ID, ratio)
    elseif ratio < lastBossHpRatio then
        bossHpDelayRatio = math.max(bossHpDelayRatio or lastBossHpRatio, lastBossHpRatio)
        bossHpDelayRemaining = HP_DAMAGE_TRAIL_HOLD_SECONDS
        set_width_ratio(BOSS_HP_DELAY_MASK_ID, bossHpDelayRatio)
    else
        bossHpDelayRatio = ratio
        bossHpDelayRemaining = 0.0
        set_width_ratio(BOSS_HP_DELAY_MASK_ID, ratio)
    end

    set_width_ratio(BOSS_HP_MASK_ID, ratio)

    lastBossHpRatio = ratio
end

function SetPlayerPostureRatio(ratio)
    if widget == nil then
        return
    end

    ratio = posture_fill_ratio(ratio)

    if lastPlayerPostureRatio ~= nil and math.abs(lastPlayerPostureRatio - ratio) <= 0.0001 then
        return
    end

    set_width_ratio(PLAYER_POSTURE_LEFT_MASK_ID, ratio)
    set_width_ratio(PLAYER_POSTURE_RIGHT_MASK_ID, ratio)
    set_player_posture_visual_state(ratio)
    widget:SetProperty(
        PLAYER_POSTURE_PANEL_ID,
        "opacity",
        ratio > POSTURE_VISIBILITY_EPSILON and "1" or "0"
    )

    lastPlayerPostureRatio = ratio
end

function SetBossPostureRatio(ratio)
    if widget == nil then
        return
    end

    ratio = posture_fill_ratio(ratio)

    if lastBossPostureRatio ~= nil and math.abs(lastBossPostureRatio - ratio) <= 0.0001 then
        return
    end

    set_width_ratio(BOSS_POSTURE_LEFT_MASK_ID, ratio)
    set_width_ratio(BOSS_POSTURE_RIGHT_MASK_ID, ratio)
    set_boss_posture_visual_state(ratio)

    lastBossPostureRatio = ratio
end

function SetBossHUDVisible(visible)
    visible = visible == true

    lastBossHudVisible = visible
    set_element_visible(BOSS_PANEL_ID, visible and hudVisible)
    set_element_visible(BOSS_POSTURE_PANEL_ID, visible and hudVisible)
end

function SetBossVisibility(visible)
    SetBossHUDVisible(visible)
end

function SetItemHUDVisible(visible)
    visible = visible == true

    lastItemHudVisible = visible
    set_element_visible(ITEM_PANEL_ID, visible and hudVisible)
end

function SetItemVisibility(visible)
    SetItemHUDVisible(visible)
end

function SetHUDVisible(visible)
    local requestedVisible = visible == true

    if requestedVisible and is_hud_force_hidden() then
        requestedVisible = false
    end

    hudVisible = requestedVisible

    if widget == nil then
        return
    end

    if hudVisible then
        if widget.IsInViewport == nil or not widget:IsInViewport() then
            widget:AddToViewportZ(HUD_Z_ORDER)
        end

        apply_hud_visibility()
        if refresh_hud_document_state ~= nil then
            refresh_hud_document_state()
        end
        return
    end

    apply_hud_visibility()

    if widget.RemoveFromParent ~= nil and (widget.IsInViewport == nil or widget:IsInViewport()) then
        widget:RemoveFromParent()
    end
end

function SetHUDVisibility(visible)
    SetHUDVisible(visible)
end

function IsHUDVisible()
    return hudVisible == true and not is_hud_force_hidden()
end

function SetPlayerTokenVisible(visible)
    visible = visible == true

    if lastTokenVisible ~= nil and lastTokenVisible == visible then
        return
    end

    set_element_visible(PLAYER_TOKEN_ID, visible)

    lastTokenVisible = visible
end

function SetPlayerHUD(hpRatio, tokenVisible, postureRatio)
    SetPlayerHpRatio(hpRatio)
    SetPlayerTokenVisible(tokenVisible)

    if postureRatio ~= nil then
        SetPlayerPostureRatio(postureRatio)
    end
end

function SetBossHUD(hpRatio, postureRatio)
    if postureRatio == nil then
        SetBossPostureRatio(hpRatio)
        return
    end

    SetBossHpRatio(hpRatio)
    SetBossPostureRatio(postureRatio)
end

function BindBossActor(actor, visible)
    unbind_boss_actor()

    if not is_valid_object(actor) then
        SetBossHUDVisible(false)
        return false
    end

    bossActor = actor

    local boundHealth = bind_boss_health_from_actor(actor)
    local boundPosture = bind_boss_posture_from_actor(actor)

    SetBossHUDVisible(visible ~= false)
    return boundHealth or boundPosture
end

function ClearBossActor()
    unbind_boss_actor()
    SetBossHUDVisible(false)
end

function SetEnemyHealthBarVisible(index, visible)
    if widget == nil then
        return
    end

    index = normalize_enemy_bar_index(index)

    if index == nil then
        return
    end

    visible = visible == true

    local state = get_enemy_bar_state(index)

    if state.visible ~= nil and state.visible == visible then
        return
    end

    set_element_visible(ENEMY_BAR_ID_PREFIX .. index, visible)
    state.visible = visible
end

function SetEnemyHealthBar(index, x, y, hpRatio, postureRatio, visible)
    if widget == nil then
        return
    end

    index = normalize_enemy_bar_index(index)

    if index == nil then
        return
    end

    if visible == nil and type(postureRatio) == "boolean" then
        visible = postureRatio
        postureRatio = hpRatio
    end

    visible = visible == true

    if not visible then
        SetEnemyHealthBarVisible(index, false)
        return
    end

    x = tonumber(x) or 0.0
    y = tonumber(y) or 0.0
    hpRatio = clamp01(hpRatio)
    postureRatio = posture_fill_ratio(postureRatio)

    local left = x - ENEMY_BAR_WIDTH * 0.5
    local top = y - ENEMY_BAR_HEIGHT * 0.5
    local state = get_enemy_bar_state(index)
    local barId = ENEMY_BAR_ID_PREFIX .. index

    if state.visible ~= true then
        set_element_visible(barId, true)
        state.visible = true
    end

    if not same_number(state.left, left) then
        widget:SetProperty(barId, "left", format_px(left))
        state.left = left
    end

    if not same_number(state.top, top) then
        widget:SetProperty(barId, "top", format_px(top))
        state.top = top
    end

    if state.hpRatio == nil then
        state.delayRatio = hpRatio
        state.delayRemaining = 0.0
        set_width_ratio(ENEMY_HP_DELAY_MASK_ID_PREFIX .. index, hpRatio)
    elseif hpRatio < state.hpRatio then
        state.delayRatio = math.max(state.delayRatio or state.hpRatio, state.hpRatio)
        state.delayRemaining = HP_DAMAGE_TRAIL_HOLD_SECONDS
        set_width_ratio(ENEMY_HP_DELAY_MASK_ID_PREFIX .. index, state.delayRatio)
    elseif hpRatio > state.hpRatio then
        state.delayRatio = hpRatio
        state.delayRemaining = 0.0
        set_width_ratio(ENEMY_HP_DELAY_MASK_ID_PREFIX .. index, hpRatio)
    end

    if not same_number(state.hpRatio, hpRatio) then
        set_width_ratio(ENEMY_HP_MASK_ID_PREFIX .. index, hpRatio)
        state.hpRatio = hpRatio
    end

    if not same_number(state.postureRatio, postureRatio) then
        set_width_ratio(ENEMY_POSTURE_MASK_ID_PREFIX .. index, postureRatio)
        state.postureRatio = postureRatio
    end
end

function HideAllEnemyHealthBars()
    if widget ~= nil then
        for i = 0, ENEMY_BAR_MAX - 1 do
            SetEnemyHealthBarVisible(i, false)
        end
    end

    clear_enemy_slot_mapping()
    clear_enemy_visibility_state()
    enemyBarStates = {}
end

function SetEnemyHealthBarActorVisible(enemyOrId, visible, durationSeconds)
    local valueType = type(enemyOrId)
    if (valueType == "userdata" or valueType == "table") and is_valid_object(enemyOrId) then
        cache_enemy_actor(enemyOrId)
    end

    local enemyId = resolve_enemy_visibility_id(enemyOrId)

    if enemyId == nil then
        return false
    end

    local state = get_enemy_visibility_state(enemyId)
    visible = visible == true

    if visible then
        durationSeconds = tonumber(durationSeconds)

        if durationSeconds ~= nil and durationSeconds > 0.0 then
            state.manualUntil = hudTimeSeconds + durationSeconds
        else
            state.manualVisible = true
            state.manualUntil = nil
        end

        return true
    end

    state.manualVisible = false
    state.manualUntil = nil
    state.revealUntil = nil

    local slot = enemySlotById[enemyId]

    if slot ~= nil then
        release_enemy_slot(slot)
    end

    return true
end

function SetEnemyHealthBarVisibility(enemyOrId, visible, durationSeconds)
    return SetEnemyHealthBarActorVisible(enemyOrId, visible, durationSeconds)
end

function SetEnemyHealthBarLockOnTarget(enemy)
    local enemyId = resolve_enemy_visibility_id(enemy)

    if lockOnEnemyId ~= nil and lockOnEnemyId ~= enemyId then
        SetEnemyHealthBarActorVisible(lockOnEnemyId, false)
    end

    lockOnEnemyId = enemyId

    if enemyId == nil then
        return false
    end

    return SetEnemyHealthBarActorVisible(enemyId, true)
end

function ClearEnemyHealthBarLockOnTarget()
    if lockOnEnemyId ~= nil then
        SetEnemyHealthBarActorVisible(lockOnEnemyId, false)
        lockOnEnemyId = nil
    end
end

refresh_hud_document_state = function()
    local hpRatio = lastHpRatio
    local hpDelayRatio = playerHpDelayRatio
    local bossHpRatio = lastBossHpRatio
    local bossPostureRatio = lastBossPostureRatio
    local playerPostureRatio = lastPlayerPostureRatio
    local tokenVisible = lastTokenVisible
    local bossVisible = lastBossHudVisible
    local itemVisible = lastItemHudVisible

    lastHpRatio = nil
    playerHpDelayRatio = nil
    lastBossHpRatio = nil
    lastBossPostureRatio = nil
    lastPlayerPostureRatio = nil
    playerPostureVisualState = nil
    bossPostureVisualState = nil
    lastTokenVisible = nil
    lastBossHudVisible = nil
    lastItemHudVisible = false
    enemyBarStates = {}

    if hpRatio ~= nil then
        SetPlayerHpRatio(hpRatio)
    end
    if hpDelayRatio ~= nil then
        playerHpDelayRatio = hpDelayRatio
        set_width_ratio(PLAYER_HP_DELAY_MASK_ID, hpDelayRatio)
    end
    if bossHpRatio ~= nil then
        SetBossHpRatio(bossHpRatio)
    end
    if bossPostureRatio ~= nil then
        SetBossPostureRatio(bossPostureRatio)
    end
    if playerPostureRatio ~= nil then
        SetPlayerPostureRatio(playerPostureRatio)
    end
    if tokenVisible ~= nil then
        SetPlayerTokenVisible(tokenVisible)
    end
    if bossVisible ~= nil then
        SetBossHUDVisible(bossVisible)
    end
    SetItemHUDVisible(itemVisible == true)
end

local function expose_hud_api()
    _G.HUD = _G.HUD or {}

    _G.HUD.SetPlayerHpRatio = SetPlayerHpRatio
    _G.HUD.SetBossHpRatio = SetBossHpRatio
    _G.HUD.SetPlayerPostureRatio = SetPlayerPostureRatio
    _G.HUD.SetBossPostureRatio = SetBossPostureRatio
    _G.HUD.SetHUDVisible = SetHUDVisible
    _G.HUD.SetHUDVisibility = SetHUDVisibility
    _G.HUD.SetVisible = SetHUDVisible
    _G.HUD.IsHUDVisible = IsHUDVisible
    _G.HUD.SetBossHUDVisible = SetBossHUDVisible
    _G.HUD.SetBossVisibility = SetBossVisibility
    _G.HUD.SetItemHUDVisible = SetItemHUDVisible
    _G.HUD.SetItemVisibility = SetItemVisibility
    _G.HUD.SetPlayerTokenVisible = SetPlayerTokenVisible
    _G.HUD.SetPlayerHUD = SetPlayerHUD
    _G.HUD.SetBossHUD = SetBossHUD
    _G.HUD.BindBossActor = BindBossActor
    _G.HUD.ClearBossActor = ClearBossActor
    _G.HUD.SetEnemyHealthBar = SetEnemyHealthBar
    _G.HUD.SetEnemyHealthBarVisible = SetEnemyHealthBarVisible
    _G.HUD.SetEnemyHealthBarActorVisible = SetEnemyHealthBarActorVisible
    _G.HUD.SetEnemyHealthBarVisibility = SetEnemyHealthBarVisibility
    _G.HUD.SetEnemyHealthBarLockOnTarget = SetEnemyHealthBarLockOnTarget
    _G.HUD.ClearEnemyHealthBarLockOnTarget = ClearEnemyHealthBarLockOnTarget
    _G.HUD.HideAllEnemyHealthBars = HideAllEnemyHealthBars
end

local function clear_hud_api()
    if _G.HUD == nil then
        return
    end

    if _G.HUD.SetBossPostureRatio == SetBossPostureRatio then
        _G.HUD.SetPlayerHpRatio = nil
        _G.HUD.SetBossHpRatio = nil
        _G.HUD.SetPlayerPostureRatio = nil
        _G.HUD.SetBossPostureRatio = nil
        _G.HUD.SetHUDVisible = nil
        _G.HUD.SetHUDVisibility = nil
        _G.HUD.SetVisible = nil
        _G.HUD.IsHUDVisible = nil
        _G.HUD.SetBossHUDVisible = nil
        _G.HUD.SetBossVisibility = nil
        _G.HUD.SetPlayerTokenVisible = nil
        _G.HUD.SetPlayerHUD = nil
        _G.HUD.SetBossHUD = nil
        _G.HUD.BindBossActor = nil
        _G.HUD.ClearBossActor = nil
        _G.HUD.SetEnemyHealthBar = nil
        _G.HUD.SetEnemyHealthBarVisible = nil
        _G.HUD.SetEnemyHealthBarActorVisible = nil
        _G.HUD.SetEnemyHealthBarVisibility = nil
        _G.HUD.SetEnemyHealthBarLockOnTarget = nil
        _G.HUD.ClearEnemyHealthBarLockOnTarget = nil
        _G.HUD.HideAllEnemyHealthBars = nil
    end
end

function BeginPlay()
    widget = UI.CreateWidget(HUD_DOCUMENT)

    if widget == nil then
        print("[HUD] failed to create " .. HUD_DOCUMENT)
        return
    end

    widget:SetWantsMouse(false)
    widget:AddToViewportZ(HUD_Z_ORDER)
    expose_hud_api()
    SetHUDVisible(not is_hud_force_hidden())
    HideAllEnemyHealthBars()
    SetBossHUDVisible(false)
    enemyBarUpdateElapsed = ENEMY_BAR_UPDATE_INTERVAL
    enemyActorRefreshElapsed = ENEMY_ACTOR_REFRESH_INTERVAL

    SetPlayerHpRatio(1.0)
    bind_player_pawn_events()
    SetBossHpRatio(1.0)
    SetBossPostureRatio(0.0)
    SetPlayerPostureRatio(0.0)
    SetPlayerTokenVisible(true)
end

function EndPlay()
    unbind_player_health()
    unbind_player_posture()
    unbind_player_pawn_events()
    unbind_boss_actor()
    clear_hud_api()

    if widget ~= nil then
        widget:RemoveFromParent()
        widget = nil
    end

    lastHpRatio = nil
    playerHpDelayRatio = nil
    playerHpDelayRemaining = 0.0
    lastBossHpRatio = nil
    lastBossPostureRatio = nil
    lastBossHudVisible = nil
    lastPlayerPostureRatio = nil
    playerPostureVisualState = "hidden"
    bossPostureVisualState = "hidden"
    lastTokenVisible = nil
    enemyBarStates = {}
    enemyBarUpdateElapsed = 0.0
    enemyActorRefreshElapsed = ENEMY_ACTOR_REFRESH_INTERVAL
    cachedEnemyActors = {}
    cachedEnemyIds = {}
    hudTimeSeconds = 0.0
    hudVisible = true
    clear_enemy_slot_mapping()
    clear_enemy_visibility_state()
end

function Tick(dt)
    dt = tonumber(dt) or 0.0
    hudTimeSeconds = hudTimeSeconds + dt

    if is_hud_force_hidden() and hudVisible then
        SetHUDVisible(false)
    end

    if not hudVisible then
        return
    end

    enemyBarUpdateElapsed = enemyBarUpdateElapsed + dt
    enemyActorRefreshElapsed = enemyActorRefreshElapsed + dt
    update_hp_damage_trails(dt)
    update_player_posture_effect()
    update_boss_posture_effect()

    if enemyBarUpdateElapsed < ENEMY_BAR_UPDATE_INTERVAL then
        return
    end

    enemyBarUpdateElapsed = enemyBarUpdateElapsed - ENEMY_BAR_UPDATE_INTERVAL

    if enemyBarUpdateElapsed > ENEMY_BAR_UPDATE_INTERVAL then
        enemyBarUpdateElapsed = ENEMY_BAR_UPDATE_INTERVAL
    end

    update_enemy_health_bars()
end
