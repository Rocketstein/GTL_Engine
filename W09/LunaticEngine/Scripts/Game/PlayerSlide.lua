local Engine = require("Common.Engine")
local Vector = require("Common.Vector")

------------------------------------------------------------------
-- PlayerSlide.lua
-- Runner 슬라이드 State Machine
--
-- PlayerController.lua는 입력, 이동, 생명주기 담당
-- 슬라이드 중 바뀌는 collision shape / mesh transform은 여기서 관리
------------------------------------------------------------------

local PlayerSlide = {}
PlayerSlide.__index = PlayerSlide


------------------------------------------------
-- PlayerSlide 생성 함수들
------------------------------------------------

function PlayerSlide.new(actor)
    -- PlayerSlide 객체는 한 Runner actor의 슬라이드 중 collision/mesh 변경값을 기억합니다.
    -- PlayerController가 입력을 판단하고, 실제 모양 변경은 여기 Begin/End만 호출하면 됩니다.
    return setmetatable({
        actor = actor,                      -- 슬라이드를 적용할 Runner Actor
        is_sliding = false,                 -- 현재 슬라이드 중 상태값
        collision_shape = nil,              -- 슬라이드 때 줄일 shape component
        normal_shape_extent = nil,          -- 평상시 collision 크기
        slide_shape_extent = nil,           -- 슬라이드 중 collision 크기
        slide_shape_z_delta = 0.0,          -- collision을 줄인 만큼 바닥에 붙여두기 위한 Z 보정값
        mesh = nil,                         -- 슬라이드 중 시각 높이를 줄일 static mesh
        normal_mesh_local_scale = nil,      -- 평상시 mesh scale
        slide_mesh_local_scale = nil,       -- 슬라이드 중 mesh scale
        normal_mesh_local_location = nil,   -- 평상시 mesh local 위치
        slide_mesh_local_location = nil     -- 슬라이드 중 mesh local 위치
    }, PlayerSlide)
end

------------------------------------------------
-- PlayerSlide 초기화 함수들
------------------------------------------------

