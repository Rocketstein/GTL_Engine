---@meta CrashEngine

-- 이 파일은 Lua Language Server가 자동완성과 타입 힌트를 만들 때만 읽는 설명서입니다.
-- 게임 런타임에서는 로드하지 않습니다. 실제 구현은 C++의 ScriptSystem/ScriptComponent 바인딩에 있습니다.
-- Class Actor =========================================================
---@class Actor
local Actor = {}

---@return boolean
function Actor:IsValid() end

---@return string
function Actor:GetName() end

---@return string
function Actor:GetClassName() end

---@return Vec3
function Actor:GetLocation() end

---@param location Vec3
---@return boolean
function Actor:SetLocation(location) end

---@param delta Vec3
---@return boolean
function Actor:AddWorldOffset(delta) end

---@return Actor
function ScriptComponent:GetActor() end

---@return Vec3
function Actor:GetRotation() end

---@param rotation Vec3
---@return boolean
function Actor:SetRotation(rotation) end

---@return Vec3
function Actor:GetScale() end

---@param scale Vec3
---@return boolean
function Actor:SetScale(scale) end

---@return Vec3
function Actor:GetForward() end

---@return boolean
function Actor:IsVisible() end

---@param visible boolean
---@return boolean
function Actor:SetVisible(visible) end

---@return number
function Actor:GetCustomTimeDilation() end

---@param timeDilation number
function Actor:SetCustomTimeDilation(timeDilation) end
-- ====================================================================

-- Class World ========================================================
---@class World
local World = {}

---@return boolean
function World:IsValid() end

---@return boolean
function World:IsGameplayPaused() end

---@param paused boolean
function World:SetGameplayPaused(paused) end

---@return number
function World:GetTimeDilation() end

---@param timeDilation number
function World:SetTimeDilation(timeDilation) end

---@param tag string
---@return Actor[]
function World:GetActorsByTag(tag) end
-- ====================================================================

-- Class Component ====================================================
---@class Component
local Component = {}

---@return boolean
function Component:IsValid() end

---@return string
function Component:GetName() end

---@return string
function Component:GetClassName() end

---@param className string
---@return boolean
function Component:IsA(className) end

---@return Actor
function Component:GetOwner() end

---@return World
function Component:GetWorld() end

---@return boolean
function Component:IsActive() end

---@param active boolean
---@return boolean
function Component:SetActive(active) end

---@param className string
---@param index? integer
---@return Component
function Actor:GetComponent(className, index) end

---@param className? string
---@return Component[]
function Actor:GetComponents(className) end

---@param className string
---@param index? integer
---@return Component
function ScriptComponent:GetComponent(className, index) end

---@param className? string
---@return Component[]
function ScriptComponent:GetComponents(className) end

---@return Vec3
function Component:GetWorldLocation() end

---@param location Vec3
---@return boolean
function Component:SetWorldLocation(location) end

---@return Vec3
function Component:GetRelativeLocation() end

---@param location Vec3
---@return boolean
function Component:SetRelativeLocation(location) end

---@return Vec3
function Component:GetForwardVector() end

---@return boolean
function Component:IsVisible() end

---@param visible boolean
---@return boolean
function Component:SetVisibility(visible) end

---@return boolean
function Component:ShouldGenerateOverlapEvents() end

---@param generate boolean
---@return boolean
function Component:SetGenerateOverlapEvents(generate) end

---@param actor Actor
---@return boolean
function Component:IsOverlappingActor(actor) end

---@param component Component
---@return boolean
function Component:IsOverlappingComponent(component) end

---@return boolean
function Component:IsUIComponent() end

---@param renderSpace '"ScreenSpace"'|'"WorldSpace"'|'"Screen"'|'"World"'
---@return boolean
function Component:SetUIRenderSpace(renderSpace) end

---@param texturePath string
---@return boolean
function Component:SetUITexturePath(texturePath) end

---@param anchor Vec2
---@return boolean
function Component:SetUIAnchor(anchor) end

---@param anchor Vec2
---@return boolean
function Component:SetUIAnchorMin(anchor) end

---@param anchor Vec2
---@return boolean
function Component:SetUIAnchorMax(anchor) end

---@return Vec2
function Component:GetUIAnchorMin() end

---@return Vec2
function Component:GetUIAnchorMax() end

---@param position Vec2
---@return boolean
function Component:SetUIAnchoredPosition(position) end

---@return Vec2
function Component:GetUIAnchoredPosition() end

---@param size Vec2
---@return boolean
function Component:SetUISizeDelta(size) end

---@return Vec2
function Component:GetUISizeDelta() end

---@param size Vec2
---@return boolean
function Component:SetUIWorldSize(size) end

---@param billboard boolean
---@return boolean
function Component:SetUIBillboard(billboard) end

---@param pivot Vec2
---@return boolean
function Component:SetUIPivot(pivot) end

---@return Vec2
function Component:GetUIPivot() end

---@param degrees number
---@return boolean
function Component:SetUIRotationDegrees(degrees) end

---@return number
function Component:GetUIRotationDegrees() end

---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function Component:SetUITint(r, g, b, a) end

---@return table
function Component:GetUITint() end

