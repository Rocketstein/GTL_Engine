#include "Physics/Asset/BodyConstraintGenerator.h"

#include "Physics/Asset/PhysicsAsset.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"   // UObjectManager

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
	int32 FindFirstChildBone(const FSkeletalMesh* Mesh, int32 BoneIndex)
	{
		for (int32 i = 0; i < static_cast<int32>(Mesh->Bones.size()); ++i)
		{
			if (Mesh->Bones[i].ParentIndex == BoneIndex)
			{
				return i;
			}
		}
		return -1;
	}

	FVector MatrixTranslation(const FMatrix& M)
	{
		return FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	}

	// 본-로컬 Y축을 Dir(정규화) 로 회전시키는 쿼터니언.
	// 엔진 캡슐 규약: Sphyl.Rotation=identity → 길이축이 본-로컬 Y (AddGeometry 가 PxQuat(90°,Z)
	// 보정으로 PhysX X축을 Y축에 맞춤). 그래서 "Y→본방향" 회전을 Sphyl.Rotation 으로 준다.
	FQuat RotationYToDir(const FVector& Dir)
	{
		const float Dot = Dir.Y;   // (0,1,0)·Dir
		if (Dot >  0.99999f) { return FQuat(); }
		if (Dot < -0.99999f) { return FQuat::FromAxisAngle(FVector(1.0f, 0.0f, 0.0f), 3.14159265f); }
		FVector Axis(Dir.Z, 0.0f, -Dir.X);   // (0,1,0) × Dir
		const float AxisLen = std::sqrt(Axis.X * Axis.X + Axis.Y * Axis.Y + Axis.Z * Axis.Z);
		Axis = FVector(Axis.X / AxisLen, Axis.Y / AxisLen, Axis.Z / AxisLen);
		return FQuat::FromAxisAngle(Axis, std::acos(Dot));
	}

	// 본-로컬 X축을 Dir(정규화) 로 회전시키는 쿼터니언. 조인트 프레임의 twist 축(PxD6 의 X)을
	// 본 세그먼트 방향에 맞추는 데 사용한다.
	FQuat RotationXToDir(const FVector& Dir)
	{
		const float Dot = Dir.X;   // (1,0,0)·Dir
		if (Dot >  0.99999f) { return FQuat(); }
		if (Dot < -0.99999f) { return FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), 3.14159265f); }
		FVector Axis(0.0f, -Dir.Z, Dir.Y);   // (1,0,0) × Dir
		const float AxisLen = std::sqrt(Axis.X * Axis.X + Axis.Y * Axis.Y + Axis.Z * Axis.Z);
		Axis = FVector(Axis.X / AxisLen, Axis.Y / AxisLen, Axis.Z / AxisLen);
		return FQuat::FromAxisAngle(Axis, std::acos(Dot));
	}

	float VLen(const FVector& V) { return std::sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z); }
	FVector VCross(const FVector& A, const FVector& B)
	{
		return FVector(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}
	FVector VNorm(const FVector& V)
	{
		const float L = VLen(V);
		return (L > 1e-6f) ? FVector(V.X / L, V.Y / L, V.Z / L) : FVector(1.0f, 0.0f, 0.0f);
	}

	FString ToLowerName(FString Name)
	{
		std::transform(Name.begin(), Name.end(), Name.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return Name;
	}

	bool NameContainsAny(const FString& LowerName, const TArray<const char*>& Keys)
	{
		for (const char* Key : Keys)
		{
			if (LowerName.find(Key) != FString::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool IsTorsoFitBoneName(const FString& Name)
	{
		static const TArray<const char*> Keys = { "pelvis", "hip", "spine", "chest" };
		return NameContainsAny(ToLowerName(Name), Keys);
	}

	bool IsFineAutoBodyBoneName(const FString& Name)
	{
		static const TArray<const char*> Keys = { "finger", "toe", "skirt", "hair", "twist", "ik" };
		return NameContainsAny(ToLowerName(Name), Keys);
	}

	float GetBoneWeight(const FVertexPNCTBW& V, int32 BoneIndex)
	{
		float Weight = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			if (V.BoneIndices[i] == BoneIndex)
			{
				Weight += V.BoneWeights[i];
			}
		}
		return Weight;
	}

	int32 GetDominantBoneIndex(const FVertexPNCTBW& V)
	{
		int32 BestBone = -1;
		float BestWeight = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			if (V.BoneIndices[i] >= 0 && V.BoneWeights[i] > BestWeight)
			{
				BestBone = V.BoneIndices[i];
				BestWeight = V.BoneWeights[i];
			}
		}
		return BestBone;
	}

	void CollectBoneLocalSamples(
		const FSkeletalMesh* Mesh,
		int32 BoneIndex,
		const FMatrix& InvRef,
		bool bDominantOnly,
		TArray<FVector>& OutSamples)
	{
		OutSamples.clear();
		for (const FVertexPNCTBW& V : Mesh->Vertices)
		{
			const float Weight = GetBoneWeight(V, BoneIndex);
			if (Weight < 0.2f)
			{
				continue;
			}
			if (bDominantOnly && GetDominantBoneIndex(V) != BoneIndex)
			{
				continue;
			}
			OutSamples.push_back(InvRef.TransformPositionWithW(V.Position));
		}
	}

	float SelectQuantile(TArray<float> Values, float Alpha)
	{
		if (Values.empty())
		{
			return 0.0f;
		}

		const size_t LastIndex = Values.size() - 1;
		const size_t QuantileIndex = static_cast<size_t>(std::round(std::clamp(Alpha, 0.0f, 1.0f) * static_cast<float>(LastIndex)));
		std::nth_element(Values.begin(), Values.begin() + QuantileIndex, Values.end());
		return Values[QuantileIndex];
	}

	void GetSampleBounds(const TArray<FVector>& Samples, bool bTrimOutliers, FVector& OutMin, FVector& OutMax)
	{
		OutMin = FVector(1e30f, 1e30f, 1e30f);
		OutMax = FVector(-1e30f, -1e30f, -1e30f);

		if (bTrimOutliers && Samples.size() >= 16)
		{
			TArray<float> Xs;
			TArray<float> Ys;
			TArray<float> Zs;
			Xs.reserve(Samples.size());
			Ys.reserve(Samples.size());
			Zs.reserve(Samples.size());
			for (const FVector& L : Samples)
			{
				Xs.push_back(L.X);
				Ys.push_back(L.Y);
				Zs.push_back(L.Z);
			}

			OutMin.X = SelectQuantile(Xs, 0.05f);
			OutMax.X = SelectQuantile(Xs, 0.95f);
			OutMin.Y = SelectQuantile(Ys, 0.05f);
			OutMax.Y = SelectQuantile(Ys, 0.95f);
			OutMin.Z = SelectQuantile(Zs, 0.05f);
			OutMax.Z = SelectQuantile(Zs, 0.95f);
			return;
		}

		for (const FVector& L : Samples)
		{
			OutMin.X = std::min(OutMin.X, L.X); OutMin.Y = std::min(OutMin.Y, L.Y); OutMin.Z = std::min(OutMin.Z, L.Z);
			OutMax.X = std::max(OutMax.X, L.X); OutMax.Y = std::max(OutMax.Y, L.Y); OutMax.Z = std::max(OutMax.Z, L.Z);
		}
	}

	// 경첩(1축) 관절로 다룰 본 이름인지 — 무릎/팔꿈치류. 이름 기반 휴리스틱(스켈레톤 의존).
	bool IsHingeBoneName(const FString& Name)
	{
		FString L = ToLowerName(Name);
		static const char* Keys[] = { "calf", "shin", "lowerleg", "knee", "forearm", "lowerarm", "elbow" };
		for (const char* K : Keys)
		{
			if (L.find(K) != FString::npos) { return true; }
		}
		return false;
	}

	// 자동 일괄 생성(GenerateAll)에서 바디를 만들지 않을 본 — 손가락/발가락/치맛자락처럼
	// 물리적 의미는 작고 바디 수만 크게 늘리는 본. 바디 수는 simulate 비용에 거의 비례하므로
	// 이걸 빼는 게 가장 큰 절감 레버다. 이름 기반 휴리스틱(스켈레톤 의존, 필요 시 키 추가).
	// ※ 수동 Add(GenerateBody)는 이 필터를 거치지 않으므로 원하면 개별 본에 바디를 추가할 수 있다.
	bool ShouldSkipBoneForAutoBody(const FString& Name)
	{
		FString L = Name;
		std::transform(L.begin(), L.end(), L.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		static const char* Keys[] = {
			// 손가락 (Mixamo: HandIndex/Middle/Ring/Pinky/Thumb, Bip001: Finger/Thumb)
			"finger", "thumb", "index", "middle", "pinky", "pinki", "ring",
			// 발가락
			"toe",
			// 의상/장식 다이내믹 본
			"skirt", "cloth", "dress", "hair", "tail", "ribbon", "scarf", "cape",
		};
		for (const char* K : Keys)
		{
			if (L.find(K) != FString::npos) { return true; }
		}
		return false;
	}

	// 본의 첫 자식 방향/거리로 캡슐(본-로컬)을 피팅. 자식 없으면(=leaf) 작은 기본값.
	// (Fallback) 부모→첫 자식 세그먼트로 캡슐 피팅. 스킨 버텍스가 없을 때만 사용.
	bool FitCapsuleFromChildSegment(const FSkeletalMesh* Mesh, int32 BoneIndex, FKSphylElem& Out)
	{
		const int32 Child = FindFirstChildBone(Mesh, BoneIndex);
		if (Child == -1)
		{
			return false;
		}
		// 자식은 이 본의 자식이므로 자식 ReferenceLocalPose 의 translation = 본-로컬에서의 자식 원점.
		const FVector ChildLocal = MatrixTranslation(Mesh->Bones[Child].ReferenceLocalPose);
		const float Len = std::sqrt(ChildLocal.X * ChildLocal.X + ChildLocal.Y * ChildLocal.Y + ChildLocal.Z * ChildLocal.Z);
		if (Len < 1e-3f)
		{
			return false;
		}
		const FVector Dir(ChildLocal.X / Len, ChildLocal.Y / Len, ChildLocal.Z / Len);
		Out.Center   = FVector(ChildLocal.X * 0.5f, ChildLocal.Y * 0.5f, ChildLocal.Z * 0.5f);
		Out.Radius   = std::max(Len * 0.12f, 0.05f);
		Out.Length   = std::max(Len - 2.0f * Out.Radius, Len * 0.1f);
		Out.Rotation = RotationYToDir(Dir).ToRotator();
		return true;
	}

	// 본에 스킨된 버텍스들을 본-로컬(ref pose 기준)로 모아 AABB 를 구하고 캡슐을 피팅한다.
	// 세그먼트가 없는 본(겹친 루트)·다분기 본(Pelvis 등)·말단 본도 실제 메시 부피에 맞게 잡힌다.
	// 스킨 버텍스가 없으면 세그먼트 휴리스틱 → 기본 캡슐 순으로 폴백.
	void FitBodyCapsule(const FSkeletalMesh* Mesh, int32 BoneIndex, FKSphylElem& Out)
	{
		// 1) 스킨 버텍스 AABB (본-로컬). 스키닝과 동일하게 InverseBindPose 로 bind-pose 버텍스를
		//    본-로컬로 가져온다. (스킨 바인드 포즈 ≠ 레퍼런스 포즈인 경우 — 루트 90° 등 — 에도
		//    드로우/시뮬과 정확히 일치: elem(bind-local) * 현재 본 글로벌 = 스킨된 버텍스 위치.)
		const FMatrix InvRef = Mesh->Bones[BoneIndex].GetInverseBindPose();
		FVector Min( 1e30f,  1e30f,  1e30f);
		FVector Max(-1e30f, -1e30f, -1e30f);
		int32 Count = 0;
		for (const FVertexPNCTBW& V : Mesh->Vertices)
		{
			float Weight = 0.0f;
			for (int32 i = 0; i < 4; ++i)
			{
				if (V.BoneIndices[i] == BoneIndex) { Weight += V.BoneWeights[i]; }
			}
			if (Weight < 0.2f) { continue; }   // 주요 영향 버텍스만(노이즈/약한 가중치 제외)

			const FVector L = InvRef.TransformPositionWithW(V.Position);
			Min.X = std::min(Min.X, L.X); Min.Y = std::min(Min.Y, L.Y); Min.Z = std::min(Min.Z, L.Z);
			Max.X = std::max(Max.X, L.X); Max.Y = std::max(Max.Y, L.Y); Max.Z = std::max(Max.Z, L.Z);
			++Count;
		}

		if (Count > 0)
		{
			Out.Center = FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f);
			const float HX = std::max((Max.X - Min.X) * 0.5f, 1e-4f);
			const float HY = std::max((Max.Y - Min.Y) * 0.5f, 1e-4f);
			const float HZ = std::max((Max.Z - Min.Z) * 0.5f, 1e-4f);

			// 가장 긴 축 = 캡슐 길이축, 나머지 둘 중 큰 쪽을 반경으로(버텍스 포함). 캡슐 길이축 규약=본-로컬 Y.
			float HalfLongest, CrossA, CrossB;
			FVector AxisDir;
			if (HX >= HY && HX >= HZ)      { HalfLongest = HX; CrossA = HY; CrossB = HZ; AxisDir = FVector(1, 0, 0); }
			else if (HY >= HX && HY >= HZ) { HalfLongest = HY; CrossA = HX; CrossB = HZ; AxisDir = FVector(0, 1, 0); }
			else                           { HalfLongest = HZ; CrossA = HX; CrossB = HY; AxisDir = FVector(0, 0, 1); }

			Out.Radius   = std::max(std::max(CrossA, CrossB), 0.01f);
			Out.Length   = std::max(2.0f * HalfLongest - 2.0f * Out.Radius, 0.0f);
			Out.Rotation = RotationYToDir(AxisDir).ToRotator();
			return;
		}

		// 2) 스킨 버텍스 없음(비스킨 본 등) → 세그먼트 휴리스틱.
		if (FitCapsuleFromChildSegment(Mesh, BoneIndex, Out))
		{
			return;
		}

		// 3) 기본 캡슐.
		Out.Center   = FVector(0.0f, 0.0f, 0.0f);
		Out.Rotation = FRotator(0.0f, 0.0f, 0.0f);
		Out.Radius   = 0.5f;
		Out.Length   = 1.0f;
	}

	void FitBodyCapsuleRobust(const FSkeletalMesh* Mesh, int32 BoneIndex, FKSphylElem& Out)
	{
		const FMatrix InvRef = Mesh->Bones[BoneIndex].GetInverseBindPose();
		const bool bTorsoFit = IsTorsoFitBoneName(Mesh->Bones[BoneIndex].Name);

		TArray<FVector> Samples;
		if (bTorsoFit)
		{
			CollectBoneLocalSamples(Mesh, BoneIndex, InvRef, true, Samples);
		}
		if (Samples.size() < 8)
		{
			CollectBoneLocalSamples(Mesh, BoneIndex, InvRef, false, Samples);
		}

		if (!Samples.empty())
		{
			FVector Min;
			FVector Max;
			GetSampleBounds(Samples, bTorsoFit, Min, Max);

			Out.Center = FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f);
			const float HX = std::max((Max.X - Min.X) * 0.5f, 1e-4f);
			const float HY = std::max((Max.Y - Min.Y) * 0.5f, 1e-4f);
			const float HZ = std::max((Max.Z - Min.Z) * 0.5f, 1e-4f);

			float HalfLongest, CrossA, CrossB;
			FVector AxisDir;
			if (HX >= HY && HX >= HZ)      { HalfLongest = HX; CrossA = HY; CrossB = HZ; AxisDir = FVector(1, 0, 0); }
			else if (HY >= HX && HY >= HZ) { HalfLongest = HY; CrossA = HX; CrossB = HZ; AxisDir = FVector(0, 1, 0); }
			else                           { HalfLongest = HZ; CrossA = HX; CrossB = HY; AxisDir = FVector(0, 0, 1); }

			Out.Radius = std::max(std::max(CrossA, CrossB), 0.01f);
			Out.Length = std::max(2.0f * HalfLongest - 2.0f * Out.Radius, 0.0f);
			Out.Rotation = RotationYToDir(AxisDir).ToRotator();
			return;
		}

		if (FitCapsuleFromChildSegment(Mesh, BoneIndex, Out))
		{
			return;
		}

		Out.Center = FVector(0.0f, 0.0f, 0.0f);
		Out.Rotation = FRotator(0.0f, 0.0f, 0.0f);
		Out.Radius = 0.5f;
		Out.Length = 1.0f;
	}

}

UBodySetup* FBodyConstraintGenerator::GenerateBody(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh, int32 BoneIndex)
{
	if (!Asset || !Mesh || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh->Bones.size()))
	{
		return nullptr;
	}

	const FName BoneName(Mesh->Bones[BoneIndex].Name);
	if (Asset->FindBodyIndex(BoneName) != -1)
	{
		return nullptr;   // 이미 존재
	}

	UBodySetup* Body = UObjectManager::Get().CreateObject<UBodySetup>(Asset);
	if (!Body)
	{
		return nullptr;
	}
	Body->BoneName = BoneName;

	FKSphylElem Capsule;
	FitBodyCapsuleRobust(Mesh, BoneIndex, Capsule);
	Body->AggGeom.SphylElems.push_back(Capsule);

	Asset->BodySetups.push_back(Body);
	return Body;
}

