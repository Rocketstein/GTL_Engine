#include "PhysicsAssetDebugSceneProxy.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/Asset/PhysicsAsset.h"
#include "Physics/Asset/BodySetup.h"
#include "Physics/Asset/ConstraintSetup.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

#pragma region Wire builders

namespace
{
	void AddLine(TArray<FWireLine>& Lines, const FVector& A, const FVector& B)
	{
		Lines.push_back({ A, B });
	}

	// Center 기준 AxisA→AxisB 평면의 원. AxisA/AxisB 는 정규 직교 단위벡터.
	void AddWireCircle(TArray<FWireLine>& Lines, const FVector& Center, const FVector& AxisA, const FVector& AxisB,
		float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 3) return;
		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = Step * i;
			FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	// StartDir→EndDir(직교) 90° 호. 캡슐 반구 캡용.
	void AddWireQuarterArc(TArray<FWireLine>& Lines, const FVector& Center, const FVector& StartDir, const FVector& EndDir,
		float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 1) return;
		const float Step = (FMath::Pi * 0.5f) / static_cast<float>(Segments);
		FVector Prev = Center + StartDir * Radius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = Step * i;
			FVector Next = Center + (StartDir * std::cos(Angle) + EndDir * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	void BuildSphere(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z, float Radius)
	{
		AddWireCircle(Lines, Center, X, Y, Radius, 16);
		AddWireCircle(Lines, Center, Y, Z, Radius, 16);
		AddWireCircle(Lines, Center, Z, X, Radius, 16);
	}

	void BuildBox(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z, const FVector& HalfExtent)
	{
		const FVector EX = X * HalfExtent.X;
		const FVector EY = Y * HalfExtent.Y;
		const FVector EZ = Z * HalfExtent.Z;

		FVector C[8];
		C[0] = Center - EX - EY - EZ;
		C[1] = Center + EX - EY - EZ;
		C[2] = Center + EX + EY - EZ;
		C[3] = Center - EX + EY - EZ;
		C[4] = Center - EX - EY + EZ;
		C[5] = Center + EX - EY + EZ;
		C[6] = Center + EX + EY + EZ;
		C[7] = Center - EX + EY + EZ;

		AddLine(Lines, C[0], C[1]); AddLine(Lines, C[1], C[2]); AddLine(Lines, C[2], C[3]); AddLine(Lines, C[3], C[0]);
		AddLine(Lines, C[4], C[5]); AddLine(Lines, C[5], C[6]); AddLine(Lines, C[6], C[7]); AddLine(Lines, C[7], C[4]);
		AddLine(Lines, C[0], C[4]); AddLine(Lines, C[1], C[5]); AddLine(Lines, C[2], C[6]); AddLine(Lines, C[3], C[7]);
	}

	// 길이축 = Y (엔진 캡슐 규약). X/Z = 단면 평면. HalfLength = 원통 절반 길이(반구 제외).
	void BuildCapsule(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z,
		float Radius, float HalfLength)
	{
		const FVector Top    = Center + Y * HalfLength;
		const FVector Bottom = Center - Y * HalfLength;

		// 원통: 양끝 링 + 4 측선
		AddWireCircle(Lines, Top,    X, Z, Radius, 16);
		AddWireCircle(Lines, Bottom, X, Z, Radius, 16);
		AddLine(Lines, Top + X * Radius, Bottom + X * Radius);
		AddLine(Lines, Top - X * Radius, Bottom - X * Radius);
		AddLine(Lines, Top + Z * Radius, Bottom + Z * Radius);
		AddLine(Lines, Top - Z * Radius, Bottom - Z * Radius);

		// 반구 캡(각 끝 4 호)
		AddWireQuarterArc(Lines, Top, X,  Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, X * -1.0f, Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, Z,  Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, Z * -1.0f, Y, Radius, 6);

		AddWireQuarterArc(Lines, Bottom, X,  Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, X * -1.0f, Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, Z,  Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, Z * -1.0f, Y * -1.0f, Radius, 6);
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  솔리드 빌더 — 와이어 빌더의 삼각형 대응물. 정점은 월드 공간, 법선은 외향.
	//  CachedSolidTris 에 평평한 삼각형 수프(3정점 = 1삼각형)로 누적한다.
	// ─────────────────────────────────────────────────────────────────────────

	void AddSolidTri(TArray<FPhysicsDebugVertex>& Out,
		const FVector& P0, const FVector& N0,
		const FVector& P1, const FVector& N1,
		const FVector& P2, const FVector& N2)
	{
		Out.push_back({ P0, N0 });
		Out.push_back({ P1, N1 });
		Out.push_back({ P2, N2 });
	}

	// 외향 법선 N 을 공유하는 사각형(A→B→C→D, CCW) → 삼각형 2개.
	void AddSolidQuad(TArray<FPhysicsDebugVertex>& Out,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N)
	{
		AddSolidTri(Out, A, N, B, N, C, N);
		AddSolidTri(Out, A, N, C, N, D, N);
	}

	// UV 구. Z = 극축, X/Y = 적도 평면. 법선 = (점-중심) 방향.
	void BuildSolidSphere(TArray<FPhysicsDebugVertex>& Out, const FVector& Center,
		const FVector& X, const FVector& Y, const FVector& Z, float Radius)
	{
		if (Radius <= 0.0f) return;
		constexpr int32 Stacks = 12;   // theta: 0(+Z극) → pi(-Z극)
		constexpr int32 Sectors = 16;  // phi:   0 → 2pi

		auto Dir = [&](int32 i, int32 j) -> FVector
		{
			const float Theta = FMath::Pi * static_cast<float>(i) / static_cast<float>(Stacks);
			const float Phi = 2.0f * FMath::Pi * static_cast<float>(j) / static_cast<float>(Sectors);
			return Z * std::cos(Theta) + (X * std::cos(Phi) + Y * std::sin(Phi)) * std::sin(Theta);
		};

		for (int32 i = 0; i < Stacks; ++i)
		{
			for (int32 j = 0; j < Sectors; ++j)
			{
				const FVector N00 = Dir(i, j);
				const FVector N01 = Dir(i, j + 1);
				const FVector N10 = Dir(i + 1, j);
				const FVector N11 = Dir(i + 1, j + 1);

				const FVector P00 = Center + N00 * Radius;
				const FVector P01 = Center + N01 * Radius;
				const FVector P10 = Center + N10 * Radius;
				const FVector P11 = Center + N11 * Radius;

				AddSolidTri(Out, P00, N00, P10, N10, P11, N11);
				AddSolidTri(Out, P00, N00, P11, N11, P01, N01);
			}
		}
	}

	// OBB. 6면 평면 셰이딩(면 법선).
	void BuildSolidBox(TArray<FPhysicsDebugVertex>& Out, const FVector& Center,
		const FVector& X, const FVector& Y, const FVector& Z, const FVector& HalfExtent)
	{
		const FVector EX = X * HalfExtent.X;
		const FVector EY = Y * HalfExtent.Y;
		const FVector EZ = Z * HalfExtent.Z;

		FVector C[8];
		C[0] = Center - EX - EY - EZ;
		C[1] = Center + EX - EY - EZ;
		C[2] = Center + EX + EY - EZ;
		C[3] = Center - EX + EY - EZ;
		C[4] = Center - EX - EY + EZ;
		C[5] = Center + EX - EY + EZ;
		C[6] = Center + EX + EY + EZ;
		C[7] = Center - EX + EY + EZ;

		AddSolidQuad(Out, C[4], C[5], C[6], C[7],  Z);		 // +Z
		AddSolidQuad(Out, C[0], C[3], C[2], C[1], Z * -1);   // -Z
		AddSolidQuad(Out, C[1], C[2], C[6], C[5],  X);		 // +X
		AddSolidQuad(Out, C[0], C[4], C[7], C[3], X * -1);   // -X
		AddSolidQuad(Out, C[3], C[7], C[6], C[2],  Y);		 // +Y
		AddSolidQuad(Out, C[0], C[1], C[5], C[4], Y * -1);   // -Y
	}

	// 한쪽 반구 캡. Apex 방향 = ApexAxis(단위). 적도(theta=0) → 극(theta=pi/2).
	void BuildSolidHemisphere(TArray<FPhysicsDebugVertex>& Out, const FVector& Origin,
		const FVector& X, const FVector& Z, const FVector& ApexAxis, float Radius)
	{
		constexpr int32 Stacks = 6;
		constexpr int32 Sectors = 16;

		auto Dir = [&](int32 i, int32 j) -> FVector
		{
			const float Theta = (FMath::Pi * 0.5f) * static_cast<float>(i) / static_cast<float>(Stacks);
			const float Phi = 2.0f * FMath::Pi * static_cast<float>(j) / static_cast<float>(Sectors);
			return (X * std::cos(Phi) + Z * std::sin(Phi)) * std::cos(Theta) + ApexAxis * std::sin(Theta);
		};

		for (int32 i = 0; i < Stacks; ++i)
		{
			for (int32 j = 0; j < Sectors; ++j)
			{
				const FVector N00 = Dir(i, j);
				const FVector N01 = Dir(i, j + 1);
				const FVector N10 = Dir(i + 1, j);
				const FVector N11 = Dir(i + 1, j + 1);

				AddSolidTri(Out, Origin + N00 * Radius, N00, Origin + N10 * Radius, N10, Origin + N11 * Radius, N11);
				AddSolidTri(Out, Origin + N00 * Radius, N00, Origin + N11 * Radius, N11, Origin + N01 * Radius, N01);
			}
		}
	}

	// 길이축 = Y. 원통 측면 + 양끝 반구 캡. HalfLength = 원통 절반(반구 제외).
	void BuildSolidCapsule(TArray<FPhysicsDebugVertex>& Out, const FVector& Center,
		const FVector& X, const FVector& Y, const FVector& Z, float Radius, float HalfLength)
	{
		if (Radius <= 0.0f) return;
		const FVector Top = Center + Y * HalfLength;
		const FVector Bottom = Center - Y * HalfLength;
		constexpr int32 Sectors = 16;

		// 원통 측면: 라디얼 법선.
		for (int32 j = 0; j < Sectors; ++j)
		{
			const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(j) / static_cast<float>(Sectors);
			const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(j + 1) / static_cast<float>(Sectors);
			const FVector D0 = X * std::cos(Phi0) + Z * std::sin(Phi0);
			const FVector D1 = X * std::cos(Phi1) + Z * std::sin(Phi1);

			const FVector T0 = Top + D0 * Radius;
			const FVector T1 = Top + D1 * Radius;
			const FVector B0 = Bottom + D0 * Radius;
			const FVector B1 = Bottom + D1 * Radius;

			AddSolidTri(Out, T0, D0, B0, D0, B1, D1);
			AddSolidTri(Out, T0, D0, B1, D1, T1, D1);
		}

		// 반구 캡: 위쪽은 +Y, 아래쪽은 -Y 로 볼록.
		BuildSolidHemisphere(Out, Top, X, Z, Y, Radius);
		BuildSolidHemisphere(Out, Bottom, X, Z, Y * -1.0f, Radius);
	}

	int32 FindBoneIndexByName(const FSkeletalMesh* Mesh, const FName& BoneName)
	{
		const FString Name = BoneName.ToString();
		for (int32 i = 0; i < static_cast<int32>(Mesh->Bones.size()); ++i)
		{
			if (Mesh->Bones[i].Name == Name)
			{
				return i;
			}
		}
		return -1;
	}

	// 모션 타입 → 시각화 각도(라디안). Locked=0(콘 없음), Limited=한계(도→라디안), Free=FreeAngle.
	float AngularVizAngle(EAngularConstraintMotion Motion, float LimitDeg, float FreeAngleRad)
	{
		if (Motion == ACM_Locked) return 0.0f;
		if (Motion == ACM_Free)   return FreeAngleRad;
		const float Rad = LimitDeg * (FMath::Pi / 180.0f);
		return std::max(0.0f, std::min(Rad, FMath::Pi));
	}

	// 조인트 프레임(앵커 + X=twist, Y=swing1, Z=swing2 축) 기준으로 시각화 라인 생성.
	//  - 프레임 축(twist 축은 길게)
	//  - Swing 타원뿔(Swing1=Y방향, Swing2=Z방향 반각)
	//  - Twist 허용 범위 호(Y-Z 평면)
	void BuildConstraintViz(TArray<FWireLine>& Lines, const FVector& Anchor,
		const FVector& AX, const FVector& AY, const FVector& AZ,
		const FConstraintSetup& C, float Size)
	{
		const float AxisLen = Size;

		// 프레임 축. twist(X)를 더 길게.
		AddLine(Lines, Anchor, Anchor + AX * (AxisLen * 1.6f));
		AddLine(Lines, Anchor, Anchor + AY * AxisLen);
		AddLine(Lines, Anchor, Anchor + AZ * AxisLen);

		// Swing 콘: X축을 중심으로 Y(swing1)/Z(swing2) 반각만큼 벌어진 타원뿔.
		const float A1 = AngularVizAngle(C.Swing1Motion, C.Swing1Limit, FMath::Pi * 0.5f);
		const float A2 = AngularVizAngle(C.Swing2Motion, C.Swing2Limit, FMath::Pi * 0.5f);
		if (A1 > 0.0f || A2 > 0.0f)
		{
			const float ConeLen = AxisLen * 1.3f;
			// tan 이 90°에서 발산하므로 80°로 클램프.
			const float Clamp = FMath::Pi * 0.444f;
			const float RY = ConeLen * std::tan(std::min(A1, Clamp));
			const float RZ = ConeLen * std::tan(std::min(A2, Clamp));
			const FVector End = Anchor + AX * ConeLen;

			const int32 Seg = 24;
			FVector Prev = End + AY * RY;
			for (int32 i = 1; i <= Seg; ++i)
			{
				const float Ang = (2.0f * FMath::Pi) * static_cast<float>(i) / static_cast<float>(Seg);
				const FVector P = End + AY * (RY * std::cos(Ang)) + AZ * (RZ * std::sin(Ang));
				AddLine(Lines, Prev, P);
				Prev = P;
			}
			// 앵커→타원 4 스포크.
			AddLine(Lines, Anchor, End + AY * RY);
			AddLine(Lines, Anchor, End - AY * RY);
			AddLine(Lines, Anchor, End + AZ * RZ);
			AddLine(Lines, Anchor, End - AZ * RZ);
		}

		// Twist 부채꼴: 트위스트는 X축 둘레 회전이므로 X에 수직인 Y-Z 평면에서 ±TwistLimit 범위를
		// 부채꼴(arc + 방사형 스포크)로 그린다. 호 = 허용 범위 경계, 스포크 = 부채꼴 채움.
		const float TW = AngularVizAngle(C.TwistMotion, C.TwistLimit, FMath::Pi);
		if (TW > 0.0f)
		{
			const float R = AxisLen * 0.9f;
			// ~12° 간격으로 분할(최소 4).
			const int32 Seg = std::max(4, static_cast<int32>((2.0f * TW) / (FMath::Pi / 15.0f)));
			FVector Prev = Anchor + AY * (R * std::cos(-TW)) + AZ * (R * std::sin(-TW));
			AddLine(Lines, Anchor, Prev);   // 시작 반경선
			for (int32 i = 1; i <= Seg; ++i)
			{
				const float Ang = -TW + (2.0f * TW) * static_cast<float>(i) / static_cast<float>(Seg);
				const FVector P = Anchor + AY * (R * std::cos(Ang)) + AZ * (R * std::sin(Ang));
				AddLine(Lines, Prev, P);     // 호 세그먼트
				AddLine(Lines, Anchor, P);   // 방사형 스포크(부채꼴 채움)
				Prev = P;
			}
		}
	}
}

#pragma endregion


FPhysicsAssetDebugSceneProxy::FPhysicsAssetDebugSceneProxy(UPhysicsAssetDebugComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags = EPrimitiveProxyFlags::EditorOnly
		| EPrimitiveProxyFlags::NeverCull
		| EPrimitiveProxyFlags::PhysicsAssetDebug;

	BodyColor       = FVector4(0.30f, 0.75f, 1.00f, 1.0f);   // 하늘색
	SelectedColor   = FVector4(1.00f, 0.80f, 0.20f, 1.0f);   // 노랑(선택 본)
	ConstraintColor = FVector4(0.40f, 1.00f, 0.45f, 1.0f);   // 연두(조인트 프레임/한계)
	RebuildDebugRender();
}

FPhysicsAssetDebugSceneProxy::~FPhysicsAssetDebugSceneProxy()
{
}

void FPhysicsAssetDebugSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	RebuildDebugRender();
}

void FPhysicsAssetDebugSceneProxy::RebuildDebugRender()
{
	CachedLines.clear();
	CachedSelectedLines.clear();
	CachedConstraintLines.clear();
	CachedSolidTris.clear();
	CachedSelectedSolidTris.clear();

	UPhysicsAssetDebugComponent* Comp = static_cast<UPhysicsAssetDebugComponent*>(GetOwner());
	if (!Comp || !Comp->IsVisibleDebug()) return;

	UPhysicsAsset* Asset = Comp->GetPhysicsAsset();
	USkeletalMeshComponent* MeshComp = Comp->GetTargetMeshComponent();
	if (!Asset || !MeshComp) return;

	USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset) return;

