local GameManager = require("Game.GameManager")
local AudioManager = require("Game.AudioManager")
local Engine = require("Common.Engine")
local UI = require("Common.UI")

---------------------------
-- Crash Log 연출
---------------------------

local crash_ui = nil		-- Crash Image UI
local ok_btn = nil			-- 확인 버튼
local is_crashed = false	-- Crash 여부 뜻하는 Bool 변수
local listener_key = nil	-- Game Manager Listener key

------------------------------------------------
-- 생명주기
------------------------------------------------

-- FakeCrash 연출에 필요한 UI 컴포넌트를 찾고,
-- CrashDump 분석 이벤트에 연출 트리거 hook을 1회 연결합니다
function BeginPlay()
	print("[FakeCrashEvent] BeginPlay 진입 owner=" .. tostring(obj and obj.Name))

	-- 디버그: obj가 들고 있는 OwnedComponents 이름을 전부 찍는다
	-- 이 액터에 실제로 어떤 컴포넌트가 등록돼 있는지 직접 확인해서
	-- crash_ui/ok_btn 매칭 실패 원인을 좁힌다
	if obj and obj.GetComponentByType then
		local img = Engine.GetComponentByType(obj, "UUIImageComponent")
		print("[FakeCrashEvent] GetComponentByType(UUIImageComponent) valid=" ..
			tostring(Engine.IsValidComponent(img)))
		local btn = Engine.GetComponentByType(obj, "UIButtonComponent")
		print("[FakeCrashEvent] GetComponentByType(UIButtonComponent) valid=" ..
			tostring(Engine.IsValidComponent(btn)))
	end

	-- PIE 진입 시 Editor->Game World 복제 과정에서 컴포넌트 ObjectName이 새로 생성되므로
	-- 이름 매칭(CrashAlertBox 등)에 의존하지 않고 type 매칭으로 잡는다.
	-- 같은 액터 안에 UUIImageComponent / UIButtonComponent가 1개만 있다는 전제이며,
	-- 여러 개가 필요해지면 별도 ScriptComponent를 두거나 상위 컴포넌트로 좁히는 식으로 분리한다.
	if obj and obj.GetComponentByType then
		local img = Engine.GetComponentByType(obj, "UUIImageComponent")
		if img then
			crash_ui = img
		end

		local btn = Engine.GetComponentByType(obj, "UIButtonComponent")
		if btn then
			ok_btn = btn
		end
	end

	-- 이름 매칭은 PIE에서 깨지지만, 에디터/Standalone에서 디버그 도움이 되므로 fallback으로 둔다.
	if not crash_ui then
		crash_ui = Engine.GetComponent(obj, "CrashAlertBox", "UUIImageComponent_36", "UUIImageComponent_0")
	end
	if not ok_btn then
		ok_btn = Engine.GetComponent(obj, "CrashOKButton", "UIButtonComponent_10", "UIButtonComponent_0")
	end

	if crash_ui then
		UI.SetTexture(crash_ui, "Asset/Content/Texture/CrashImage.png")
		UI.SetVisible(crash_ui, false)
		print("[FakeCrashEvent] crash_ui 확보 완료, 초기 visible=false")
	else
		warn("Crash 배경 이미지 컴포넌트를 찾을 수 없습니다.")
	end

	if ok_btn then
		UI.SetTexture(ok_btn, "Asset/Content/Texture/OK.png")
		UI.SetVisible(ok_btn, false)
		print("[FakeCrashEvent] ok_btn 확보 완료, 초기 visible=false")
	else
		warn("Crash OK 버튼 컴포넌트를 찾을 수 없습니다.")
	end

	-- CrashDumpItem 3개째에 Critical Analysis가 발동될 때만 FakeCrash 연출을 띄운다.
	if not GameManager.OnCrashDumpAnalyzed then
		warn("[FakeCrashEvent] GameManager.OnCrashDumpAnalyzed hook missing. Check GameManager.lua.")
	end
	-- BeginPlay가 hot-reload/재진입으로 여러 번 불려도 override가 N중 누적되지 않게
	-- 최초 1회 베이스 hook을 _G에 백업해 두고 매번 그 위에 한 겹만 얹는다.
	listener_key = "FakeCrashEvent_" .. tostring(obj and (obj.UUID or obj.Name) or "unknown")

	GameManager.RemoveCrashDumpAnalyzedListener(listener_key)

	GameManager.AddCrashDumpAnalyzedListener(listener_key, function()
	    -- 이 callback이 남아있더라도 owner가 죽었으면 즉시 제거되게 false 반환
	    if not Engine.IsValidActor(obj) then
	        return false
	    end

	    print("[FakeCrashEvent] OnCrashDumpAnalyzed listener fired")
	    TriggerFakeCrash()

	    return true
	end)

	print("[FakeCrashEvent] OnCrashDumpAnalyzed listener installed key=" .. tostring(listener_key))
