---@meta LunaticEngine

-- VSCode/LuaLS 자동완성 전용 stub
-- 런타임에서 require하지 않는다
-- 실제 구현은 C++ LuaScriptRuntime/LuaScriptInstance 바인딩에 있음

---@class Vector
---@field x number
---@field y number
---@field z number
---@operator add(Vector): Vector
---@operator sub(Vector): Vector
---@operator mul(number): Vector
---@operator div(number): Vector
local Vector = {}

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector.new(x, y, z) end

---@param x number
---@param y number
---@param z number
---@return Vector
function vec3(x, y, z) end

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector3(x, y, z) end

---@class GroundHit
---@field hit boolean
---@field location Vector
---@field normal Vector
---@field ground_z number
---@field distance number
---@field actor ActorProxy
---@field component ComponentProxy
local GroundHit = {}

---@class ActorProxy
---@field Name string
---@field UUID integer
---@field Tag string
---@field Velocity Vector
local ActorProxy = {}

---@return boolean
function ActorProxy:IsValid() end

---@return Vector
function ActorProxy:GetWorldLocation() end

---@param location Vector
function ActorProxy:SetWorldLocation(location) end

---@param x number
---@param y number
---@param z number
function ActorProxy:SetWorldLocationXYZ(x, y, z) end

---@return Vector
function ActorProxy:GetWorldRotation() end

---@param rotation Vector
function ActorProxy:SetWorldRotation(rotation) end

---@param x number
---@param y number
---@param z number
function ActorProxy:SetWorldRotationXYZ(x, y, z) end

---@return Vector
function ActorProxy:GetWorldScale() end

---@param scale Vector
function ActorProxy:SetWorldScale(scale) end

---@param x number
---@param y number
---@param z number
function ActorProxy:SetWorldScaleXYZ(x, y, z) end

---@param maxDistance number
---@param skinWidth number
---@return GroundHit
function ActorProxy:FindGround(maxDistance, skinWidth) end

---@param tag string
---@return boolean
function ActorProxy:HasTag(tag) end

---@param componentName string
---@return ComponentProxy
function ActorProxy:GetComponent(componentName) end

---@param typeName string
---@return ComponentProxy
function ActorProxy:GetComponentByType(typeName) end

---@return ComponentProxy
function ActorProxy:GetStaticMeshComponent() end

---@return ComponentProxy
function ActorProxy:GetScriptComponent() end

---@param delta Vector
function ActorProxy:AddWorldOffset(delta) end

---@param x number
---@param y number
---@param z number
function ActorProxy:AddWorldOffset(x, y, z) end

---@param location Vector
function ActorProxy:MoveTo(location) end

---@param x number
---@param y number
---@param z number
function ActorProxy:Translate(x, y, z) end

---@param location Vector
function ActorProxy:MoveTo(location) end

---@param x number
---@param y number
---@param z? number
function ActorProxy:MoveTo(x, y, z) end

---@param delta Vector
function ActorProxy:MoveBy(delta) end

---@param x number
---@param y number
---@param z? number
function ActorProxy:MoveBy(x, y, z) end

---@param target ActorProxy
function ActorProxy:MoveToActor(target) end

function ActorProxy:StopMove() end

---@return boolean
function ActorProxy:IsMoveDone() end

---@param speed number
function ActorProxy:SetMoveSpeed(speed) end

---@return number
function ActorProxy:GetMoveSpeed() end

---@return integer
function ActorProxy:GetDamage() end

---@param damage integer
---@return boolean
function ActorProxy:SetDamage(damage) end

function ActorProxy:PrintLocation() end

function ActorProxy:Destroy() end

---@class ComponentProxy
---@field Name string
---@field Owner ActorProxy
---@field TypeName string
local ComponentProxy = {}

---@return boolean
function ComponentProxy:IsValid() end

---@return string
function ComponentProxy:GetTypeName() end

---@param active boolean
---@return boolean
function ComponentProxy:SetActive(active) end

---@return boolean
function ComponentProxy:IsActive() end

---@param visible boolean
---@return boolean
function ComponentProxy:SetVisible(visible) end

---@return boolean
function ComponentProxy:IsVisible() end

---@return Vector|nil
function ComponentProxy:GetWorldLocation() end

---@param location Vector
---@return boolean
function ComponentProxy:SetWorldLocation(location) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetWorldLocationXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetLocalLocation() end

