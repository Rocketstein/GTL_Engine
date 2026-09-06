local ItemBase = require("Game.Items.ItemBase")
local ItemsConfig = require("Game.Config.Items")

local item = ItemBase.New({
    RequiredInteractorTag = ItemsConfig.required_interactor_tag,
    Features = ItemsConfig.crash_dump.features,
})

------------------------------------------------
-- Crash Dump 충돌 콜백 함수
------------------------------------------------

function OnBeginOverlap(otherActor, otherComp, selfComp)
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end
