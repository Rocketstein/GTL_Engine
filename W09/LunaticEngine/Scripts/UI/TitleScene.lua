local Engine = require("Common.Engine")
local UI = require("Common.UI")
local story_scene_loading = false
local loading_overlay = nil
local loading_text = nil
local start_button = nil
local score_button = nil
local options_button = nil
local credit_button = nil
local exit_button = nil
local logo_image = nil

------------------------------------------------
-- TitleScene 버튼 찾기 함수들
------------------------------------------------

function BeginPlay()
    loading_overlay = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "LoadingOverlay")
    loading_text = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "LoadingText")
    start_button = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "UIButtonComponent_0")
    score_button = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "ScoreButton")
    options_button = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "OptionsButton")
    credit_button = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "CreditButton")
    exit_button = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "ExitButton")
    logo_image = Engine.GetRequiredComponent(obj, "TitleScene 컴포넌트를 찾을 수 없습니다:", "UUIImageComponent_0")

    UI.SetVisible(loading_overlay, false)
    UI.SetVisible(loading_text, false)
end

------------------------------------------------
-- TitleScene 버튼 액션 함수들
------------------------------------------------

function ShowScoreboard()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return open_scoreboard_popup("Saves/scoreboard.json")
end

function ShowOptions()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return open_title_options_popup()
end

function ShowCredits()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return open_title_credits_popup()
end

function ExitGame()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return request_exit_game()
end

function StartStoryScene()
    if story_scene_loading then
        return false
    end

    story_scene_loading = true
    stop_bgm()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return load_scene("game/story.scene")
end

------------------------------------------------
-- TitleScene 프레임 갱신 함수들
------------------------------------------------

function Tick(dt)
    if story_scene_loading then
        return
    end

    if start_button and start_button:WasClicked() then
        StartStoryScene()
        return
    end

    if score_button and score_button:WasClicked() then
        ShowScoreboard()
        return
    end

    if options_button and options_button:WasClicked() then
        ShowOptions()
        return
    end

    if credit_button and credit_button:WasClicked() then
        ShowCredits()
        return
    end

    if exit_button and exit_button:WasClicked() then
        ExitGame()
        return
    end
end
