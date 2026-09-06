local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")
local Audio = require("Core.Audio")
local CameraShake = require("Core.CameraShake")
local CameraManager = require("CameraManager")
local Co = require("LuaCoroutine")

local BACKGROUND_MUSIC_KEY = "background"
local BACKGROUND_MUSIC_VOLUME = 0.7
local KEY_CONTROL = 0x11
local KEY_LEFT_CONTROL = 0xA2
local KEY_RIGHT_CONTROL = 0xA3
local KEY_Z = 0x5A
local START_FADE_DURATION = 1.0
local START_FADE_COLOR = { 0.0, 0.0, 0.0 }
local START_FADE_CURVE = "EaseInOut"
local MAIN_CAMERA_SWITCH_DELAY = 2.0
local MAIN_CAMERA_BLEND_DURATION = 2.0
local LETTERBOX_VISIBLE_ASPECT_RATIO = 2.39
local LETTERBOX_FALLBACK_VIEWPORT_ASPECT_RATIO = 16.0 / 9.0
local LETTERBOX_OPACITY = 1.0
local LETTERBOX_OUT_DURATION = 0.65
local LETTERBOX_COLOR = { 0.0, 0.0, 0.0 }

local GameManager = {
    GameTimeLimit = 600.0,

    TimeRemaining = 600.0,
    KillCount = 0,
    IsGameOver = false,
    IsPaused = false,
    GodMode = false,

    SessionPrepared = false,
    PlayerRegistered = false,
    GameStartRequested = false,
    Initialized = false,

    PlayerScript = nil,
    WeaponInventory = nil,
    LevelSystem = nil,
    World = nil,
    LevelUpUI = nil,
    ExpBarUI = nil,
    KillCountUI = nil,
    ResultUI = nil,
    MainMenuController = nil,
    FadeInOnNextGameStart = false,
    BackgroundMusicHandle = nil,
    BackgroundMusicPositionMs = 0.0,
    IsLevelUpSelectionActive = false,
    CurrentLevelUpOptions = nil,
    LastResult = nil,
    ManagerComponent = nil,
    CameraSwitchCoroutine = nil,
    SessionID = 0,
    Stats = {}
}

local GAMEPLAY_POOL_CLASSES = {
    "AGroundEnemyActor",
    "AFlyingWaveEnemyActor",
    "AEnemyBaseActor",
    "AProjectileActor",
    "APickupActor",
    "ADeathDecalActor",
}

local function captureWorldFromScript(script)
    if script ~= nil and type(script.GetWorld) == "function" then
        local world = script.GetWorld()
        if world ~= nil and world:IsValid() then
            GameManager.World = world
            if world.SetGameplayPaused ~= nil then
                world:SetGameplayPaused(GameManager.IsPaused == true)
            end
        end
    end
end

local function playGameStartFadeIn()
    local runner = GameManager.PlayerScript or GameManager.MainMenuController
    if runner == nil or type(runner.GetPostProcess) ~= "function" then
        Log("[GameManager] Game start fade skipped: runner unavailable.")
        return
    end

    local postProcess = runner:GetPostProcess()
    if postProcess == nil or not postProcess:IsValid() then
        Log("[GameManager] Game start fade skipped: PostProcess handle unavailable.")
        return
    end

    postProcess:SetFadeAlpha(1.0, START_FADE_COLOR)
    postProcess:FadeIn(START_FADE_DURATION, START_FADE_COLOR, START_FADE_CURVE)
end

local function getPostProcess()
    local runner = GameManager.PlayerScript or GameManager.MainMenuController or GameManager.ManagerComponent
    if runner == nil or type(runner.GetPostProcess) ~= "function" then
        return nil
    end

    local postProcess = runner:GetPostProcess()
    if postProcess == nil or not postProcess:IsValid() then
        return nil
    end

    return postProcess
end

local function getLetterboxColor(postProcess)
    if postProcess ~= nil and type(postProcess.GetLetterboxColor) == "function" then
        local color = postProcess:GetLetterboxColor()
        if color ~= nil then
            return color
        end
    end

    return LETTERBOX_COLOR
end

