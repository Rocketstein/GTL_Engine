local CollectibleConfig = require("Game.Config.Collectible")
---------------------------
-- 2 종류 item 설정값
---------------------------
local ItemsConfig = {
    required_interactor_tag = "Player", -- 기본 아이템을 획득할 수 있는 actor tag

    -- trace/score를 올리는 Log Fragment 아이템 설정
    log_fragment = {
        score_value = CollectibleConfig.log_fragment_score,
        features = {
            PickupOnOverlap = true,
            ConsumeOnPickup = false,
            ScoreReward = false,
            LogFragmentReward = true,
            SingleUse = true,
            DebugLog = true,
        },
    },

    -- crash_dump: Critical Analysis 재료 아이템 설정입니다.
    crash_dump = {
        features = {
            PickupOnOverlap = true,
            ConsumeOnPickup = false,
            CrashDumpReward = true,
            SingleUse = true,
            DebugLog = true,
        },
    },
}

return ItemsConfig