---@param location Vector
---@return boolean
function ComponentProxy:SetLocalLocation(location) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetLocalLocationXYZ(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:AddWorldOffset(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:AddWorldOffsetXYZ(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:AddLocalOffset(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:AddLocalOffsetXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetWorldRotation() end

---@param rotation Vector
---@return boolean
function ComponentProxy:SetWorldRotation(rotation) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetWorldRotationXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetLocalRotation() end

---@param rotation Vector
---@return boolean
function ComponentProxy:SetLocalRotation(rotation) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetLocalRotationXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetWorldScale() end

---@param scale Vector
---@return boolean
function ComponentProxy:SetWorldScale(scale) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetWorldScaleXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetLocalScale() end

---@param scale Vector
---@return boolean
function ComponentProxy:SetLocalScale(scale) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetLocalScaleXYZ(x, y, z) end

---@param enabled boolean
---@return boolean
function ComponentProxy:SetCollisionEnabled(enabled) end

---@param enabled boolean
---@return boolean
function ComponentProxy:SetGenerateOverlapEvents(enabled) end

---@param other ActorProxy
---@return boolean
function ComponentProxy:IsOverlappingActor(other) end

---@return '"Box"'|'"Sphere"'|'"Capsule"'|'"Unknown"'
function ComponentProxy:GetShapeType() end

---@return number|nil
function ComponentProxy:GetShapeHalfHeight() end

---@param halfHeight number
---@return boolean
function ComponentProxy:SetShapeHalfHeight(halfHeight) end

---@return number|nil
function ComponentProxy:GetShapeRadius() end

---@param radius number
---@return boolean
function ComponentProxy:SetShapeRadius(radius) end

---@return Vector|nil
function ComponentProxy:GetShapeExtent() end

---@param extent Vector
---@return boolean
function ComponentProxy:SetShapeExtent(extent) end

---@param meshPath string 현재 프로젝트의 OBJ/StaticMesh 경로 문자열을 사용합니다.
---@return boolean
function ComponentProxy:SetStaticMesh(meshPath) end

---@param text string
---@return boolean
function ComponentProxy:SetText(text) end

---@return string|nil
function ComponentProxy:GetText() end

---@return Vector|nil
function ComponentProxy:GetScreenPosition() end

---@param position Vector
---@return boolean
function ComponentProxy:SetScreenPosition(position) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetScreenPositionXYZ(x, y, z) end

---@return Vector|nil
function ComponentProxy:GetScreenSize() end

---@param size Vector
---@return boolean
function ComponentProxy:SetScreenSize(size) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:SetScreenSizeXYZ(x, y, z) end

---@param texturePath string
---@return boolean
function ComponentProxy:SetTexture(texturePath) end

---@return string|nil
function ComponentProxy:GetTexturePath() end

---@param tint Vector
---@return boolean
function ComponentProxy:SetTint(tint) end

---@param r number
---@param g number
---@param b number
---@param a number
---@return boolean
function ComponentProxy:SetTint(r, g, b, a) end

---@param label string
---@return boolean
function ComponentProxy:SetLabel(label) end

---@return string|nil
function ComponentProxy:GetLabel() end

---@return boolean
function ComponentProxy:IsHovered() end

---@return boolean
function ComponentProxy:IsPressed() end

---@return boolean
function ComponentProxy:WasClicked() end

---@param audioPath string
---@return boolean
function ComponentProxy:SetAudioPath(audioPath) end

---@return string|nil
function ComponentProxy:GetAudioPath() end

---@param category '"sfx"'|'"background"'|'"bgm"'
---@return boolean
function ComponentProxy:SetAudioCategory(category) end

---@return string|nil
function ComponentProxy:GetAudioCategory() end

---@param looping boolean
---@return boolean
function ComponentProxy:SetAudioLooping(looping) end

---@return boolean
function ComponentProxy:IsAudioLooping() end

---@param audioPath? string
---@return boolean
function ComponentProxy:PlayAudio(audioPath) end

---@return boolean
function ComponentProxy:StopAudio() end

---@return boolean
function ComponentProxy:PauseAudio() end

---@return boolean
function ComponentProxy:ResumeAudio() end

---@return boolean
function ComponentProxy:IsAudioPlaying() end

---@param speed number
---@return boolean
function ComponentProxy:SetSpeed(speed) end

---@return number|nil
function ComponentProxy:GetSpeed() end

---@param target Vector
---@return boolean
function ComponentProxy:MoveTo(target) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:MoveTo(x, y, z) end

---@param delta Vector
---@return boolean
function ComponentProxy:MoveBy(delta) end

---@param x number
---@param y number
---@param z number
---@return boolean
function ComponentProxy:MoveBy(x, y, z) end

---@return boolean
function ComponentProxy:StopMove() end

---@return boolean
function ComponentProxy:IsMoveDone() end

---@type ActorProxy
obj = nil

---@param ... any
function log(...) end

---@param ... any
function warn(...) end

---@param ... any
function error_log(...) end

---@param ... any
function print(...) end

---@return number
function time() end

---@return number
function delta_time() end

---@param keyName string
---@return boolean
function GetKey(keyName) end

---@param keyName string
---@return boolean
function GetKeyDown(keyName) end

---@param keyName string
---@return boolean
function GetKeyUp(keyName) end

---@class InputAPI
Input = {}

---@param keyName string
---@return boolean
function Input.GetKey(keyName) end

---@param keyName string
---@return boolean
function Input.GetKeyDown(keyName) end

---@param keyName string
---@return boolean
function Input.GetKeyUp(keyName) end

---@return number
function GetMouseDeltaX() end

---@return number
function GetMouseDeltaY() end

---@return number
function GetMouseWheel() end

---@return boolean
function MouseMoved() end

---@param buttonName string
---@return boolean
function IsDragging(buttonName) end

---@param buttonName string
---@return number
function GetDragDeltaX(buttonName) end

---@param buttonName string
---@return number
function GetDragDeltaY(buttonName) end

---@param buttonName string
---@return number
function GetDragDistance(buttonName) end

---@param entry string|function
---@return boolean
function StartCoroutine(entry) end

---@param seconds number
function wait(seconds) end

---@param seconds number
function Wait(seconds) end

-- wait_real/wait_signal/signal은 LuaCoroutineScheduler가 ScriptInstance 환경에 바인딩합니다.
-- yield API는 StartCoroutine 안에서만 호출해야 합니다.
---@param seconds number
function wait_real(seconds) end

---@param seconds number
function WaitReal(seconds) end

---@param frames integer
function wait_frames(frames) end

function wait_until_move_done() end

---@param keyName string
function wait_key_down(keyName) end

---@param name string
function wait_signal(name) end

---@param name string
function signal(name) end

---@param className string
---@param location Vector
---@return ActorProxy
function spawn_actor(className, location) end

---@param actorName string
---@return ActorProxy
function find_actor(actorName) end

---@param uuid integer
---@return ActorProxy
function find_actor_by_uuid(uuid) end

---@param tag string
---@return ActorProxy
function find_actor_by_tag(tag) end

---@param tag string
---@return ActorProxy[] ipairs로 순회 가능한 Lua table입니다.
function find_actors_by_tag(tag) end

---@param actor ActorProxy
function destroy_actor(actor) end

---@param soundPath string
---@param looping? boolean
---@return string
function play_sfx(soundPath, looping) end

---@param soundPath string
---@param looping? boolean
---@return string
function play_bgm(soundPath, looping) end

---@param filePath string
---@return table|nil
function load_json_file(filePath) end

---@param filePath string
---@param data table
---@return boolean
function save_json_file(filePath, data) end

---@param score integer
---@return boolean
function open_score_save_popup(score) end

---@return string|nil
function consume_score_save_popup_result() end

---@param message string
---@return boolean
function open_message_popup(message) end

---@return boolean
function consume_message_popup_ok() end

---@param filePath string
---@return boolean
function open_scoreboard_popup(filePath) end

---@return boolean
function open_title_options_popup() end

---@return boolean
function open_title_credits_popup() end

---@return boolean
function request_exit_game() end

---@param handle string
---@return boolean
function stop_audio_by_handle(handle) end

---@param handle string
---@return boolean
function pause_audio_by_handle(handle) end

---@param handle string
---@return boolean
function resume_audio_by_handle(handle) end

---@param handle string
---@return boolean
function is_audio_playing_by_handle(handle) end

---@return boolean
function stop_bgm() end

---@return boolean
function pause_bgm() end

---@return boolean
function resume_bgm() end

---@return boolean
function is_bgm_playing() end

function BeginPlay() end

---@param dt number
function Tick(dt) end

function EndPlay() end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
function OnBeginOverlap(otherActor, otherComp, selfComp) end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
function OnEndOverlap(otherActor, otherComp, selfComp) end

---@param otherActor ActorProxy
---@param otherComp ComponentProxy
---@param selfComp ComponentProxy
---@param impactLocation Vector
---@param impactNormal Vector
function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal) end

-- TODO: InputMapping에서 발생한 ActionValue를 ScriptComponent까지 전달하는 정식 경로가 필요합니다.
-- 현재 C++에는 CallLuaInputAction 호출 함수만 있으며, 엔진 입력 루프와의 연결은 아직 정식 완료 상태가 아닙니다.
---@param actionName string
---@param value Vector
---@param scalar number
function OnInputAction(actionName, value, scalar) end