local function getViewportAspectRatio()
    if type(GetViewportAspectRatio) == "function" then
        local aspectRatio = tonumber(GetViewportAspectRatio()) or 0.0
        if aspectRatio > 0.0 then
            return aspectRatio
        end
    end

    return LETTERBOX_FALLBACK_VIEWPORT_ASPECT_RATIO
end

local function setLetterboxAspectRatio(postProcess, aspectRatio, bEnabled)
    if postProcess == nil or type(postProcess.SetLetterbox) ~= "function" then
        return false
    end

    local targetAspectRatio = math.max(0.0001, tonumber(aspectRatio) or LETTERBOX_VISIBLE_ASPECT_RATIO)
    postProcess:SetLetterbox(targetAspectRatio, LETTERBOX_OPACITY, getLetterboxColor(postProcess))

    if type(postProcess.SetLetterboxEnabled) == "function" then
        postProcess:SetLetterboxEnabled(bEnabled ~= false)
    end

    return true
end

local function resetOpeningLetterbox()
    local postProcess = getPostProcess()
    if postProcess == nil then
        return
    end

    setLetterboxAspectRatio(postProcess, LETTERBOX_VISIBLE_ASPECT_RATIO, true)
end

local function playLetterboxOut()
    local postProcess = getPostProcess()
    if postProcess == nil then
        Log("[GameManager] Letterbox out skipped: PostProcess handle unavailable.")
        return
    end

    local fromAspectRatio = LETTERBOX_VISIBLE_ASPECT_RATIO
    if type(postProcess.GetLetterboxTargetAspectRatio) == "function" then
        fromAspectRatio = tonumber(postProcess:GetLetterboxTargetAspectRatio()) or fromAspectRatio
    end

    local toAspectRatio = getViewportAspectRatio()
    local elapsed = 0.0
    local duration = math.max(0.0001, LETTERBOX_OUT_DURATION)

    while elapsed < duration do
        local deltaTime = Co.WaitNextFrame() or 0.0
        elapsed = elapsed + math.max(0.0, deltaTime)

        local t = math.max(0.0, math.min(1.0, elapsed / duration))
        local ease = t * t * (3.0 - 2.0 * t)
        local aspectRatio = fromAspectRatio + (toAspectRatio - fromAspectRatio) * ease
        setLetterboxAspectRatio(postProcess, aspectRatio, true)
    end

    setLetterboxAspectRatio(postProcess, toAspectRatio, false)
end

local function normalizeSoundPosition(key, positionMs)
    local position = positionMs or 0.0
    local duration = Audio.GetLength(key) or 0.0

    if duration > 0.0 then
        position = position % duration
    end

    return position
end

local function resetStats()
    GameManager.Stats = {
        MaxHP = 100.0,
        CurrentHP = 100.0,
        MoveSpeedMult = 1.0,
        ExpMult = 1.0,
        PickupRangeMult = 1.0,
        DamageMult = 1.0,
    }
end

local function stopAllWeapons()
    if GameManager.WeaponInventory == nil or GameManager.WeaponInventory.Weapons == nil then
        return
    end

    for _, weapon in pairs(GameManager.WeaponInventory.Weapons) do
        if weapon.Stop ~= nil then
            weapon:Stop()
        end
    end
end

local function cleanupGameplayActors()
    if type(GetActorPoolManager) ~= "function" then
        return
    end

    local pool = GetActorPoolManager()
    if pool == nil or not pool:IsValid() or pool.ReleaseActiveByClass == nil then
        return
    end

    for _, className in ipairs(GAMEPLAY_POOL_CLASSES) do
        pool:ReleaseActiveByClass(className)
    end
end

local function updateKillCountUI()
    if GameManager.KillCountUI ~= nil and type(GameManager.KillCountUI.SetKillCount) == "function" then
        GameManager.KillCountUI:SetKillCount(GameManager.KillCount or 0)
    end
end

local function resetRunState()
    stopAllWeapons()
    resetStats()

    GameManager.TimeRemaining = GameManager.GameTimeLimit or 600.0
    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.GodMode = false
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil
    GameManager.LastResult = nil

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    if GameManager.PlayerScript ~= nil and type(GameManager.PlayerScript.ResetRuntimeAddedComponents) == "function" then
        GameManager.PlayerScript.ResetRuntimeAddedComponents()
    end

    if GameManager.PlayerScript ~= nil then
        GameManager.WeaponInventory = WeaponInventory.New(GameManager.PlayerScript)
        GameManager.LevelSystem = LevelSystem.New(GameManager.PlayerScript, GameManager.WeaponInventory)
    else
        GameManager.WeaponInventory = nil
        GameManager.LevelSystem = nil
    end

    updateKillCountUI()
