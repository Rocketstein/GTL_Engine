local ItemBase = require("Game.Items.ItemBase")
local ItemsConfig = require("Game.Config.Items")

local LogFragmentConfig = ItemsConfig.log_fragment

local item = ItemBase.New({
    ScoreValue = LogFragmentConfig.score_value,
    RequiredInteractorTag = ItemsConfig.required_interactor_tag,
    Features = LogFragmentConfig.features,
})

------------------------------------------------
-- Log Fragment 충돌 콜백 함수들
------------------------------------------------

function OnBeginOverlap(otherActor, otherComp, selfComp)
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end