	const int32 SelectedBone = Comp->GetSelectedBoneIndex();

	// 바디 표시 방식 스냅샷. 둘 다 off 면 바디 루프 자체를 생략(조인트는 별개).
	const bool bDrawWire = Comp->IsDrawBodyWireframe();
	const bool bDrawSolid = Comp->IsDrawBodySolid();

	for (const UBodySetup* Body : Asset->BodySetups)
	{
		if (!Comp->IsDrawBodies() || (!bDrawWire && !bDrawSolid)) break;   // "Constraint 만 보기" 모드 — 바디 생략
		if (!Body) continue;

		const int32 BoneIndex = FindBoneIndexByName(MeshAsset, Body->BoneName);
		if (BoneIndex < 0) continue;

		// 본 월드 트랜스폼. elem 은 본-로컬(컴포넌트 단위)이므로 본 월드 스케일까지 적용해야
		// 컴포넌트 월드 스케일(예: cm→m 변환)에서 크기가 어긋나지 않는다.
		const FVector BonePos   = MeshComp->GetBoneLocationByIndex(BoneIndex);
		const FQuat   BoneQuat  = MeshComp->GetBoneQuatByIndex(BoneIndex);
		const FVector BoneScale = MeshComp->GetBoneScaleByIndex(BoneIndex);
		const float   S = (BoneScale.X + BoneScale.Y + BoneScale.Z) / 3.0f;   // 균등 스케일 근사(반경/길이용)

		// 본-로컬 점 → 월드(스케일·회전·이동 순).
		auto ToWorld = [&](const FVector& Local) -> FVector
		{
			return BonePos + BoneQuat.RotateVector(FVector(Local.X * BoneScale.X, Local.Y * BoneScale.Y, Local.Z * BoneScale.Z));
		};

		const bool bSelected = (BoneIndex == SelectedBone);
		TArray<FWireLine>& Out = bSelected ? CachedSelectedLines : CachedLines;
		TArray<FPhysicsDebugVertex>& SolidOut = bSelected ? CachedSelectedSolidTris : CachedSolidTris;

		// Sphere — 회전 불필요. 월드 축 정렬.
		for (const FKSphereElem& Elem : Body->AggGeom.SphereElems)
		{
			const FVector WC = ToWorld(Elem.Center);
			if (bDrawWire)  BuildSphere(Out, WC, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Elem.Radius * S);
			if (bDrawSolid) BuildSolidSphere(SolidOut, WC, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Elem.Radius * S);
		}

		// Box / Capsule — elem.Rotation 을 본 회전과 합성해 월드 기준 축 산출.
		for (const FKBoxElem& Elem : Body->AggGeom.BoxElems)
		{
			const FQuat Q = BoneQuat * Elem.Rotation.ToQuaternion();
			const FVector WC = ToWorld(Elem.Center);
			const FVector AX = Q.RotateVector(FVector(1, 0, 0));
			const FVector AY = Q.RotateVector(FVector(0, 1, 0));
			const FVector AZ = Q.RotateVector(FVector(0, 0, 1));
			const FVector HE(Elem.HalfExtent.X * S, Elem.HalfExtent.Y * S, Elem.HalfExtent.Z * S);
			if (bDrawWire)  BuildBox(Out, WC, AX, AY, AZ, HE);
			if (bDrawSolid) BuildSolidBox(SolidOut, WC, AX, AY, AZ, HE);
		}

		for (const FKSphylElem& Elem : Body->AggGeom.SphylElems)
		{
			const FQuat Q = BoneQuat * Elem.Rotation.ToQuaternion();
			const FVector WC = ToWorld(Elem.Center);
			const FVector AX = Q.RotateVector(FVector(1, 0, 0));
			const FVector AY = Q.RotateVector(FVector(0, 1, 0));
			const FVector AZ = Q.RotateVector(FVector(0, 0, 1));
			if (bDrawWire)  BuildCapsule(Out, WC, AX, AY, AZ, Elem.Radius * S, Elem.Length * 0.5f * S);
			if (bDrawSolid) BuildSolidCapsule(SolidOut, WC, AX, AY, AZ, Elem.Radius * S, Elem.Length * 0.5f * S);
		}
	}

