// Fill out your copyright notice in the Description page of Project Settings.

#include "Text3DProject.h"
#include "Modules/ModuleManager.h"
#include "SceneManagement.h"
#include "RHI.h"
#include "Materials/Material.h"
#include "PrimitiveSceneProxy.h"

#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"


IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Text3DProject, "Text3DProject" );

#if 0
struct FMyVB : FVertexBuffer
{
	TArray<FDynamicMeshVertex> mVertices;

	void CreateTriangle()
	{
		mVertices.Add(FDynamicMeshVertex(FVector(0, 0, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector2D(0, 0), FColor::Red));
		mVertices.Add(FDynamicMeshVertex(FVector(0, 100, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector2D(0, 0), FColor::Red));
		mVertices.Add(FDynamicMeshVertex(FVector(100, 100, 0), FVector(1, 0, 0), FVector(0, 0, 1), FVector2D(0, 0), FColor::Red));
	}
	void InitRHI() override
	{
		CreateTriangle();

		const uint32 SizeInBytes = mVertices.Num() * mVertices.GetTypeSize();
		void* DataMapped = nullptr;
		FRHIResourceCreateInfo ci;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(SizeInBytes, BUF_Static, ci, DataMapped);
		FMemory::Memcpy(DataMapped, mVertices.GetData(), SizeInBytes);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};
struct FMyIB : FIndexBuffer
{
	TArray<int32> mIndices;
	
	void InitRHI() override
	{
		mIndices.Append({ 0, 1, 2 });

		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), mIndices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);
		FMemory::Memcpy(Buffer, mIndices.GetData(), mIndices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};


struct FMyVF : FLocalVertexFactory
{
	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FVertexBuffer* VertexBuffer)
	{
		check(IsInRenderingThread());

		// Initialize the vertex factory's stream components.
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
		//NewData.TextureCoordinates.Add(
		//	FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
		//);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_Float3);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
		//NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);
		SetData(NewData);
	}

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FVertexBuffer* VertexBuffer)
	{
		if (IsInRenderingThread())
		{
			Init_RenderThread(VertexBuffer);
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(CreateVF)(
				[=](FRHICommandListImmediate& RHICmdList) {
					this->Init_RenderThread(VertexBuffer);
				}
			);
		}
	}
};

class FTestRenderProxy : public FPrimitiveSceneProxy
{
public:
	FTestRenderProxy(UTestRenderComponent* pComponent) : FPrimitiveSceneProxy(pComponent)
	{
		mMaterialRelevance = pComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());

		mMaterial = pComponent->GetMaterial(0);
		if (!mMaterial)
			mMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		
		mVB = new FMyVB;
		mIB = new FMyIB;
		mVF = new FMyVF;
		
		mVF->Init(mVB);

		BeginInitResource(mVB);
		BeginInitResource(mIB);
		BeginInitResource(mVF);
	}
	~FTestRenderProxy()
	{
		mVF->ReleaseResource();
		mVB->ReleaseResource();
		mIB->ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView *>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,  FMeshElementCollector& meshCollector) const override
	{
		for (int iView = 0; iView < Views.Num(); iView++)
		{
			if (VisibilityMap & (1 << iView))
			{
				FMeshBatch& Mesh = meshCollector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];

				const FSceneView* View = Views[iView];
				// Draw the mesh.
				BatchElement.IndexBuffer = mIB;
				Mesh.bWireframe = false;
				Mesh.VertexFactory = mVF;
				Mesh.MaterialRenderProxy = mMaterial->GetRenderProxy(IsSelected(), IsHovered());
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
				//CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = mIB->mIndices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = mVB->mVertices.Num() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				meshCollector.AddMesh(iView, Mesh);
			}
		}
	}

	uint32 GetAllocatedSize() const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}
	virtual uint32 GetMemoryFootprint(void) const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		mMaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	FMyVF* mVF = nullptr;
	FMyVB* mVB = nullptr;
	FMyIB* mIB = nullptr;
	UMaterialInterface* mMaterial;
	FMaterialRelevance mMaterialRelevance;
};

FPrimitiveSceneProxy* UTestRenderComponent::CreateSceneProxy()
{
	return new FTestRenderProxy(this);
}

int32 UTestRenderComponent::GetNumMaterials() const
{
	return 1;
}

UTestRenderComponent::UTestRenderComponent()
{

}
#endif