int32 FBodyConstraintGenerator::GenerateConstraint(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh, int32 ChildBoneIndex)
{
	if (!Asset || !Mesh || ChildBoneIndex < 0 || ChildBoneIndex >= static_cast<int32>(Mesh->Bones.size()))
	{
		return -1;
	}

	const int32 ParentIndex = Mesh->Bones[ChildBoneIndex].ParentIndex;
	if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Mesh->Bones.size()))
	{
		return -1;   // 루트(부모 없음)
	}

	const FName ChildName(Mesh->Bones[ChildBoneIndex].Name);
	const FName ParentName(Mesh->Bones[ParentIndex].Name);
	if (Asset->FindConstraintIndex(ChildName) != -1)
	{
		return -1;   // 중복
	}

	FConstraintSetup Constraint;
	Constraint.ParentBone = ParentName;
	Constraint.ChildBone  = ChildName;

	// 조인트 앵커는 자식 본 원점. 추가로 조인트 프레임의 twist 축(X)을 본 세그먼트 방향
	// (자식의 첫 자식으로 향하는 방향)에 정렬한다. 정렬이 없으면 본 로컬 X(스켈레톤에 따라
	// 옆방향)가 twist 축이 되어 Swing 콘이 본을 따라가지 않고 옆으로 뻗는다.
	//   ChildFrame  = (원점, FrameRot)  — 자식 본 로컬
	//   ParentFrame = ChildFrameMat * 자식 로컬 포즈 (행렬 합성, 아래) — rest 에서 두 프레임 일치
	const FBone& ChildBone = Mesh->Bones[ChildBoneIndex];

	// 세그먼트 방향(자식 → 그 첫 자식) — child 본 로컬.
	FVector SegLocal(0.0f, 0.0f, 0.0f);
	bool bHasSeg = false;
	const int32 GrandChild = FindFirstChildBone(Mesh, ChildBoneIndex);
	if (GrandChild != -1)
	{
		const FVector Seg = MatrixTranslation(Mesh->Bones[GrandChild].ReferenceLocalPose);
		if (VLen(Seg) > 1e-4f) { SegLocal = VNorm(Seg); bHasSeg = true; }
	}

	FQuat FrameRot;   // 조인트 프레임 회전(child 본 로컬). identity = 본 로컬 축 그대로(leaf 폴백).
	if (bHasSeg && IsHingeBoneName(ChildName.ToString()))
	{
		// 경첩(무릎/팔꿈치): twist 축을 "경첩 회전축"에 맞춘다. rest 포즈가 펴진 상태라
		// 두 세그먼트의 외적으론 축을 못 구하므로, 세그먼트와 월드축의 외적으로 측면 축을 추정한다.
		// twist 만 넓게 Limited, 두 swing 은 Locked → 1축 힌지(UE 무릎의 Twist=60, Swing≈0 형태).
		const FQuat ChildGlobalRot = FQuat::FromMatrix(ChildBone.GetReferenceGlobalPose());
		const FVector SegComp = ChildGlobalRot.RotateVector(SegLocal);
		const FVector Refs[3] = { FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f) };
		FVector HingeComp(1.0f, 0.0f, 0.0f);
		for (const FVector& Ref : Refs)
		{
			const FVector C = VCross(SegComp, Ref);
			if (VLen(C) > 0.3f) { HingeComp = C; break; }   // 세그먼트와 충분히 수직인 첫 축
		}
		const FVector HingeLocal = ChildGlobalRot.Inverse().RotateVector(VNorm(HingeComp));
		FrameRot = RotationXToDir(VNorm(HingeLocal));

		Constraint.TwistMotion  = ACM_Limited;
		Constraint.TwistLimit   = 80.0f;   // 도. 넓은 대칭 굽힘 범위(방향/비대칭은 수동 튜닝).
		Constraint.Swing1Motion = ACM_Locked;
		Constraint.Swing2Motion = ACM_Locked;
	}
	else if (bHasSeg)
	{
		// 일반(볼/콘) 조인트: twist 축 = 본 길이축. 모션은 struct 기본값(전 축 Limited 콘) 유지.
		FrameRot = RotationXToDir(SegLocal);
	}

	// ChildFrame = (자식 본 원점, FrameRot). ParentFrame 은 엔진 규약(row-vector child*parent)으로
	// 행렬 합성해 bind 포즈에서 ChildFrame 과 정확히 일치시킨다(쿼터니언 합성은 column 규약이라
	// 순서가 어긋나 머리/쇄골 등에서 90° 스냅을 유발했음):
	//   jointWorld = ChildFrameMat * childGlobal = ParentFrameMat * parentGlobal
	//   childGlobal = childLocalPose * parentGlobal  →  ParentFrameMat = ChildFrameMat * childLocalPose
	Constraint.ChildFrame  = FTransform(FVector(0.0f, 0.0f, 0.0f), FrameRot, FVector(1.0f, 1.0f, 1.0f));

	const FMatrix ChildFrameMat  = FrameRot.ToMatrix();
	const FMatrix ParentFrameMat = ChildFrameMat * ChildBone.ReferenceLocalPose;
	FTransform ParentFrame(ParentFrameMat);
	ParentFrame.Scale = FVector(1.0f, 1.0f, 1.0f);   // 본 로컬 포즈 스케일 누수 방지
	Constraint.ParentFrame = ParentFrame;

	Asset->ConstraintSetups.push_back(Constraint);
	return static_cast<int32>(Asset->ConstraintSetups.size()) - 1;
}

