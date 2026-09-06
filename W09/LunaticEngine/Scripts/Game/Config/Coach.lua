---------------------------
-- Coach 평가값 보관 함수
---------------------------

local CoachConfig = {
    -- initial_approval: 게임 시작 시 코치 평가 초기값입니다.
    initial_approval = 50,

    -- *_delta: 게임 이벤트별 코치 평가 변화량입니다.
    log_collected_delta = 1,        -- log 수집 시 평가 변화량
    hotfix_delta = 8,
    critical_analysis_delta = 10,   -- 크래시 분석 시 추가 점수
    player_hit_delta = -5,          -- 충돌 시 깎이는 인정도

    -- rank_thresholds: approval 점수 이상일 때 적용할 랭크 기준입니다.
    rank_thresholds = {
        { rank = "S", approval = 90 },
        { rank = "A", approval = 75 },
        { rank = "B", approval = 55 },
        { rank = "C", approval = 35 },
        { rank = "D", approval = 15 },
    },

    -- 기본 랭크입니다.
    fallback_rank = "F",
}

return CoachConfig
