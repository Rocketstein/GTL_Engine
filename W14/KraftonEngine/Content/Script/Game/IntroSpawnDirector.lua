-- ============================================================
-- IntroSpawnDirector.lua
-- Attach to a single controller actor placed in the GamePlay scene.
-- On scene start, runs the opening cinematic as a Tick-driven state machine:
--   1) Enter cutscene (blocks player input, phase -> CutScene).
--   2) Play the editor-authored camera path (ACameraCinematicActor, tag CINE_TAG).
--   3) Invoke every IntroEnemy's spawn effect. Each enemy's own SpawnDelay
--      (set on its UEnemySpawnEffectComponent in the editor) staggers its pop-in,
--      so the sequence is authored per-enemy, not hardcoded here.
--   4) Wait until the camera path AND all spawn effects finish, then exit cutscene.
-- NOTE: this drives itself off Tick(dt). It does NOT use the global coroutine
-- manager — nothing ticks UpdateCoroutines in this project, so coroutines never
-- advance here. Tick is reliably driven on script components (as GameFlowController is).
-- ============================================================

local CINE_TAG        = "IntroScenematic"  -- tag on the ACameraCinematicActor
local INTRO_ENEMY_TAG = "IntroEnemy"      -- tag on enemies that spawn during the intro
local TAIL_BEAT       = 0.5               -- hold after everything finishes, before handing control back
local MAX_INTRO       = 30.0              -- hard ceiling so the cutscene can never hang the game

-- Phases: "pending" (defer one frame past BeginPlay sweep) -> "running" -> "tail" -> "done".
local phase            = "pending"
local elapsed          = 0.0
local tail_elapsed     = 0.0
local entered_cutscene = false
local cine             = nil
local enemies          = {}

local function dbg(m) if print then print("[IntroSpawnDirector] " .. tostring(m)) end end

local function first_by_tag(tag)
    if World and World.FindActorsByTag then local l = World.FindActorsByTag(tag); if l and #l > 0 then return l[1] end end
    return nil
end
local function first_by_class(cn)
    if World and World.FindActorsByClass then local l = World.FindActorsByClass(cn); if l and #l > 0 then return l[1] end end
    return nil
end
-- Actor UFUNCTIONs (Play/Stop/IsPlaying) go through reflection — FindActorsByTag
-- hands back a base AActor handle.
local function rcall(o, fn, ...) if o and Reflection and Reflection.Call then return Reflection.Call(o, fn, ...) end return nil end

local function cine_is_playing()
    if not cine then return false end
    local ok, playing = pcall(function() return rcall(cine, "IsPlaying") end)
    return ok and playing == true
end

local function any_spawn_playing()
    if not (Scene and Scene.IsEnemySpawnPlaying) then return false end
    for _, e in ipairs(enemies) do
        if Scene.IsEnemySpawnPlaying(e) then return true end
    end
    return false
end

local function exit_cutscene()
    if entered_cutscene and Game and Game.ExitCutscene then
        Game.ExitCutscene()
    end
    entered_cutscene = false
end

-- Kicked off on the first Tick (one frame after BeginPlay), so every actor's
-- BeginPlay has run and the spawn-effect components have hidden their owners.
local function begin_intro()
    cine = first_by_tag(CINE_TAG) or first_by_class("ACameraCinematicActor")
    enemies = (World and World.FindActorsByTag) and World.FindActorsByTag(INTRO_ENEMY_TAG) or {}

    if Game and Game.EnterCutscene then
        Game.EnterCutscene()
        entered_cutscene = true
    end

    if cine then rcall(cine, "Play") end

    -- Fire every intro enemy's spawn effect at once; per-enemy SpawnDelay does the staggering.
    if Scene and Scene.PlayEnemySpawnEffect then
        for _, e in ipairs(enemies) do
            Scene.PlayEnemySpawnEffect(e)
        end
    end
    dbg("intro started: cine=" .. tostring(cine ~= nil) .. " enemies=" .. tostring(#enemies))
end

local function finish_intro()
    if cine then rcall(cine, "Stop") end
    exit_cutscene()
    phase = "done"
    dbg("intro complete -> gameplay")
end

function BeginPlay()
    phase = "pending"
    dbg("BeginPlay")
end

function Tick(dt)
    if phase == "pending" then
        begin_intro()
        phase = "running"
        elapsed = 0.0
        return
    end

    if phase == "running" then
        elapsed = elapsed + (dt or 0.0)
        if (not cine_is_playing() and not any_spawn_playing()) or elapsed >= MAX_INTRO then
            phase = "tail"
            tail_elapsed = 0.0
        end
        return
    end

    if phase == "tail" then
        tail_elapsed = tail_elapsed + (dt or 0.0)
        if tail_elapsed >= TAIL_BEAT then
            finish_intro()
        end
    end
end

function EndPlay()
    -- Scene tore down mid-intro: never leave the world in a cutscene (input disabled / phase stuck).
    if phase ~= "done" then exit_cutscene() end
end
