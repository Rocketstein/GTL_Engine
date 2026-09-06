local DialogueConfig = require("Game.Config.Dialogue")

---------------------------
-- 결과 화면에 필요한 resource
---------------------------

local ResultScreenConfig = {
    -- scene_path: GameOver 후 이동할 결과 화면 씬입니다.
    scene_path = "game/gameresult.scene",

    -- title: 결과 화면 상단 제목입니다.
    title = "DEBUG SESSION RESULT",

    -- dialogue_path: 결과 랭크별 코치 코멘트 JSON 경로입니다.
    dialogue_path = DialogueConfig.result_path,

    -- coach_*: 결과 화면 fallback 코치 이름과 기본 코멘트입니다.
    coach_name_baek = "백 사령관",
    coach_name_lim = "임 오퍼레이터",
    coach_comment_baek = "판단은 정확했다. 다음엔 더 깊이 들어가라.",
    coach_comment_lim = "나쁘지 않다. 이제 버그가 널 좀 무서워하겠군.",

    -- preview_result: 에디터에서 결과 화면만 열었을 때 표시할 샘플 데이터입니다.
    preview_result = {
        reason = "EditorPreview",
        score = 128450,
        logs = 24,
        trace = 88,
        dumps = 2,
        hotfix_count = 3,
        critical_analysis_count = 1,
        distance = 1842.0,
        elapsed_time = 153.4,
        stability = 2,
        max_stability = 3,
        coach_approval = 84,
        coach_rank = "A",
        shield_count = 1,
    },
}

return ResultScreenConfig
