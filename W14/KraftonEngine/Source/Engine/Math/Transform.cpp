#include "Transform.h"

FTransform::FTransform(const FMatrix& Mat)
{
	Location = Mat.GetLocation();
	Scale = Mat.GetScale();
	Rotation = Mat.ToQuatWithoutScale();
}

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}