---@param visible boolean
---@return boolean
function Component:SetUIVisibility(visible) end

---@return boolean
function Component:IsUIButton() end

---@return boolean
function Component:IsButtonInteractable() end

---@param interactable boolean
---@return boolean
function Component:SetButtonInteractable(interactable) end

---@return boolean
function Component:IsButtonHovered() end

---@return boolean
function Component:IsButtonPressed() end

---@return integer
function Component:GetButtonClickCount() end

---@param className string
---@return any
function Component:Cast(className) end
-- ====================================================================

-- Class ActorPoolManager ==============================================
---@class ActorPoolManager
local ActorPoolManager = {}

---@return boolean
function ActorPoolManager:IsValid() end

---@param className string
---@param count integer
function ActorPoolManager:Warmup(className, count) end

---@param className string
---@return Actor
function ActorPoolManager:Acquire(className) end

---@param actor Actor
function ActorPoolManager:Release(actor) end

---@param className string
---@return integer
function ActorPoolManager:GetActiveCount(className) end

---@return ActorPoolManager
function GetActorPoolManager() end
-- ====================================================================

-- Class Collider2D ====================================================
---@class Collider2D : Component
local Collider2D = {}

---@return Vec2
function Collider2D:GetShapeWorldLocation2D() end

---@return number
function Collider2D:GetCollisionPlaneZ() end

---@return Vec2
function Collider2D:GetBoxExtent() end

---@param extent Vec2
---@return boolean
function Collider2D:SetBoxExtent(extent) end

---@return number
function Collider2D:GetRadius() end

---@param radius number
---@return boolean
function Collider2D:SetRadius(radius) end
-- ====================================================================

---@class Vec3
---@field x number
---@field y number
---@field z number
---@field X number
---@field Y number
---@field Z number
---@field [integer] number

---@class Vec2
---@field x number
---@field y number
---@field X number
---@field Y number
---@field [integer] number

---@class LuaScriptPropertyDescriptor
---@field type "int"|"float"|"bool"|"string"|"vec3"
---@field default? integer|number|boolean|string|Vec3
---@field min? number
---@field max? number
---@field speed? number

---@class ScriptComponent
---@field properties? table<string, LuaScriptPropertyDescriptor>
local ScriptComponent = {}

---@param message string
function Log(message) end

---@class Input
Input = {}

---@param key integer
---@return boolean
function Input.GetKey(key) end

---@param key integer
---@return boolean
function Input.GetKeyDown(key) end

---@param key integer
---@return boolean
function Input.GetKeyUp(key) end

---@param axisName string
---@return number
function Input.GetAxis(axisName) end

---@param button integer
---@return boolean
function Input.GetMouseButton(button) end

---@param button integer
---@return boolean
function Input.GetMouseButtonDown(button) end

---@param button integer
---@return boolean
function Input.GetMouseButtonUp(button) end

---@return integer, integer
function Input.GetMousePosition() end

function ScriptComponent:BeginPlay() end

---@param deltaTime number
function ScriptComponent:Tick(deltaTime) end

function ScriptComponent:EndPlay() end

---@param func fun()
---@return integer
function ScriptComponent:start_coroutine(func) end

---@param id integer
---@return boolean
function ScriptComponent:stop_coroutine(id) end

---@return Vec3
function ScriptComponent:GetActorLocation() end

---@param location Vec3
---@return boolean
function ScriptComponent:SetActorLocation(location) end

---@param delta Vec3
---@return boolean
function ScriptComponent:AddActorWorldOffset(delta) end

---@return Vec3
function ScriptComponent:GetActorRotation() end

---@param rotation Vec3
---@return boolean
function ScriptComponent:SetActorRotation(rotation) end

---@return Vec3
function ScriptComponent:GetActorScale() end

---@param scale Vec3
---@return boolean
function ScriptComponent:SetActorScale(scale) end

---@return Vec3
function ScriptComponent:GetActorForward() end

---@return boolean
function ScriptComponent:IsActorVisible() end

---@param visible boolean
---@return boolean
function ScriptComponent:SetActorVisible(visible) end

---@return number
function ScriptComponent:GetCustomTimeDilation() end

---@param timeDilation number
function ScriptComponent:SetCustomTimeDilation(timeDilation) end

---@param ownerActor Actor
---@return boolean
function ScriptComponent:InitializeFor(ownerActor) end

---@param targetActor Actor
---@return boolean
function ScriptComponent:SetViewTarget(targetActor) end

---@param targetActor Actor
---@param blendTime number
---@param blendFunction? '"Linear"'|'"Cubic"'|'"EaseIn"'|'"EaseOut"'|'"EaseInOut"'
---@param blendExp? number
---@param lockOutgoing? boolean
---@return boolean
function ScriptComponent:SetViewTargetBlend(targetActor, blendTime, blendFunction, blendExp, lockOutgoing) end

function ScriptComponent:SetGameCameraCutThisFrame() end

---@return Vec3
function ScriptComponent:GetCameraLocation() end

---@return Vec3
function ScriptComponent:GetCameraRotation() end

---@return boolean
function ScriptComponent:HasValidCameraCache() end