end

local function buildFallbackResult(resultType, reason)
    return {
        ResultType = resultType,
        Reason = reason,
        RemainingTime = math.max(0.0, tonumber(GameManager.TimeRemaining) or 0.0),
        KillCount = GameManager.KillCount or 0,
        Rank = 0,
        Records = {},
    }
end

function GameManager.SwitchToMainCameraActor()
    if GameManager.PlayerScript == nil or type(GameManager.PlayerScript.GetActor) ~= "function" then
        Log("[GameManager] MainCameraActor switch failed: PlayerScript is nil.")
        return false
    end

    local playerActor = GameManager.PlayerScript:GetActor()
    if playerActor == nil or not playerActor:IsValid() then
        Log("[GameManager] MainCameraActor switch failed: Player actor is invalid.")
        return false
    end

    local runner = GameManager.ManagerComponent or GameManager.PlayerScript
    if runner == nil or type(runner.QueryActorByTagClosest) ~= "function" then
        Log("[GameManager] MainCameraActor switch failed: query runner is invalid.")
        return false
    end

    local mainCameraActor = runner:QueryActorByTagClosest(
        "MainCameraActor",
        playerActor:GetLocation(),
        100000.0
    )

    if mainCameraActor == nil or not mainCameraActor:IsValid() then
        Log("[GameManager] MainCameraActor switch failed: MainCameraActor tag not found.")
        return false
    end

    if not CameraManager.InitializeFor(playerActor) then
        Log("[GameManager] MainCameraActor switch failed: CameraManager.InitializeFor failed.")
        return false
    end

    if not CameraManager.SetViewTargetBlend(mainCameraActor, MAIN_CAMERA_BLEND_DURATION, "EaseInOut", 2.0, false) then
        Log("[GameManager] MainCameraActor switch failed: SetViewTargetBlend failed.")
        return false
    end

    return true
end

function GameManager.StartMainCameraSwitchCoroutine()
    local runner = GameManager.ManagerComponent or GameManager.PlayerScript
    if runner == nil or type(runner.StartCoroutine) ~= "function" then
        Log("[GameManager] MainCameraActor switch coroutine failed: StartCoroutine is nil.")
        return false
    end

    if GameManager.CameraSwitchCoroutine ~= nil and type(runner.StopCoroutine) == "function" then
        runner.StopCoroutine(GameManager.CameraSwitchCoroutine)
        GameManager.CameraSwitchCoroutine = nil
    end

    GameManager.CameraSwitchCoroutine = runner.StartCoroutine(function()
        Co.Wait(MAIN_CAMERA_SWITCH_DELAY)
        if GameManager.SwitchToMainCameraActor() then
            Co.Wait(MAIN_CAMERA_BLEND_DURATION)
            playLetterboxOut()
        end
        GameManager.CameraSwitchCoroutine = nil
    end)

    return GameManager.CameraSwitchCoroutine ~= nil
end

function GameManager.PlayBackgroundMusic()
    if GameManager.IsGameOver then
        return
    end

    if GameManager.BackgroundMusicHandle ~= nil and Audio.IsPlaying(GameManager.BackgroundMusicHandle) then
        return
    end

    local handle = Audio.PlayLoop(BACKGROUND_MUSIC_KEY, Audio.Bus.BGM, BACKGROUND_MUSIC_VOLUME)
    GameManager.BackgroundMusicHandle = handle

    if handle ~= nil then
        Audio.SetPosition(handle, normalizeSoundPosition(BACKGROUND_MUSIC_KEY, GameManager.BackgroundMusicPositionMs))
    end
end

function GameManager.PauseBackgroundMusic()
    local handle = GameManager.BackgroundMusicHandle
    if handle ~= nil then
        local position = Audio.GetPosition(handle)
        if position ~= nil then
            GameManager.BackgroundMusicPositionMs = normalizeSoundPosition(BACKGROUND_MUSIC_KEY, position)
        end

        Audio.Stop(handle)
    end

    GameManager.BackgroundMusicHandle = nil