	// ── 조인트 시각화: 각 Constraint 의 ChildFrame 앵커에 프레임 축 + Swing 콘 + Twist 호 ──
	// 마스터 토글 off 면 생략. 본이 선택돼 있으면 그 본에 연결된 Constraint 만 그린다.
	for (const FConstraintSetup& C : Asset->ConstraintSetups)
	{
		if (!Comp->IsDrawConstraints()) break;

		const int32 ChildBone = FindBoneIndexByName(MeshAsset, C.ChildBone);
		if (ChildBone < 0) continue;

		// 선택된 본이 있으면, 그 본이 child 나 parent 인 Constraint 만 표시.
		if (SelectedBone >= 0)
		{
			const int32 ParentBoneSel = FindBoneIndexByName(MeshAsset, C.ParentBone);
			if (ChildBone != SelectedBone && ParentBoneSel != SelectedBone)
			{
				continue;
			}
		}

		const FVector BonePos   = MeshComp->GetBoneLocationByIndex(ChildBone);
		const FQuat   BoneQuat  = MeshComp->GetBoneQuatByIndex(ChildBone);
		const FVector BoneScale = MeshComp->GetBoneScaleByIndex(ChildBone);

		// 조인트 앵커(월드) = 자식 본 월드에 ChildFrame(본-로컬, 컴포넌트 단위) 적용.
		const FVector LP = C.ChildFrame.Location;
		const FVector AnchorWorld = BonePos + BoneQuat.RotateVector(
			FVector(LP.X * BoneScale.X, LP.Y * BoneScale.Y, LP.Z * BoneScale.Z));
		const FQuat FrameQuat = BoneQuat * C.ChildFrame.Rotation;

		const FVector AX = FrameQuat.RotateVector(FVector(1, 0, 0));
		const FVector AY = FrameQuat.RotateVector(FVector(0, 1, 0));
		const FVector AZ = FrameQuat.RotateVector(FVector(0, 0, 1));

		// 시각화 크기: 본 길이(부모와의 거리)에 비례 — 월드 스케일/단위에 무관하게 적당한 크기.
		float VizSize = 0.0f;
		const int32 ParentBone = FindBoneIndexByName(MeshAsset, C.ParentBone);
		if (ParentBone >= 0)
		{
			const FVector ParentPos = MeshComp->GetBoneLocationByIndex(ParentBone);
			const FVector D = AnchorWorld - ParentPos;
			VizSize = 0.4f * std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
		}
		if (VizSize < 1e-4f)
		{
			// 폴백: 본 월드 스케일 기반 기본 크기.
			VizSize = 0.5f * ((BoneScale.X + BoneScale.Y + BoneScale.Z) / 3.0f);
		}

		BuildConstraintViz(CachedConstraintLines, AnchorWorld, AX, AY, AZ, C, VizSize);
	}
}
