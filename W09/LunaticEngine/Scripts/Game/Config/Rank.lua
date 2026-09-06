---------------------------
-- Rank 관련 resource
---------------------------

local RankConfig = {
    -- default_texture: 알 수 없는 랭크에 사용할 기본 배지 텍스처입니다.
    default_texture = "Asset/Content/Texture/UI/rank_c.png",

    -- texture_by_code: 랭크 코드별 배지 텍스처입니다.
    texture_by_code = {
        s = "Asset/Content/Texture/UI/rank_s.png",
        a = "Asset/Content/Texture/UI/rank_a.png",
        b = "Asset/Content/Texture/UI/rank_b.png",
        c = "Asset/Content/Texture/UI/rank_c.png",
        d = "Asset/Content/Texture/UI/rank_c.png",
        f = "Asset/Content/Texture/UI/rank_f.png",
    },
}

return RankConfig
