------------------------------------------------
-- 기본 생명주기 함수
------------------------------------------------

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
end

-- dt 프레임 delta time
function Tick(dt)
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

------------------------------------------------
-- 기본 충돌 콜백 함수
------------------------------------------------

-- otherActor : 충돌 또는 overlap 대상 Actor
-- otherComp : 충돌 또는 overlap 대상 컴포넌트
-- selfComp : 이 스크립트가 붙은 컴포넌트
function OnBeginOverlap(otherActor, otherComp, selfComp)
end

-- otherActor : 충돌 또는 overlap 대상 Actor
-- otherComp : 충돌 또는 overlap 대상 컴포넌트
-- selfComp : 이 스크립트가 붙은 컴포넌트
function OnEndOverlap(otherActor, otherComp, selfComp)
end

-- otherActor 충돌 또는 overlap 대상 Actor
-- otherComp 충돌 또는 overlap 대상 컴포넌트
-- selfComp 이 스크립트가 붙은 컴포넌트
-- impactLocation Hit 발생 위치
-- impactNormal Hit 표면 법선
function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
end
