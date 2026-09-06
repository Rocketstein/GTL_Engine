---------------------------
-- Screen Animation 계수
---------------------------

local ScreenConsoleAnimatorConfig = {
    -- tint_*: 화면 전환 애니메이션에서 반복 사용되는 색상값입니다.
    tint_clear = { 0.0, 0.0, 0.0, 0.0 },
    tint_final = { 1.0, 1.0, 1.0, 1.0 },

    -- power_*: CRT 전원 켜짐 연출 tint 단계입니다.
    power_flash = { 0.42, 0.96, 1.0, 0.88 },
    power_dim = { 0.26, 0.72, 0.82, 0.22 },
    power_rise = { 0.76, 1.0, 1.0, 0.74 },
    power_flicker = { 0.36, 0.82, 0.92, 0.30 },

    -- swap_*: 화면 교체 기본 연출 tint 단계입니다.
    swap_flash = { 0.48, 0.92, 1.0, 0.66 },
    swap_hold = { 0.74, 1.0, 1.0, 0.84 },
    swap_flicker = { 0.58, 0.88, 0.96, 0.42 },
    swap_settle = { 0.90, 1.0, 1.0, 0.92 },
}

return ScreenConsoleAnimatorConfig