end

function GameManager.StopBackgroundMusic(resetPosition)
    local handle = GameManager.BackgroundMusicHandle
    if handle ~= nil then
        if resetPosition ~= true then
            local position = Audio.GetPosition(handle)
            if position ~= nil then
                GameManager.BackgroundMusicPositionMs = normalizeSoundPosition(BACKGROUND_MUSIC_KEY, position)
            end
        end

        Audio.Stop(handle)
    end

    GameManager.BackgroundMusicHandle = nil

    if resetPosition == true then
        GameManager.BackgroundMusicPositionMs = 0.0
    end
end

function GameManager._ResetState()
    GameManager.StopBackgroundMusic(true)

    if GameManager.World ~= nil and GameManager.World:IsValid() and GameManager.World.SetGameplayPaused ~= nil then
        GameManager.World:SetGameplayPaused(false)
    end

    resetStats()

    GameManager.KillCount = 0
    GameManager.IsGameOver = false
    GameManager.GodMode = false
    GameManager.IsPaused = false
    GameManager.Initialized = false

    GameManager.SessionPrepared = false
    GameManager.PlayerRegistered = false
    GameManager.GameStartRequested = false

    GameManager.PlayerScript = nil
    GameManager.WeaponInventory = nil
    GameManager.LevelSystem = nil
    GameManager.World = nil
    GameManager.LevelUpUI = nil
    GameManager.ExpBarUI = nil
    GameManager.KillCountUI = nil
    GameManager.ResultUI = nil
    GameManager.MainMenuController = nil
    GameManager.FadeInOnNextGameStart = false
    GameManager.BackgroundMusicHandle = nil
    GameManager.BackgroundMusicPositionMs = 0.0
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil
    GameManager.LastResult = nil
    GameManager.ManagerComponent = nil
    GameManager.CameraSwitchCoroutine = nil

    Log("[GameManager] Session State Fully Reset.")
end

function GameManager.PrepareSession(gameTimeLimit)
    GameManager._ResetState()

    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = true
    GameManager.GameStartRequested = true

    Log("[GameManager] Session Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
    GameManager._CheckAndStart()
end

function GameManager.PrepareMainMenu(gameTimeLimit)
    GameManager._ResetState()

    GameManager.GameTimeLimit = gameTimeLimit or 600.0
    GameManager.TimeRemaining = GameManager.GameTimeLimit
    GameManager.SessionPrepared = false
    GameManager.GameStartRequested = false
    GameManager.SetGameplayPaused(true)

    Log("[GameManager] Main Menu Prepared (TimeLimit: " .. tostring(GameManager.GameTimeLimit) .. ")")
end

function GameManager.RequestStartGame(bFadeInOnStart)
    if GameManager.Initialized then
        return true
    end

    cleanupGameplayActors()
    resetRunState()

    GameManager.SessionPrepared = true
    GameManager.GameStartRequested = true
    GameManager.FadeInOnNextGameStart = bFadeInOnStart == true
    resetOpeningLetterbox()

    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.Hide) == "function" then
        GameManager.ResultUI:Hide()
    end

    -- 게임 시작 요청 시 카메라 컷을 강제하여 이전 세션의 잔상을 제거합니다.
    if GameManager.PlayerScript ~= nil then
        local playerActor = GameManager.PlayerScript:GetActor()
        if playerActor ~= nil and playerActor:IsValid() then
            CameraManager.InitializeFor(playerActor)
            CameraManager.SetGameCameraCutThisFrame()
        end
    end

    GameManager._CheckAndStart()
    return GameManager.Initialized == true
end

function GameManager.RegisterPlayer(playerScript)
    GameManager.PlayerScript = playerScript
    captureWorldFromScript(playerScript)

    GameManager.WeaponInventory = WeaponInventory.New(playerScript)
    GameManager.LevelSystem = LevelSystem.New(playerScript, GameManager.WeaponInventory)

    GameManager.PlayerRegistered = true
    Log("[GameManager] Player Registered.")
    GameManager._CheckAndStart()
end

function GameManager._CheckAndStart()
    if GameManager.SessionPrepared and GameManager.PlayerRegistered and GameManager.GameStartRequested and not GameManager.Initialized then
        GameManager.SetGameplayPaused(false)
        GameManager.Initialized = true
        GameManager.OnGameStart()
    end