void FBodyConstraintGenerator::GenerateAll(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh)
{
	if (!Asset || !Mesh)
	{
		return;
	}

	const int32 BoneCount = static_cast<int32>(Mesh->Bones.size());

	// 바디 — 스키닝에 관여하는 본만 생성(헬퍼/IK/트위스트 등 스킨 비참여 본 제외).
	// 이미 있는 건 GenerateBody 가 스킵. leaf 스킨 본은 FitBodyCapsule 이 기본 캡슐로 처리.
	for (int32 i = 0; i < BoneCount; ++i)
	{
		if (!Mesh->IsBoneSkinned(i))
		{
			continue;
		}
		// 손가락/발가락/치맛자락 등은 바디 생성 제외(수동 Add 로는 추가 가능).
		if (ShouldSkipBoneForAutoBody(FName(Mesh->Bones[i].Name).ToString()))
		{
			continue;
		}
		if (Mesh->Bones[i].ParentIndex < 0 || IsFineAutoBodyBoneName(Mesh->Bones[i].Name))
		{
			continue;
		}
		GenerateBody(Asset, Mesh, i);
	}

	// 조인트 — 본/부모 모두 바디가 있는 쌍에만 생성(이미 있는 건 GenerateConstraint 가 스킵).
	for (int32 i = 0; i < BoneCount; ++i)
	{
		const int32 P = Mesh->Bones[i].ParentIndex;
		if (P < 0)
		{
			continue;
		}
		if (Asset->FindBodyIndex(FName(Mesh->Bones[i].Name)) == -1) { continue; }
		if (Asset->FindBodyIndex(FName(Mesh->Bones[P].Name)) == -1) { continue; }
		GenerateConstraint(Asset, Mesh, i);
	}
}
