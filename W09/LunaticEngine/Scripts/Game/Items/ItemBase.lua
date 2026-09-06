local GameManager = require("Game.GameManager")
local Engine = require("Common.Engine")

------------------------------------------------
-- ItemBase는 아이템 Lua 스크립트들이 공통으로 쓰는
-- OnPickedUp/GrantLogFragment/GrantCrashDump
------------------------------------------------
local ItemBase = {}
ItemBase.__index = ItemBase

-- EItemFeatureFlags와 같은 이름을 사용
local DefaultFeatures = {
    PickupOnOverlap = true,     -- 플레이어와 닿자마자 먹는 아이템인지
    LogFragmentReward = false,  -- logs/score/trace/coach approval을 한 번에 올리는 규칙
    CrashDumpReward = false,    -- dumps를 올리고 3개째에 Critical Analysis를 발동하는 규칙
    SingleUse = true,           -- Overlap 중복방지
    DebugLog = false,           -- 아이템 pickup 흐름 콘솔 확인 On/Off
}

------------------------------------------------
-- Item Feature 구성 함수들
------------------------------------------------

local function merge_features(features)
    -- item script가 일부 feature만 넘겨도 나머지는 안전한 기본값을 유지
    local merged = {}
    for key, value in pairs(DefaultFeatures) do
        merged[key] = value
    end

    if features then
        for key, value in pairs(features) do
            merged[key] = value == true
        end
    end

    return merged
end

------------------------------------------------
-- ItemBase 생성 / 설정 함수들
------------------------------------------------

function ItemBase.New(config)
    -- ItemBase는 기본 동작이 있는 Lua 객체
    -- LogItem.lua/CrashDumpItem.lua처럼 이 객체를 만들고 OnBeginOverlap만 위임하면 바로 pickup item
    config = config or {}
    local self = setmetatable({}, ItemBase)
    -- 단순 점수 아이템 보상
    self.ScoreValue = config.ScoreValue or 0
    -- 어떤 actor가 먹을 수 있는지 제한
    self.RequiredInteractorTag = config.RequiredInteractorTag or "Player"
    -- 키/클릭 상호작용 아이템에 쓸 값
    self.Cooldown = config.Cooldown or 0.0
    -- SingleUse 아이템이 중복 처리되지 않도록 막는 내부 상태
    self.bPicked = false
    -- 아이템 현재 pickup 가능 여부
    self.bEnabled = config.bStartsEnabled ~= false
    -- 이 아이템이 어떤 규칙을 쓸지 모아둔 플래그
    self.Features = merge_features(config.Features)
    return self
end

function ItemBase:Log(message)
    -- DebugLog feature가 켜진 아이템만 로그 출력
    if self:HasFeature("DebugLog") then
        print("[ItemBase] " .. tostring(message))
    end
end

-- feature 플래그를 안전하게 읽는 함수
function ItemBase:HasFeature(featureName)
    return self.Features and self.Features[featureName] == true
end

-- 아이템 규칙을 설정하는데 사용하는 함수
function ItemBase:SetFeatureEnabled(featureName, enabled)
    self.Features = self.Features or merge_features(nil)
    self.Features[featureName] = enabled == true
end

------------------------------------------------
-- Pickup 판정 / 상호작용 함수들
------------------------------------------------

-- Title/Game input policy가 바뀌어도 item pickup은 collision event만으로 동작
function ItemBase:OnBeginOverlap(otherActor, otherComp, selfComp)
    -- 픽업 가능한 상태인건지 체크
    if not self.bEnabled or not self:HasFeature("PickupOnOverlap") then
        return
    end

    -- overlap pair가 여러 프레임 유지되거나 begin이 중복 호출되어도 한 번만 처리
    if self:HasFeature("SingleUse") and self.bPicked then
        return
    end

    -- Overlap 이벤트가 발생한 actor가 tag를 가지고 있는지 체크
    if not self:IsValidInteractor(otherActor, otherComp, selfComp) then
        return
    end

    self.bPicked = true
    self:Interact(otherActor, otherComp, selfComp)
end

-- 특정 Tag를 가진 actor와 상호작용 가능하게 체크하는 함수
-- RequiredInteractorTag를 빈 문자열로 두면 모든 actor와 상호작용 가능
function ItemBase:IsValidInteractor(otherActor, otherComp, selfComp)
    if not Engine.IsValidActor(otherActor) then
        return false
    end

    if self.RequiredInteractorTag == nil or self.RequiredInteractorTag == "" then
        return true
    end

    return otherActor.HasTag and otherActor:HasTag(self.RequiredInteractorTag)
end

-- 공통 feature 순서
-- Lua는 보상만 처리
function ItemBase:Interact(otherActor, otherComp, selfComp)
    if self:HasFeature("LogFragmentReward") then
        self:GrantLogFragment(otherActor)
    end

    if self:HasFeature("CrashDumpReward") then
        self:GrantCrashDump(otherActor)
    end

    self:OnPickedUp(otherActor, otherComp, selfComp)
end

------------------------------------------------
-- Pickup 보상 Hook 함수들
------------------------------------------------

-- logs +1, score +10, trace +2
-- coach approval 상승은 GameManager에서 처리
-- trace 100%가 되면 여기서 Hotfix가 발동되고, 화면에 HOTFIX APPLIED 띄우면 됨.
function ItemBase:GrantLogFragment(otherActor)
    GameManager.CollectLogFragment()
    self:Log("GrantLogFragment actor=" .. tostring(otherActor and otherActor.Name))
end

function ItemBase:GrantCrashDump(otherActor)
    -- dumps가 3개가 되면 Critical Analysis가 발동되고 score/shield/coach approval이 올라갑니다.
    -- TODO: 여기서 화면에 CRITICAL ANALYSIS 연출 붙이면 됨.
    local result = GameManager.CollectCrashDump()
    self:Log(
        "GrantCrashDump actor=" .. tostring(otherActor and otherActor.Name) ..
        " critical=" .. tostring(result and result.critical_analysis_applied)
    )
end

function ItemBase:OnPickedUp(otherActor, otherComp, selfComp)
    -- item별 Lua script가 override해서 추가 보상을 넣는 지점입니다.
end

return ItemBase