end

function GameManager.OnGameStart()
    GameManager.SessionID = GameManager.SessionID + 1

    if GameManager.FadeInOnNextGameStart then
        GameManager.FadeInOnNextGameStart = false
        playGameStartFadeIn()
    end

    GameManager.PlayBackgroundMusic()

    -- VFX 액터 풀 웜업 (총구 화염, 피격 이펙트, 폭발 등 공유)
    local pool = GetActorPoolManager()
    if pool ~= nil and pool:IsValid() then
        pool:Warmup("ASubUVVfxActor", 20)
    end

    if GameManager.WeaponInventory then
        GameManager.WeaponInventory:AddWeapon("MainCannon")
    end
    GameManager.StartMainCameraSwitchCoroutine()
    Log("[GameManager] --- GAME START ---")
end

function GameManager.IsGameplayPaused()
    return GameManager.IsPaused == true
end

function GameManager.IsInputAllowed()
    if not GameManager.Initialized or GameManager.IsGameOver or GameManager.IsPaused then
        return false
    end
    return true
end

local function isGodModeHotkeyPressed()
    if Input == nil or Input.GetKeyDown == nil or Input.GetKey == nil then
        return false
    end

    local ctrlDown =
        Input.GetKey(KEY_CONTROL) or
        Input.GetKey(KEY_LEFT_CONTROL) or
        Input.GetKey(KEY_RIGHT_CONTROL)

    return ctrlDown and Input.GetKeyDown(KEY_Z)
end

function GameManager.ActivateGodMode()
    GameManager.GodMode = true

    if GameManager.Stats ~= nil then
        GameManager.Stats.CurrentHP = GameManager.Stats.MaxHP or GameManager.Stats.CurrentHP or 100.0
    end

    local activatedCount = 0
    if GameManager.WeaponInventory ~= nil and GameManager.WeaponInventory.ActivateAllWeaponsAtLevel ~= nil then
        activatedCount = GameManager.WeaponInventory:ActivateAllWeaponsAtLevel(3)
    end

    if GameManager.PlayerScript ~= nil and GameManager.PlayerScript.UpdateHealthBar ~= nil then
        GameManager.PlayerScript:UpdateHealthBar()
    end

    Log("[GameManager] GodMode enabled. weapons level 3 activated = " .. tostring(activatedCount))
end

function GameManager.SetGameplayPaused(paused)
    GameManager.IsPaused = paused == true

    local world = GameManager.World
    if world ~= nil and world:IsValid() and world.SetGameplayPaused ~= nil then
        world:SetGameplayPaused(GameManager.IsPaused)
    end
end

function GameManager.RegisterLevelUpUI(uiScript)
    GameManager.LevelUpUI = uiScript
    captureWorldFromScript(uiScript)

    if GameManager.IsLevelUpSelectionActive and GameManager.CurrentLevelUpOptions ~= nil and
       uiScript ~= nil and type(uiScript.Show) == "function" then
        uiScript:Show(GameManager.CurrentLevelUpOptions)
    end
end

function GameManager.RegisterExpBarUI(expBarScript)
    GameManager.ExpBarUI = expBarScript
    captureWorldFromScript(expBarScript)

    if expBarScript ~= nil and type(expBarScript.SetLevelUpMode) == "function" then
        expBarScript:SetLevelUpMode(GameManager.IsLevelUpSelectionActive)
    end
end

function GameManager.RegisterKillCountUI(uiScript)
    GameManager.KillCountUI = uiScript
    captureWorldFromScript(uiScript)

    if uiScript ~= nil and type(uiScript.SetKillCount) == "function" then
        uiScript:SetKillCount(GameManager.KillCount or 0)
    end
end

function GameManager.RegisterResultUI(uiScript)
    GameManager.ResultUI = uiScript
    captureWorldFromScript(uiScript)

    if GameManager.LastResult ~= nil and uiScript ~= nil and type(uiScript.ShowResult) == "function" then
        uiScript:ShowResult(GameManager.LastResult)
    end
end

function GameManager.UnregisterResultUI(uiScript)
    if GameManager.ResultUI == uiScript then
        GameManager.ResultUI = nil
    end
end

