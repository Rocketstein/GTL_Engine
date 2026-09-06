-- ============================================================
-- BloodMoonPhase.lua
-- Attach to a single director actor placed in the GamePlay scene.
-- Mini-phase atmosphere: when every minor enemy is dead, the night
-- sets in — the blood moon rises, the spotlight reddens, and the height
-- fog deepens to a dark blue. Self-contained: it owns its own trigger
-- and a brief input lock; it does NOT touch the boss encounter or
-- BossIntroDirector yet (that coupling is a later step) -- it only swaps
-- the *music*, not the fight.
--
-- On trigger (fires once), when all minor enemies are dead:
--   * lerps the mood spotlight -> red and the height fog -> dark blue
--   * fades the battle BGM out across that same lerp window (BGMState)
--   * once settled: starts the boss BGM at full volume (no fade-in) and
--     calls BossIntroDirector.StartIntro
--
-- Atmosphere setters come from the Game-side `Scene` Lua table
-- (Source/Game/Lua/GameLuaBindings.cpp), which resolves the typed
-- component off each actor and pushes the change to the render scene.
-- ============================================================
__events = { BeginPlay = true, PostBeginPlay = false, Tick = true, PostStartMatch = true, EndPlay = false, OnOverlap = false, OnEndOverlap = false, OnHit = false, OnEndHit = false }

-- Shared single-channel BGM owner (require-cached; survives scene loads).
local BGMState = require("Game/BGMState")

-- ---- Tunables -------------------------------------------------------------
local ENEMY_TAG      = "Enemy"            -- minor enemies share this tag (matches HUD_Manager)
local SPOTLIGHT_TAG  = "MoodSpotlight"    -- tag the mood spotlight actor with this
local FOG_TAG        = "MoodFog"          -- tag the height-fog actor with this
local BLOOD_MOON_TAG = "BloodMoon"        -- tag the (hidden) blood-moon actor with this
local BLOOD_MOON_PARTICLE_TAG = "BloodMoonParticle" -- tag the blood-moon particle actor with this
local BOSS_INTRO_TAG = "BossIntroDirector" -- tag the boss intro director actor with this
local BOSS_INTRO_START_FLAG = "BossIntroDirectorStartOk"

local LERP_TIME      = 2.5                 -- seconds: spotlight/fog ease to the night look

-- Boss BGM that replaces the battle track once the night settles.
local BGM_BOSS       = "BGM_Boss"
local BGM_BOSS_FILE  = "BGM/rubyzephyr-silent-blade (Boss_bgm).mp3"
local BGM_VOLUME     = 0.5                 -- matches GameFlowController's BGM level

-- Target look. Spot reddens; fog deepens to a dark blue.
local SPOT_RED       = { 1.0, 0.08, 0.08, 1.0 }
local FOG_NIGHT      = { 0.02, 0.04, 0.12, 1.0 }

-- ---- State ----------------------------------------------------------------
local spotlight, fog, bloodMoon, bloodMoonParticle
local spotFrom, fogFrom          -- captured start colors (lerp from authored values)
local sawEnemies = false         -- guard: don't fire before any minor enemy ever lived
local triggered  = false         -- transition started (fires once)
local handedOff  = false         -- transition finished; stop ticking
local t          = 0.0           -- seconds since trigger

-- ---- Helpers --------------------------------------------------------------
local function dbg(m) if print then print("[BloodMoonPhase] " .. tostring(m)) end end

local function first_by_tag(tag)
    if World and World.FindActorsByTag then
        local l = World.FindActorsByTag(tag)
        if l and #l > 0 then return l[1] end
    end
    return nil
end

local function is_valid(o)
    if o == nil then return false end
    if IsValid then return IsValid(o) end
    return true
end

local function safe_call(target, fn, ...)
    if target == nil then return nil end
    local f = target[fn]
    if f == nil then return nil end
    local ok, result = pcall(f, target, ...)
    if not ok then return nil end
    return result
end

local function find_boss_intro_director()
    local director = first_by_tag(BOSS_INTRO_TAG) or first_by_tag("BossIntro")
    if director then return director end

    if World and World.FindActorByName then
        return World.FindActorByName("BossIntroDirector")
            or World.FindActorByName("BossIntroDirectorBP")
    end

    return nil
end

local function call_director(component, fn)
    if component and component.CallFunction then
        if _G ~= nil then _G[BOSS_INTRO_START_FLAG] = nil end
        if safe_call(component, "CallFunction", fn) ~= true then return false end
        return _G == nil or _G[BOSS_INTRO_START_FLAG] ~= false
    end
    return false
end

local function start_boss_intro()
    local director = find_boss_intro_director()
    if not director then return false end

    if call_director(safe_call(director, "GetLuaScriptComponent"), "StartIntro") then
        return true
    end
    if call_director(safe_call(director, "GetLuaBlueprintComponent"), "StartIntro") then
        return true
    end

    return false
end

local function lerp(a, b, s) return a + (b - a) * s end

