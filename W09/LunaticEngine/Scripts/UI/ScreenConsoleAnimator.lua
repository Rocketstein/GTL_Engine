local UI = require("Common.UI")
local AnimatorConfig = require("Game.Config.ScreenConsoleAnimator")

local ScreenConsoleAnimator = {}

local TINT_CLEAR = AnimatorConfig.tint_clear
local TINT_FINAL = AnimatorConfig.tint_final

local POWER_FLASH = AnimatorConfig.power_flash
local POWER_DIM = AnimatorConfig.power_dim
local POWER_RISE = AnimatorConfig.power_rise
local POWER_FLICKER = AnimatorConfig.power_flicker

local SWAP_FLASH = AnimatorConfig.swap_flash
local SWAP_HOLD = AnimatorConfig.swap_hold
local SWAP_FLICKER = AnimatorConfig.swap_flicker
local SWAP_SETTLE = AnimatorConfig.swap_settle

------------------------------------------------
-- Screen Console 애니메이터 함수들
------------------------------------------------

function ScreenConsoleAnimator.new(base_component, overlay_component)
    local self = {
        base = base_component,
        overlay = overlay_component,
        current_texture = nil,
        busy = false,
    }

    function self:is_busy()
        return self.busy
    end

    function self:is_idle()
        return not self.busy
    end

    function self:reset_to_off(off_texture, preload_texture)
        self.current_texture = off_texture

        UI.SetVisible(self.base, true)
        UI.SetVisible(self.overlay, true)

        UI.SetTexture(self.base, off_texture)
        UI.SetTint(self.base, TINT_FINAL)

        if preload_texture then
            UI.SetTexture(self.overlay, preload_texture)
        end
        UI.SetTint(self.overlay, TINT_CLEAR)
    end

    function self:play_power_on(target_texture)
        if self.busy then
            return false
        end

        self.busy = true
        UI.SetVisible(self.base, true)
        UI.SetVisible(self.overlay, true)
        UI.SetTexture(self.overlay, target_texture)
        UI.SetTint(self.overlay, TINT_CLEAR)

        wait(0.20)
        UI.AnimateTint(self.overlay, TINT_CLEAR, POWER_FLASH, 0.04, 3)
        UI.AnimateTint(self.overlay, POWER_FLASH, POWER_DIM, 0.06, 4)
        UI.AnimateTint(self.overlay, POWER_DIM, POWER_RISE, 0.05, 4)
        UI.AnimateTint(self.overlay, POWER_RISE, POWER_FLICKER, 0.04, 3)
        UI.AnimateTint(self.overlay, POWER_FLICKER, TINT_FINAL, 0.11, 6)

        self.current_texture = target_texture
        UI.SetTexture(self.base, self.current_texture)
        UI.SetTint(self.base, TINT_FINAL)
        UI.SetTint(self.overlay, TINT_CLEAR)
        self.busy = false
        return true
    end

    function self:play_signal_swap(target_texture)
        return self:play_screen_transition(target_texture, "console")
    end

    function self:play_screen_transition(target_texture, style)
        if self.busy then
            return false
        end

        local transition_style = style or "console"
        self.busy = true
        UI.SetVisible(self.base, true)
        UI.SetVisible(self.overlay, true)
        UI.SetTexture(self.overlay, target_texture)
        UI.SetTint(self.overlay, TINT_CLEAR)

        if transition_style == "soft" then
            UI.AnimateTint(self.overlay, TINT_CLEAR, { 0.72, 0.92, 1.0, 0.42 }, 0.06, 4)
            UI.AnimateTint(self.overlay, { 0.72, 0.92, 1.0, 0.42 }, { 0.94, 1.0, 1.0, 0.84 }, 0.08, 5)
            UI.AnimateTint(self.overlay, { 0.94, 1.0, 1.0, 0.84 }, TINT_FINAL, 0.08, 5)
        elseif transition_style == "glitch" then
            UI.AnimateTint(self.overlay, TINT_CLEAR, { 0.38, 0.88, 1.0, 0.60 }, 0.03, 2)
            UI.AnimateTint(self.overlay, { 0.38, 0.88, 1.0, 0.60 }, { 0.82, 1.0, 1.0, 0.28 }, 0.03, 2)
            UI.AnimateTint(self.overlay, { 0.82, 1.0, 1.0, 0.28 }, { 0.46, 0.80, 0.92, 0.72 }, 0.04, 3)
            UI.AnimateTint(self.overlay, { 0.46, 0.80, 0.92, 0.72 }, TINT_FINAL, 0.09, 5)
        else
            UI.AnimateTint(self.overlay, TINT_CLEAR, SWAP_FLASH, 0.05, 4)
            UI.AnimateTint(self.overlay, SWAP_FLASH, SWAP_HOLD, 0.06, 4)
            UI.AnimateTint(self.overlay, SWAP_HOLD, SWAP_FLICKER, 0.04, 3)
            UI.AnimateTint(self.overlay, SWAP_FLICKER, SWAP_SETTLE, 0.05, 3)
            UI.AnimateTint(self.overlay, SWAP_SETTLE, TINT_FINAL, 0.08, 5)
        end

        self.current_texture = target_texture
        UI.SetTexture(self.base, self.current_texture)
        UI.SetTint(self.base, TINT_FINAL)
        UI.SetTint(self.overlay, TINT_CLEAR)
        self.busy = false
        return true
    end

    return self
end

return ScreenConsoleAnimator