function GameManager.RegisterMainMenuController(controller)
    GameManager.MainMenuController = controller
    captureWorldFromScript(controller)
end

function GameManager.UnregisterKillCountUI(uiScript)
    if GameManager.KillCountUI == uiScript then
        GameManager.KillCountUI = nil
    end
end

function GameManager.OnEnemyKilled(enemyActor)
    if GameManager.IsGameOver then
        return GameManager.KillCount or 0
    end

    GameManager.KillCount = (GameManager.KillCount or 0) + 1

    if GameManager.KillCountUI ~= nil and type(GameManager.KillCountUI.SetKillCount) == "function" then
        GameManager.KillCountUI:SetKillCount(GameManager.KillCount)
    end

    return GameManager.KillCount
end

function GameManager.BeginLevelUpSelection(options)
    if options == nil or #options <= 0 then
        return false
    end

    if GameManager.LevelUpUI == nil or type(GameManager.LevelUpUI.Show) ~= "function" then
        Log("[GameManager] LevelUpUI is not registered.")
        return false
    end

    GameManager.CurrentLevelUpOptions = options
    GameManager.IsLevelUpSelectionActive = true
    GameManager.SetGameplayPaused(true)
    GameManager.PauseBackgroundMusic()
    Audio.Play("levelup", Audio.Bus.UI, 1.0)

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(true)
    end

    GameManager.LevelUpUI:Show(options)
    return true
end

function GameManager.ConfirmLevelUpChoice(index)
    if not GameManager.IsLevelUpSelectionActive or GameManager.LevelSystem == nil then
        return false
    end

    local selected = GameManager.LevelSystem:SelectPendingOption(index)
    if not selected then
        return false
    end

    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil

    if GameManager.LevelSystem.PendingLevelUps > 0 then
        return GameManager.LevelSystem:RequestLevelUpSelection()
    end

    GameManager.EndLevelUpSelection()
    return true
end

function GameManager.EndLevelUpSelection()
    GameManager.IsLevelUpSelectionActive = false
    GameManager.CurrentLevelUpOptions = nil

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    GameManager.SetGameplayPaused(false)
    GameManager.PlayBackgroundMusic()
end

function GameManager.Tick(deltaTime)
    if GameManager.Initialized and not GameManager.IsGameOver and isGodModeHotkeyPressed() then
        GameManager.ActivateGodMode()
    end

    if not GameManager.Initialized or GameManager.IsGameOver or GameManager.IsGameplayPaused() then
        return
    end

    if type(deltaTime) ~= "number" then return end

    GameManager.TimeRemaining = math.max(0, GameManager.TimeRemaining - deltaTime)
    if GameManager.TimeRemaining <= 0 then
        GameManager.OnGameClear()
    end
end

function GameManager.PlayerGetDamage(damage)
    if GameManager.IsGameOver or GameManager.IsGameplayPaused() then return end
    if GameManager.GodMode then
        GameManager.Stats.CurrentHP = GameManager.Stats.MaxHP or GameManager.Stats.CurrentHP or 100.0
        return
    end

    GameManager.Stats.CurrentHP = math.max(0, GameManager.Stats.CurrentHP - damage)

    -- 피격 시 0.1초 동안 빨간색으로 표시
    if GameManager.PlayerScript and GameManager.PlayerScript.StartCoroutine then
        GameManager.PlayerScript:StartCoroutine(function()
            local visual = GameManager.PlayerScript:GetComponentByName("UStaticMeshComponent", "BodyMesh")
            local turret = GameManager.PlayerScript:GetComponentByName("UStaticMeshComponent", "Visual_MainCannon_0")

            local comps = {}
            if visual and visual:IsValid() then table.insert(comps, visual) end
            if turret and turret:IsValid() then table.insert(comps, turret) end

            for _, comp in ipairs(comps) do
                comp:SetMaterialVector4Parameter(0, "SectionColor", {1.0, 0.0, 0.0, 1.0})
            end

            coroutine.yield("wait_time", 0.1)

            for _, comp in ipairs(comps) do
                if comp:IsValid() then
                    comp:SetMaterialVector4Parameter(0, "SectionColor", {1.0, 1.0, 1.0, 1.0})
                end
            end
        end)
    end

    if GameManager.Stats.CurrentHP <= 0 then
        CameraShake.Play("Explosion", 1.0)
        GameManager.OnGameOver("Player Died")
    end