local function set_particle_active(actor, active, reset)
    if actor == nil then return false end

    -- Preferred: resolve the typed PSC on the C++ side (avoids the base-handle
    -- gotcha — FindActorsByTag hands us a base AActor, so reaching the derived
    -- GetParticleSystemComponent / Activate from Lua can silently nil out).
    if Scene and Scene.SetParticleActive then
        return Scene.SetParticleActive(actor, active == true, reset == true) == true
    end

    -- Fallback: legacy direct path.
    local psc = safe_call(actor, "GetParticleSystemComponent")
    if not is_valid(psc) then return false end
    if active then
        safe_call(psc, "Activate", reset == true)
    else
        safe_call(psc, "Deactivate")
    end
    return true
end

-- A minor enemy = tagged ENEMY_TAG, no EncounterComponent (that marks the boss).
local function is_boss(enemy)
    return is_valid(safe_call(enemy, "GetEncounterComponent"))
end

local function is_alive_minor(enemy)
    if not is_valid(enemy) or is_boss(enemy) then return false end
    local health = safe_call(enemy, "GetHealthComponent")
    if not is_valid(health) then return false end
    return safe_call(health, "IsDead") ~= true
end

-- Returns the count of live minor enemies in the scene.
local function live_minor_count()
    local n = 0
    if World and World.FindActorsByTag then
        local enemies = World.FindActorsByTag(ENEMY_TAG)
        if enemies then
            for _, e in ipairs(enemies) do
                if is_alive_minor(e) then n = n + 1 end
            end
        end
    end
    return n
end

-- ---- Lifecycle ------------------------------------------------------------
local function resolve_actors()
    spotlight         = spotlight         or first_by_tag(SPOTLIGHT_TAG)
    fog               = fog               or first_by_tag(FOG_TAG)
    bloodMoon         = bloodMoon         or first_by_tag(BLOOD_MOON_TAG)
    if bloodMoonParticle == nil then
        bloodMoonParticle = first_by_tag(BLOOD_MOON_PARTICLE_TAG)
        if bloodMoonParticle ~= nil and not triggered then
            set_particle_active(bloodMoonParticle, false)
        end
    end
end

local function begin_transition()
    triggered = true
    t = 0.0
    dbg("all minor enemies cleared -> blood moon rises")
    set_particle_active(bloodMoonParticle, true, true)

    -- Capture authored start colors so the lerp eases from them (no snap).
    if Scene and Scene.GetSpotColor and spotlight then
        local r, g, b, a = Scene.GetSpotColor(spotlight)
        spotFrom = { r, g, b, a }
    end
    if Scene and Scene.GetFogColor and fog then
        local r, g, b, a = Scene.GetFogColor(fog)
        fogFrom = { r, g, b, a }
    end

end

local function apply_look(s)
    -- Ease-out: quick swing, gentle settle (matches GameFlowController's curve).
    local e = 1 - (1 - s) * (1 - s)
    if Scene and Scene.SetSpotColor and spotlight and spotFrom then
        Scene.SetSpotColor(spotlight,
            lerp(spotFrom[1], SPOT_RED[1], e),
            lerp(spotFrom[2], SPOT_RED[2], e),
            lerp(spotFrom[3], SPOT_RED[3], e),
            lerp(spotFrom[4], SPOT_RED[4], e))
    end
    if Scene and Scene.SetFogColor and fog and fogFrom then
        Scene.SetFogColor(fog,
            lerp(fogFrom[1], FOG_NIGHT[1], e),
            lerp(fogFrom[2], FOG_NIGHT[2], e),
            lerp(fogFrom[3], FOG_NIGHT[3], e),
            lerp(fogFrom[4], FOG_NIGHT[4], e))
    end
end

function BeginPlay()
    resolve_actors()
    -- Keep the blood moon hidden until the transition (in case it was authored visible).
    if Scene and Scene.SetActorVisible and bloodMoon then
        Scene.SetActorVisible(bloodMoon, false)
    end
    set_particle_active(bloodMoonParticle, false)
end

function PostStartMatch()
    resolve_actors()
end

function Tick(dt)
    if handedOff then return end
    resolve_actors()

    if not triggered then
        local n = live_minor_count()
        if n > 0 then
            sawEnemies = true
        elseif sawEnemies then
            -- Saw enemies, now zero remain -> the arena is cleared.
            begin_transition()
        end
        return
    end

    -- Transition running: ease the night look in, then hand off to the boss intro.
    t = t + (dt or 0.0)
    local s = math.min(1.0, t / LERP_TIME)
    apply_look(s)

    -- Battle BGM fades out (linear) in lockstep with the look-shift.
    if BGMState and BGMState.SetVolume and BGMState.GetBase then
        BGMState.SetVolume(BGMState.GetBase() * (1 - s))
    end

    if s >= 1.0 then
        handedOff = true
        -- Night has settled: boss BGM kicks in at full volume (no fade-in).
        -- Ensure() stops the faded-out battle loop and plays the boss track
        -- at BGM_VOLUME, so it starts hard rather than easing up.
        if BGMState and BGMState.Ensure then
            BGMState.Ensure(BGM_BOSS, BGM_BOSS_FILE, BGM_VOLUME)
        end
        if start_boss_intro() then
            dbg("atmosphere settled -> boss intro")
        else
            dbg("atmosphere settled -> boss BGM, no intro director found")
        end
    end
end
