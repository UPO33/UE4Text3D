#include "Text3DComponent.h"

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


class FText3DVertexBuffer : public FVertexBuffer
{
public:
	unsigned mNumVertices = 0;
	const TArray<FTextMeshVertex>* mVertices = nullptr;

	void Init(const TArray<FTextMeshVertex>& Vertices)
	{
		mNumVertices = Vertices.Num();

		const uint32 SizeInBytes = Vertices.Num() * Vertices.GetTypeSize();
		void* DataMapped = nullptr;
		FRHIResourceCreateInfo ci;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(SizeInBytes, BUF_Static, ci, DataMapped);
		FMemory::Memcpy(DataMapped, Vertices.GetData(), SizeInBytes);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
	virtual void InitRHI() override
	{
		Init(*mVertices);
		mVertices = nullptr;
	}
};

/** Index Buffer */
class FText3DIndexBuffer : public FIndexBuffer
{
public:
	unsigned mNumIndices = 0;
	const TArray<int32>* mIndices = nullptr;

	void Init(const TArray<int32>& Indices)
	{
		mNumIndices = Indices.Num();

		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
	
	virtual void InitRHI() override
	{
		Init(*mIndices);
		mIndices = nullptr;
	}
};

/** Vertex Factory */
class FText3DVertexFactory : public FLocalVertexFactory
{
public:

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FText3DVertexBuffer* VertexBuffer)
	{
		check(IsInRenderingThread());
		// Initialize the vertex factory's stream components.
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FTextMeshVertex, Position, VET_Float3);
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent(VertexBuffer, 0, 0, VET_Float2)
		);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FTextMeshVertex, Normal, VET_Float3);
		NewData.TangentBasisComponents[1] = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_PackedNormal);
			//STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
// 		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);
		SetData(NewData);
	}

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FText3DVertexBuffer* VertexBuffer)
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

/** Class representing a single section of the proc mesh */
struct FTextMeshSection
{
	UMaterialInterface* Material = nullptr;
	FText3DVertexBuffer VertexBuffer;
	FText3DIndexBuffer IndexBuffer;
	FText3DVertexFactory VertexFactory;
};

class FText3DSceneProxy : public FPrimitiveSceneProxy
{
public:

	FText3DSceneProxy(UText3DComponent* Component)
		: FPrimitiveSceneProxy(Component)
	{

		mMesh = Component->GeneratedMesh;
		check(mMesh.IsValid());

		MaterialRelevance = Component->GetMaterialRelevance(GetScene().GetFeatureLevel());
		
		
		////////////
		auto LGenSection = [=](unsigned meshIndex)
		{
			FTextMeshSection& section = mSections[mNumSelection];
			FResultMeshData& mesh = mMesh->mMeshes[meshIndex];
			
			if (mesh.vertices.Num() < 3)return;

			section.VertexBuffer.mVertices = &(mesh.vertices);
			section.IndexBuffer.mIndices = &(mesh.indices);
			section.Material = Component->GetMaterial(meshIndex);
			if (!section.Material)
				section.Material = UMaterial::GetDefaultMaterial(MD_Surface);

			section.VertexFactory.Init(&section.VertexBuffer);

			

			BeginInitResource(&(section.VertexBuffer));
			BeginInitResource(&(section.IndexBuffer));
			BeginInitResource(&(section.VertexFactory));

			this->mNumSelection++;
		};

		mNumSelection = 0;

		if (Component->bGenerateFronFace)
			LGenSection(0);
		if(Component->bGenerateBackFace)
			LGenSection(1);
		if(Component->bGenerateSide)
			LGenSection(2);
	}

	virtual ~FText3DSceneProxy()
	{
		for (unsigned iSection = 0; iSection < mNumSelection; iSection++)
		{
				mSections[iSection].VertexBuffer.ReleaseResource();
				mSections[iSection].IndexBuffer.ReleaseResource();
				mSections[iSection].VertexFactory.ReleaseResource();
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		// Set up wireframe material (if needed)
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
		if (bWireframe)
		{
			WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
				FLinearColor(0, 0.5f, 1.f)
			);

			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		}

		if(0) // debug drawing
		{
			for (unsigned iSection = 0; iSection < mNumSelection; iSection++)
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						FPrimitiveDrawInterface* pdi = Collector.GetPDI(ViewIndex);

						for (int32 iIndex = 0; iIndex < mMesh->mMeshes[iSection].indices.Num(); iIndex++)
						{
							int32 vertexIndex = mMesh->mMeshes[iSection].indices[iIndex];
							FVector vertexPos = mMesh->mMeshes[iSection].vertices[vertexIndex].Position;
							pdi->DrawPoint(GetLocalToWorld().TransformPosition(vertexPos), FLinearColor::Red, 1, SDPG_World);
						}
					}
				}
			}
		}

		// Iterate over sections
		for(unsigned iSection = 0; iSection < mNumSelection; iSection++)
		{
			const FTextMeshSection& sectionMesh = mSections[iSection];
			{
				FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : sectionMesh.Material->GetRenderProxy(IsSelected());

				// For each view..
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer = &sectionMesh.IndexBuffer;

						Mesh.bWireframe = bWireframe;
						Mesh.VertexFactory = &sectionMesh.VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;
						BatchElement.PrimitiveUniformBuffer = this->GetUniformBuffer();
						//CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
						BatchElement.FirstIndex = 0;
						BatchElement.NumPrimitives = sectionMesh.IndexBuffer.mNumIndices / 3;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = sectionMesh.VertexBuffer.mNumVertices - 1;
						
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;

						Collector.AddMesh(ViewIndex, Mesh);
					}
				}
			}
		}


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{

				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
#endif
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
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize() const 
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}
	FTextMeshSection mSections[3];
	unsigned mNumSelection = 0;
	FMaterialRelevance	MaterialRelevance;
	TSharedPtr<FMeshResultFinal, ESPMode::ThreadSafe> mMesh;
};

FPrimitiveSceneProxy* UText3DComponent::CreateSceneProxy()
{
	if (Text.IsEmpty() || Font == nullptr || !GeneratedMesh.IsValid())  return nullptr;

	return new FText3DSceneProxy(this);
}