end

function GameManager.OnPickupExp(baseAmount)
    if GameManager.IsGameplayPaused() then return end
    local totalExp = baseAmount * (GameManager.Stats.ExpMult or 1.0)
    if GameManager.LevelSystem then GameManager.LevelSystem:AddExp(totalExp) end
end

function GameManager.OnPickupChest()
    if GameManager.IsGameplayPaused() then return false end
    if GameManager.WeaponInventory then return GameManager.WeaponInventory:OpenChest() end
    return false
end

function GameManager.FinishRun(resultType, reason, finalRemainingTime, finalKillCount)
    GameManager.IsGameOver = true
    GameManager.Initialized = false
    GameManager.SetGameplayPaused(true)
    GameManager.StopBackgroundMusic(true)
    stopAllWeapons()

    if GameManager.LevelUpUI ~= nil and type(GameManager.LevelUpUI.Hide) == "function" then
        GameManager.LevelUpUI:Hide()
    end

    if GameManager.ExpBarUI ~= nil and type(GameManager.ExpBarUI.SetLevelUpMode) == "function" then
        GameManager.ExpBarUI:SetLevelUpMode(false)
    end

    cleanupGameplayActors()

    local remainingTime = finalRemainingTime or math.max(0.0, tonumber(GameManager.TimeRemaining) or 0.0)
    local killCount = finalKillCount or GameManager.KillCount or 0
    local record = nil
    if type(ScoreBoard_AddRecord) == "function" then
        record = ScoreBoard_AddRecord(resultType, remainingTime, killCount)
    end

    if record == nil then
        record = buildFallbackResult(resultType, reason)
        record.RemainingTime = remainingTime
        record.KillCount = killCount
    else
        record.Reason = reason
        record.ResultType = record.ResultType or resultType
        record.RemainingTime = record.RemainingTime or remainingTime
        record.KillCount = record.KillCount or killCount
        record.Records = record.Records or {}
    end

    GameManager.LastResult = record

    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.ShowResult) == "function" then
        GameManager.ResultUI:ShowResult(record)
    end

    Log("[GameManager] " .. tostring(resultType) .. ": " .. tostring(reason))
end

