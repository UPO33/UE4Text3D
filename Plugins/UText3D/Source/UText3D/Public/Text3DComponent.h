#pragma once

#include "Components/MeshComponent.h"

#include "Text3DComponent.generated.h"


struct FTri
{
	FVector a, b, c;
	FVector& operator [] (size_t index) { return ((FVector*)this)[index]; }
	const FVector& operator [] (size_t index) const { return ((const FVector*)this)[index]; }

	void Move(const FVector& v)
	{
		a += v;
		b += v;
		c += v;
	}
	FTri operator * (const FTransform& trs) const
	{
		return FTri{trs.TransformPosition(a), trs.TransformPosition(b), trs.TransformPosition(c) };
	}
};


struct FTextMeshVertex
{
	FVector	Position;
	FVector Normal;
};

struct FResultMeshData
{
	TArray<FTextMeshVertex> vertices;
	TArray<int32> indices;

	FBox CalcBound() const
	{
		FBox box(ForceInit);
		for (const FTextMeshVertex& vertex : vertices)
			box += vertex.Position;
		return box;
	}
};
struct FMeshResultFinal
{
	FResultMeshData	mMeshes[3];	//front back side
	FBox mBound;

	FBox CalcBound()
	{
		mBound = mMeshes[0].CalcBound() + mMeshes[1].CalcBound() + mMeshes[2].CalcBound();
		return mBound;
	}
};

UENUM()
enum class EText3DHAlign : uint8
{
	LEFT,
	CENTER,
	RIGHT,
};

UENUM()
enum class EText3DVAlign : uint8
{
	TOP,
	CENTER,
	BOTTOM,
};



UCLASS(editinlinenew, meta=(BlueprintSpawnableComponent))
class UTEXT3D_API UText3DComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UText3DComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MultiLine=true))
	FString Text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UFontFace* Font;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int BezierStep;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Depth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGenerateSide;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGenerateFronFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGenerateBackFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EText3DHAlign HorizontalAlignment;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EText3DVAlign VerticalAlignment;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int LineSpace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Transform;
	//optional ISO 15924 script tag, e.g cyrl, jpan, hebr, arab, ...
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Script;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//regenerates a new mesh, call this after changing any of the properties
	UFUNCTION(BlueprintCallable)
	void UpdateMesh();

	TSharedPtr<FMeshResultFinal, ESPMode::ThreadSafe> GeneratedMesh;

	FMeshResultFinal* GetGeneratedMesh() const;

	virtual int32 GetNumMaterials() const override;

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;


	virtual void OnRegister() override;

private:
	void GenerateMesh(struct FTextShaper* in);
};