-- PlayerController.lua의 BeginPlay 에서 호출.
-- 슬라이드 중 사용할 collision 크기와 mesh transform을 미리 계산해 둡니다.
-- Begin/End에서는 여기서 저장한 값만 적용/복구해서 런타임 계산을 단순하게 유지합니다.
function PlayerSlide:Configure()
    self.collision_shape = Engine.FindCollisionShape(self.actor)

    if not Engine.IsValidComponent(self.collision_shape) then
        warn("[PlayerSlide] Collision shape not found. Slide collision resize disabled.")
        self.collision_shape = nil
        self.normal_shape_extent = nil
        self.slide_shape_extent = nil
        self.slide_shape_z_delta = 0.0
    else
        if self.collision_shape.SetCollisionEnabled then
            self.collision_shape:SetCollisionEnabled(true)
        end
        if self.collision_shape.SetGenerateOverlapEvents then
            self.collision_shape:SetGenerateOverlapEvents(true)
        end

        if self.collision_shape.GetShapeExtent then
            self.normal_shape_extent = self.collision_shape:GetShapeExtent()
        else
            self.normal_shape_extent = nil
        end

        if self.normal_shape_extent then
            local normal_x = Vector.GetX(self.normal_shape_extent)
            local normal_y = Vector.GetY(self.normal_shape_extent)
            local normal_z = Vector.GetZ(self.normal_shape_extent)

            print("[slide Shape X]" .. normal_x)
            print("[slide Shape Y]" .. normal_y)
            print("[slide Shape Z]" .. normal_z)

            self.slide_shape_extent = Vector.Make(
                normal_x,
                normal_y,
                normal_z * 0.5
            )

            if self.slide_shape_extent then
                -- ShapeExtent.z는 box/capsule 계열에서 half-height로 본다.
                -- 슬라이드 때 줄어든 half-height만큼 아래로 내려야 바닥면이 그대로 유지된다.
                self.slide_shape_z_delta = normal_z - Vector.GetZ(self.slide_shape_extent)
            else
                warn("[PlayerSlide] Failed to create slide_shape_extent.")
                self.slide_shape_z_delta = 0.0
            end
        else
            warn("[PlayerSlide] GetShapeExtent returned nil. Slide collision resize disabled.")
            self.slide_shape_extent = nil
            self.slide_shape_z_delta = 0.0
        end

        print(
            "[TempleRun] Collision configured. Shape=" ..
            tostring(self.collision_shape.GetShapeType and self.collision_shape:GetShapeType() or "Unknown") ..
            ", NormalExtent=(" ..
            tostring(Vector.GetX(self.normal_shape_extent)) .. ", " ..
            tostring(Vector.GetY(self.normal_shape_extent)) .. ", " ..
            tostring(Vector.GetZ(self.normal_shape_extent)) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    end

    self.mesh = Engine.GetStaticMeshComponent(self.actor)
    if not Engine.IsValidComponent(self.mesh) then
        warn("[PlayerSlide] StaticMeshComponent not found. Slide visual scale disabled.")
        self.mesh = nil
        self.normal_mesh_local_scale = nil
        self.slide_mesh_local_scale = nil
        self.normal_mesh_local_location = nil
        self.slide_mesh_local_location = nil
        return
    end

    if self.mesh.GetLocalScale then
        self.normal_mesh_local_scale = self.mesh:GetLocalScale()
    end

    if self.mesh.GetLocalLocation then
        self.normal_mesh_local_location = self.mesh:GetLocalLocation()
    end

    if self.normal_mesh_local_scale then
        self.slide_mesh_local_scale = Vector.Make(
            Vector.GetX(self.normal_mesh_local_scale),
            Vector.GetY(self.normal_mesh_local_scale),
            Vector.GetZ(self.normal_mesh_local_scale) * 0.5
        )
    else
        warn("[PlayerSlide] GetLocalScale returned nil. Slide visual scale disabled.")
        self.slide_mesh_local_scale = nil
    end

    if self.normal_mesh_local_location then
        -- Static mesh scale은 pivot/center 기준으로 줄어든다.
        -- 그래서 시각 높이 감소분의 절반만큼 local Z를 내려야 mesh가 바닥에서 뜨지 않는다.
        self.slide_mesh_local_location = Vector.Make(
            Vector.GetX(self.normal_mesh_local_location),
            Vector.GetY(self.normal_mesh_local_location),
            Vector.GetZ(self.normal_mesh_local_location)
        )
    else
        warn("[PlayerSlide] GetLocalLocation returned nil. Slide visual location restore disabled.")
        self.slide_mesh_local_location = nil
    end

    if self.slide_mesh_local_scale then
        print(
            "[TempleRun] Mesh slide configured. NormalScaleZ=" ..
            tostring(Vector.GetZ(self.normal_mesh_local_scale)) ..
            ", SlideScaleZ=" ..
            tostring(Vector.GetZ(self.slide_mesh_local_scale))
        )
    end
end

------------------------------------------------
-- PlayerSlide 상태 조회 함수들
------------------------------------------------

function PlayerSlide:GetCollisionShape()
    -- PlayerController의 바닥 높이 계산이 현재 collision half-height를 읽을 때 쓰는 getter입니다.
    if Engine.IsValidComponent(self.collision_shape) then
        return self.collision_shape
    end

    return nil
end

function PlayerSlide:IsSliding()
    -- 슬라이드 중 Begin이 반복 호출되지 않게 PlayerController가 이 함수로 상태를 확인합니다.
    return self.is_sliding
end

------------------------------------------------
-- PlayerSlide 제어 함수들
------------------------------------------------

function PlayerSlide:Begin()
    -- 슬라이드는 duration 기반 타이머가 아니라 누르고 있는 동안 유지하는 방식입니다.
    -- 이 함수는 슬라이드 시작 1회만 호출되어야 하며, 반복 호출은 바로 무시합니다.
    if self.is_sliding then
        return
    end

    self.is_sliding = true

    if Engine.IsValidComponent(self.collision_shape) and self.slide_shape_extent and self.collision_shape.SetShapeExtent then
        self.collision_shape:SetShapeExtent(self.slide_shape_extent)

        -- Runner의 collision box는 현재 root component다.
        -- 저장해 둔 local location으로 되돌리면 달리던 actor의 X/Y까지 예전 위치로 튈 수 있다.
        -- 그래서 현재 위치 기준 AddLocalOffset만 사용한다.
        if self.slide_shape_z_delta ~= 0.0 and self.collision_shape.AddLocalOffset then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, -self.slide_shape_z_delta))
        end

        print(
            "[TempleRun] Slide collision extent=(" ..
            tostring(Vector.GetX(self.slide_shape_extent)) .. ", " ..
            tostring(Vector.GetY(self.slide_shape_extent)) .. ", " ..
            tostring(Vector.GetZ(self.slide_shape_extent)) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    end

    if Engine.IsValidComponent(self.mesh) then
        if self.slide_mesh_local_scale and self.mesh.SetLocalScale then
            self.mesh:SetLocalScale(self.slide_mesh_local_scale)
        end
        if self.slide_mesh_local_location and self.mesh.SetLocalLocation then
            self.mesh:SetLocalLocation(self.slide_mesh_local_location)
        end
    end

    print("[TempleRun] Input: Slide")
end

function PlayerSlide:End()
    -- End는 키를 떼거나 공중으로 뜬 순간 호출됩니다.
    -- 실제 복구는 Restore가 담당해서 EndPlay에서도 같은 복구 로직을 재사용합니다.
    if not self.is_sliding then
        return
    end

    self:Restore()
    print("[TempleRun] Slide end")
end

function PlayerSlide:Restore()
    -- Restore는 collision/mesh를 평상시 값으로 되돌립니다.
    -- GameOver/EndPlay에서도 호출되므로 nil 체크를 충분히 유지합니다.
    local was_sliding = self.is_sliding

    if Engine.IsValidComponent(self.collision_shape) and self.normal_shape_extent and self.collision_shape.SetShapeExtent then
        self.collision_shape:SetShapeExtent(self.normal_shape_extent)
        if was_sliding and self.slide_shape_z_delta ~= 0.0 and self.collision_shape.AddLocalOffset then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, self.slide_shape_z_delta))
        end
    end

    if Engine.IsValidComponent(self.mesh) then
        if self.normal_mesh_local_scale and self.mesh.SetLocalScale then
            self.mesh:SetLocalScale(self.normal_mesh_local_scale)
        end
        if self.normal_mesh_local_location and self.mesh.SetLocalLocation then
            self.mesh:SetLocalLocation(self.normal_mesh_local_location)
        end
    end

    self.is_sliding = false
end

return PlayerSlide