end

------------------------------------------------
-- Fake Crash 연출 함수들
------------------------------------------------

function TriggerFakeCrash()
	print("[FakeCrashEvent] TriggerFakeCrash 호출 is_crashed=" .. tostring(is_crashed) ..
		" crash_ui=" .. tostring(crash_ui ~= nil))
	if is_crashed or not crash_ui then
		print("[FakeCrashEvent] TriggerFakeCrash 가드에 막힘 (이미 crash 중이거나 crash_ui nil)")
		return
	end
	is_crashed = true

	StartCoroutine("RunFakeCrashSequence")
end

-- CrashOKButton의 OnClickAction에 "OnCrashOKClicked"를 연결하여 호출됨
function OnCrashOKClicked()
	if is_crashed then
		is_crashed = false
	end
end

-- 게임을 잠시 멈추고 가짜 오류창 UI/사운드를 보여준 뒤,
-- 플레이어 입력을 받으면 UI를 닫고 게임과 BGM을 원래 흐름으로 복구합니다.
function RunFakeCrashSequence()
	print("[FakeCrashEvent] RunFakeCrashSequence 시작")
	GameManager.Pause()
	-- AudioManager.StopBGM은 내부 bgm_started 플래그도 false로 되돌리므로
	-- 이후 PlayBGM이 "already started" 가드에 걸리지 않고 다시 재생된다.
	-- stop_all_audio는 글로벌 사운드도 같이 끊기 위해 함께 호출한다.
	AudioManager.StopBGM()
	stop_all_audio()

	wait_frames(10)
	-- ResourceManager에 사운드 키가 등록돼 있지 않아 "Sound.SFX.*" 이름은 resolve에 실패한다.
	-- 새로 추가한 .wav/.mp3 파일이므로 RootDir 기준 상대경로를 직접 넘긴다.
	play_sfx("Asset/Content/Sound/SFX/windows-98-error.mp3", false)

	crash_ui:SetVisible(true)
	crash_ui:SetTint(1.0, 1.0, 1.0, 1.0)
	print("[FakeCrashEvent] crash_ui SetVisible(true) 호출")

	if ok_btn then
		ok_btn:SetVisible(true)
		ok_btn:SetTint(1.0, 1.0, 1.0, 1.0)
		print("[FakeCrashEvent] ok_btn SetVisible(true) 호출")
	end

	while is_crashed do
		-- 버튼 클릭 외에도 ESC나 Enter로 넘길 수 있도록 처리
		if GetKeyDown("ESCAPE") or GetKeyDown("ENTER") then
			is_crashed = false
			break
		end
		wait_frames(1)
	end

	crash_ui:SetVisible(false)
	if ok_btn then
		ok_btn:SetVisible(false)
	end

	play_sfx("Asset/Content/Sound/SFX/glitch_noise.wav", false)
	stop_all_audio()

	GameManager.Resume()
	-- BGM 재시작은 Resume 직후에 한다. StopBGM에서 플래그를 리셋했으므로 정상 재생된다.
	AudioManager.PlayBGM()
	print("[FakeCrashEvent] RunFakeCrashSequence 종료")
end


function EndPlay()
	if listener_key then
        GameManager.RemoveCrashDumpAnalyzedListener(listener_key)
        print("[FakeCrashEvent] listener removed key=" .. tostring(listener_key))
        listener_key = nil
    end

    is_crashed = false

    if crash_ui then
        crash_ui:SetVisible(false)
    end

    if ok_btn then
        ok_btn:SetVisible(false)
    end

    GameManager.Resume()
end