function GameManager.StartFinishSequence(resultType, reason)
    if GameManager.IsGameOver then return end

    -- 연출 시작 전 즉시 수행해야 할 로직들 (데이터 고정)
    local finalRemainingTime = math.max(0.0, tonumber(GameManager.TimeRemaining) or 0.0)
    local finalKillCount = GameManager.KillCount or 0

    GameManager.IsGameOver = true
    GameManager.Initialized = false

    -- 무기 발사 중지
    stopAllWeapons()

    -- 연출을 실행할 코루틴 주체 찾기
    local runner = GameManager.ManagerComponent or GameManager.PlayerScript

    if runner ~= nil and type(runner.StartCoroutine) == "function" then
        runner:StartCoroutine(function()
            -- 여기에 자유롭게 연출 코드를 추가할 수 있습니다.
            if resultType == "GameClear" then
                -- 게임 클리어 시 연출 (예: 0.2배속 슬로우 모션 2초)
                if GameManager.World ~= nil then GameManager.World:SetTimeDilation(0.2) end
                -- 코루틴 "wait_time"이 TimeDilation에 영향받는 것 고려, 2 * 0.2 = 0.4로 설정
                coroutine.yield("wait_time", 0.4)
                if GameManager.World ~= nil then GameManager.World:SetTimeDilation(1.0) end
            else
                -- 게임 오버 시 연출 (0.1배속 슬로우 모션 2초)
                Log("[GameManager] Starting GameOver Sequence")
                if GameManager.World ~= nil then GameManager.World:SetTimeDilation(0.1) end

                -- 플레이어 위치 가져오기
                local playerPos = { x = 0, y = 0, z = 0 }
                if GameManager.PlayerScript ~= nil then
                    local actor = GameManager.PlayerScript:GetActor()
                    if actor ~= nil and actor:IsValid() then
                        playerPos = actor:GetLocation()
                        Log("[GameManager] PlayerPos found: " .. tostring(playerPos.x) .. ", " .. tostring(playerPos.y))
                    else
                        Log("[GameManager] Player Actor is invalid or nil")
                    end
                else
                    Log("[GameManager] PlayerScript is nil")
                end

                local pool = nil
                if type(GetActorPoolManager) == "function" then
                    pool = GetActorPoolManager()
                else
                    Log("[GameManager] GetActorPoolManager function not found")
                end

                -- 0.2초 간격으로 10번 폭발 (Unscaled Time 기준)
                for i = 1, 10 do
                    Log("[GameManager] Spawning explosion " .. tostring(i))
                    -- 구형 범위 내 랜덤 위치 (최소 0.5, 최대 1.5)
                    local angle = math.random() * math.pi * 2.0
                    local phi = math.random() * math.pi
                    local dist = 0.5 + (math.random() * 1.0)

                    local spawnPos = {
                        x = playerPos.x + math.sin(phi) * math.cos(angle) * dist,
                        y = playerPos.y + math.sin(phi) * math.sin(angle) * dist,
                        z = playerPos.z + math.cos(phi) * dist + 1.5
                    }

                    local explosion = nil
                    if pool ~= nil and pool:IsValid() then
                        explosion = pool:Acquire("ASubUVVfxActor")
                    end

                    if explosion ~= nil and explosion:IsValid() then
                        Log("[GameManager] Explosion actor acquired and setting location")
                        explosion:SetLocation(spawnPos)
                        
                        -- 폭발 연출을 위한 옵션 초기화 (머신건 이펙트와 공유하므로 필요)
                        explosion:SetScale({1.0, 1.0, 1.0})
                        local subUV = explosion:GetComponentByName("USubUVComponent", "SubUV")
                        if subUV ~= nil and subUV:IsValid() then
                            subUV:SetFrameRate(240.0)
                        end
                        
                        explosion:SetVfxPreset("Explode")
                        -- 폭발 액터 수동 반납을 위한 지연 처리
                        runner:StartCoroutine(function()
                            -- 0.1배속 상황에서 1.5초 대기는 0.15초 yield
                            coroutine.yield("wait_time", 0.15)
                            if explosion:IsValid() and pool ~= nil and pool:IsValid() then
                                Log("[GameManager] Releasing explosion actor " .. tostring(i))
                                pool:Release(explosion)
                            end
                        end)
                    else
                        Log("[GameManager] Failed to acquire explosion actor " .. tostring(i))
                    end

                    -- 0.1배속 상황에서 0.2초 대기는 0.02초 yield
                    coroutine.yield("wait_time", 0.02)
                end

                -- 나머지 시간 대기 (총 2초 연출을 위해 추가 대기, 0.1배속이므로 0.1초 yield)
                coroutine.yield("wait_time", 0.1)
                
                if GameManager.World ~= nil then GameManager.World:SetTimeDilation(1.0) end
            end

            -- 연출 종료 후 최종 처리 호출
            GameManager.FinishRun(resultType, reason, finalRemainingTime, finalKillCount)
        end)
    else
        -- 코루틴을 실행할 수 없는 경우 즉시 종료 처리
        GameManager.FinishRun(resultType, reason, finalRemainingTime, finalKillCount)
    end
end

function GameManager.OnGameClear()
    GameManager.TimeRemaining = 0.0
    GameManager.StartFinishSequence("GameClear", "Time Cleared")
end

function GameManager.OnGameOver(reason)
    GameManager.StartFinishSequence("GameOver", reason or "Game Over")
end

function GameManager.ShowScoreBoard()
    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.ShowScoreBoard) == "function" then
        GameManager.ResultUI:ShowScoreBoard()
        return true
    end

    return false
end

function GameManager.ReturnToMainMenu()
    if GameManager.ResultUI ~= nil and type(GameManager.ResultUI.Hide) == "function" then
        GameManager.ResultUI:Hide()
    end

    cleanupGameplayActors()
    resetRunState()

    GameManager.SessionPrepared = false
    GameManager.GameStartRequested = false
    GameManager.Initialized = false
    GameManager.SetGameplayPaused(true)
    GameManager.StopBackgroundMusic(true)

    if GameManager.MainMenuController ~= nil and type(GameManager.MainMenuController.ShowMenu) == "function" then
        GameManager.MainMenuController:ShowMenu()
    end
end

return GameManager
