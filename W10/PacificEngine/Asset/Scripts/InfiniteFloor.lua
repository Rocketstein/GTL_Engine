--[[
    InfiniteFloor.lua
    플레이어 위치를 추적하여 Quad 메시를 타일링하는 무한 맵 시스템.
]]

local Script = {
    properties = {
        TileClassName = { type = "string", default = "AStaticMeshActor" },
        MeshPath = { type = "string", default = "Asset/Content/Models/_Basic/Quad.OBJ" },
        MaterialPath = { type = "string", default = "Asset/Content/Materials/FloorMaterial.json" },
        TileScale = { type = "vec3", default = { x = 25.0, y = 25.0, z = 1.0 } },
        Spacing = { type = "float", default = 25.0 },
        GridSize = { type = "int", default = 7 }, -- 7x7 grid
        TargetTag = { type = "string", default = "Player" }
    }
}

function Script:BeginPlay()
    Log("InfiniteFloor: BeginPlay")
    self.Tiles = {}
    self.PoolManager = GetActorPoolManager()
    self.PlayerActor = nil
    
    -- 타일 풀 미리 준비 (GridSize * GridSize)
    local totalTiles = self.GridSize * self.GridSize
    if self.PoolManager:IsValid() then
        self.PoolManager:Warmup(self.TileClassName, totalTiles)
        
        for i = 1, totalTiles do
            local tile = self.PoolManager:Acquire(self.TileClassName)
            if tile:IsValid() then
                -- 초기 설정: 메시와 머티리얼 적용
                local meshComp = tile:GetComponent("UStaticMeshComponent")
                if meshComp:IsValid() then
                    meshComp:SetStaticMesh(self.MeshPath)
                    meshComp:SetMaterial(0, self.MaterialPath)
                end
                
                tile:SetScale(self.TileScale)
                table.insert(self.Tiles, tile)
            end
        end
    end
    
    -- 초기 그리드 좌표
    self.LastGridX = -9999
    self.LastGridY = -9999
end

function Script:Tick(deltaTime)
    -- 플레이어 찾기 (없으면 매 틱 시도)
    if not self.PlayerActor or not self.PlayerActor:IsValid() then
        self.PlayerActor = self:QueryActorByTagClosest(self.TargetTag, {x=0, y=0, z=0}, 100000.0)
        if not self.PlayerActor:IsValid() then return end
    end

    local playerPos = self.PlayerActor:GetLocation()
    
    -- 플레이어 위치를 Spacing 단위로 양자화하여 현재 그리드 중심 계산
    local currentGridX = math.floor(playerPos.x / self.Spacing + 0.5)
    local currentGridY = math.floor(playerPos.y / self.Spacing + 0.5)

    -- 그리드 위치가 변경되었을 때만 타일 재배치
    if currentGridX ~= self.LastGridX or currentGridY ~= self.LastGridY then
        self:UpdateTilePositions(currentGridX, currentGridY)
        self.LastGridX = currentGridX
        self.LastGridY = currentGridY
    end
end

function Script:UpdateTilePositions(gridX, gridY)
    local halfSize = math.floor(self.GridSize / 2)
    local tileIdx = 1
    
    for x = -halfSize, halfSize do
        for y = -halfSize, halfSize do
            local tile = self.Tiles[tileIdx]
            if tile and tile:IsValid() then
                local worldX = (gridX + x) * self.Spacing
                local worldY = (gridY + y) * self.Spacing
                tile:SetLocation({ x = worldX, y = worldY, z = 0.0 })
            end
            tileIdx = tileIdx + 1
        end
    end
    -- Log("InfiniteFloor: Tiles repositioned to grid " .. gridX .. ", " .. gridY)
end

function Script:EndPlay()
    Log("InfiniteFloor: EndPlay")
    if self.PoolManager:IsValid() then
        for _, tile in ipairs(self.Tiles) do
            self.PoolManager:Release(tile)
        end
    end
    self.Tiles = {}
end

return Script
