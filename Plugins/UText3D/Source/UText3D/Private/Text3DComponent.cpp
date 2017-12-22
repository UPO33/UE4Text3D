#include "Text3DComponent.h"
#include "Text3D.h"
#include "Engine/FontFace.h"

#include "Materials/MaterialInterface.h"
#include "Engine/Font.h"
#include "Materials/Material.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "TimerManager.h"

#include "Private/Fonts/FontCacheFreeType.h"
#include "Vectoriser.h"

#include "Internationalization/Text.h"

#include "poly2tri/poly2tri.h"

#include "Private/Fonts/FontCacheHarfBuzz.h"

#if WITH_HARFBUZZ && WITH_EDITOR

extern "C"
{

	void* HarfBuzzMalloc(size_t InSizeBytes)
	{
		return FMemory::Malloc(InSizeBytes);
	}

	void* HarfBuzzCalloc(size_t InNumItems, size_t InItemSizeBytes)
	{
		const size_t AllocSizeBytes = InNumItems * InItemSizeBytes;
		if (AllocSizeBytes > 0)
		{
			void* Ptr = FMemory::Malloc(AllocSizeBytes);
			FMemory::Memzero(Ptr, AllocSizeBytes);
			return Ptr;
		}
		return nullptr;
	}

	void* HarfBuzzRealloc(void* InPtr, size_t InSizeBytes)
	{
		return FMemory::Realloc(InPtr, InSizeBytes);
	}

	void HarfBuzzFree(void* InPtr)
	{
		FMemory::Free(InPtr);
	}

} // extern "C"

#endif // #if WITH_HARFBUZZ

// THIRD_PARTY_INCLUDES_START
// #include "hb.h"
// #include "hb-ft.h"
// #include "hb-blob.h"
// #include "hb-buffer.h"
// THIRD_PARTY_INCLUDES_END

#if WITH_FREETYPE
THIRD_PARTY_INCLUDES_START
#include "ft2build.h"

// FreeType style include
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_BITMAP_H
#include FT_ADVANCES_H
#include FT_STROKER_H

THIRD_PARTY_INCLUDES_END
#endif // 


#if WITH_FREETYPE && WITH_HARFBUZZ
std::vector<p2t::Point*> UTriangulateContour(Vectoriser *vectoriser, int c, FVector2D offset)
{
	std::vector<p2t::Point*> polyline;
	const Contour* contour = vectoriser->GetContour(c);
	for (size_t p = 0; p < contour->PointCount(); ++p)
	{
		const double* d = contour->GetPoint(p);
		polyline.push_back(new p2t::Point((d[0] / 64.0f) + offset.X, (d[1] / 64.0f) + offset.Y));
	}
	return polyline;
}
//////////////////////////////////////////////////////////////////////////
void UIndexingTriFlatNormal(const TArray<FTri>& inTriangles, TArray<FTextMeshVertex>& outVertices, TArray<int32>& outIndices)
{
	auto FindVert = [&](const FVector& inPos, const FVector& inNormal) -> uint32
	{
		int32 iVertex = FMath::Abs(outIndices.Num() - 64);
		for (; iVertex < outVertices.Num(); iVertex++)
		{
			if (inPos.Equals(outVertices[iVertex].Position) && inNormal.Equals(outVertices[iVertex].Normal))
				return (uint32)iVertex;
		}
		return ~(uint32(0));
	};

	for (int iTri = 0; iTri < inTriangles.Num(); iTri++)
	{
		// Calculate triangle edge vectors and normal
		const FVector Edge21 = inTriangles[iTri].b - inTriangles[iTri].c;
		const FVector Edge20 = inTriangles[iTri].a - inTriangles[iTri].c;
		const FVector TriNormal = (Edge21 ^ Edge20).GetSafeNormal();

		for (uint32 iIndex = 0; iIndex < 3; iIndex++)
		{
			uint32 index = FindVert(inTriangles[iTri][iIndex], TriNormal);
			if (index == ~uint32(0))	//not found?
			{
				index = outVertices.AddUninitialized();
				outVertices.Last().Position = inTriangles[iTri][iIndex];
				outVertices.Last().Normal = TriNormal;
			}
			outIndices.Add(index);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
struct FTextShaper
{

	FT_Face mFontFace = nullptr; 
	FString mText;
	hb_language_t mTextLanguage;
	int mBezierSteps;
	float mExtrude;
	bool mGenerateSide;
	bool mGenerateFontFace;
	bool mGenerateBackFace;
	EText3DVAlign mVTA;
	EText3DHAlign mHTA;
	FTransform mTransform;
	float mLineSpace;
	TArray<FTri> mTris[3];	//front, back, side
	char mScript[8] = {};

	FTextShaper(UText3DComponent* pComponent)
	{
		this->mBezierSteps = pComponent->BezierStep;
		this->mExtrude = pComponent->Depth;
		this->mText = pComponent->Text;
		this->mGenerateFontFace = pComponent->bGenerateFronFace;
		this->mGenerateBackFace = pComponent->bGenerateBackFace;
		this->mGenerateSide = pComponent->bGenerateSide;
		this->mVTA = pComponent->VerticalAlignment;
		this->mHTA = pComponent->HorizontalAlignment;
		this->mTransform = pComponent->Transform;
		this->mLineSpace = pComponent->LineSpace;
		this->mTextLanguage = hb_language_get_default();

		for (int i = 0; i < 8; i++)
			this->mScript[i] = pComponent->Script.IsValidIndex(i) ? (char)(pComponent->Script[i]) : (char)0;

	}
	void TriangulateGlyph_P2T(FT_GlyphSlot glyph, int codePoint, FVector2D offsetXY)
	{
		if (1)
		{

			Vectoriser vectoriser = Vectoriser(glyph, mBezierSteps);
			for (size_t c = 0; c < vectoriser.ContourCount(); ++c)
			{
				const Contour* contour = vectoriser.GetContour(c);

				//FVector vOffset = FVector(xx, yy, 0);

				size_t pc = contour->PointCount();
				if (mGenerateSide)
				{
					for (size_t p = 0; p < contour->PointCount(); ++p)
					{
						const double* point0 = contour->GetPoint(p);
						const double* point1 = contour->GetPoint((p + 1) % pc);

						GenSideTri(point0, point1, FVector(offsetXY, 0));
					}
				}
				if (mGenerateBackFace || mGenerateFontFace)
				{
					if (contour->GetDirection())
					{
						{
							std::vector<p2t::Point*> polyline = UTriangulateContour(&vectoriser, c, offsetXY);
							if(polyline.size() < 3)
								continue;

							p2t::CDT cdt = p2t::CDT(polyline);

							for (size_t cm = 0; cm < vectoriser.ContourCount(); ++cm) 
							{
								const Contour* sm = vectoriser.GetContour(cm);
								if (c != cm && !sm->GetDirection() && sm->IsInside(contour)) 
								{
									std::vector<p2t::Point*> pl = UTriangulateContour(&vectoriser, cm, offsetXY);
									
									if(pl.size() < 3)
										continue;

									cdt.AddHole(pl);
								}
							}

							cdt.Triangulate();
							std::vector<p2t::Triangle*> ts = cdt.GetTriangles();
							for (int i = 0; i < ts.size(); i++) 
							{
								p2t::Triangle* ot = ts[i];

								//front tri
								if(mGenerateFontFace)
								{
									FTri t1;
									t1.a.X = ot->GetPoint(0)->x;
									t1.a.Y = ot->GetPoint(0)->y;
									t1.a.Z = 0.0f;

									t1.b.X = ot->GetPoint(1)->x;
									t1.b.Y = ot->GetPoint(1)->y;
									t1.b.Z = 0.0f;

									t1.c.X = ot->GetPoint(2)->x;
									t1.c.Y = ot->GetPoint(2)->y;
									t1.c.Z = 0.0f;

									mTris[0].Add(t1);
								}

								//back tri
								if(mGenerateBackFace)
								{
									FTri t2;
									
									t2.a.X = ot->GetPoint(2)->x;
									t2.a.Y = ot->GetPoint(2)->y;
									t2.a.Z = mExtrude;

									t2.c.X = ot->GetPoint(0)->x;
									t2.c.Y = ot->GetPoint(0)->y;
									t2.c.Z = mExtrude;

									t2.b.X = ot->GetPoint(1)->x;
									t2.b.Y = ot->GetPoint(1)->y;
									t2.b.Z = mExtrude;

									mTris[1].Add(t2);
								}
							}
						
							
						}

					}
				}

			}
		}

	}
	void Shape(FVector2D start = FVector2D(0,0))
	{
		FVector2D offset = start;
		
		hb_font_t* hbFont = hb_ft_font_create(mFontFace, nullptr);
		if (hbFont == nullptr) return;
		hb_buffer_t* hbBuffer = hb_buffer_create();
		if (hbBuffer == nullptr) return;


		hb_script_t hbScript = hb_script_from_string(mScript, -1);

		TArray<FString> linesText;
		mText.ParseIntoArrayLines(linesText, false);
		int curLineMaxHeight = 0;

		for (int iLine = 0; iLine < linesText.Num(); iLine++) //for each line
		{
			curLineMaxHeight = 0;
			

			FString& lineText = linesText[iLine];

			TArray<TextBiDi::FTextDirectionInfo> directionsInfo;
			TextBiDi::ComputeTextDirection(lineText, TextBiDi::ComputeBaseDirection(lineText), directionsInfo);

			for (TextBiDi::FTextDirectionInfo dirInfo : directionsInfo) //for each section
			{
				hb_direction_t curDir = dirInfo.TextDirection == TextBiDi::ETextDirection::LeftToRight ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;

				hb_buffer_reset(hbBuffer);

				hb_buffer_set_direction(hbBuffer, curDir);
				hb_buffer_set_script(hbBuffer, hbScript);
				hb_buffer_set_language(hbBuffer, mTextLanguage);

				size_t length = dirInfo.Length;
				
				const uint16_t* sectionText = ((const uint16_t*)(*lineText)) + dirInfo.StartIndex;
				hb_buffer_add_utf16(hbBuffer, sectionText, length, 0, length);
				//hb_buffer_guess_segment_properties(hbBuffer);
				hb_shape(hbFont, hbBuffer, nullptr, 0);

				unsigned int glyphCount;
				hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
				hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

				if (glyphInfo == nullptr || glyphPos == nullptr) return;

				

				for (unsigned iGlyph = 0; iGlyph < glyphCount; iGlyph++) //for each glyph
				{
					//index in glyph map
					auto codePoint = glyphInfo[iGlyph].codepoint;
					//utf16 code
					auto characterCode = sectionText[glyphInfo[iGlyph].cluster];
					if (FT_Load_Glyph(mFontFace, codePoint, FT_LOAD_DEFAULT))
					{
						UE_LOG(Text3D, Error, TEXT("FT_Load_Glyph failed"));
						return;
					}
					
					auto glyph = mFontFace->glyph;
					if (glyph->format != FT_GLYPH_FORMAT_OUTLINE)
					{
						UE_LOG(Text3D, Error, TEXT("glyph must be FT_GLYPH_FORMAT_OUTLINE"));
						return;
					}

					curLineMaxHeight = FMath::Max(curLineMaxHeight, (int)glyph->metrics.height / 64);

					short nCountour = 0;
					nCountour = glyph->outline.n_contours;

					int startIndex = 0, endIndex = 0;

					FVector2D glyphAdvace = FVector2D((float)glyphPos[iGlyph].x_advance / 64, (float)glyphPos[iGlyph].y_advance / 64);
					FVector2D glyphOffset = FVector2D((float)glyphPos[iGlyph].x_offset / 64, (float)glyphPos[iGlyph].y_offset / 64);

					//float xa = (float)glyphPos[iGlyph].x_advance / 64;
					//float ya = (float)glyphPos[iGlyph].y_advance / 64;
					//float xo = (float)glyphPos[iGlyph].x_offset / 64;
					//float yo = (float)glyphPos[iGlyph].y_offset / 64;


					if (characterCode == '\t')
					{
						glyphAdvace *= 3; //how many space is a tab?
					}
					else if (characterCode == ' ')
					{

					}
					else
					{
						TriangulateGlyph_P2T(glyph, codePoint, offset + glyphOffset);
					}

					//x += xa;
					//y += ya;

					offset += glyphAdvace;


				}

				
			}

			
			offset.X = start.X;
			offset.Y -= (mLineSpace);
		}
		

		hb_buffer_destroy(hbBuffer);
		hb_font_destroy(hbFont);
	}
	//returns the bounding box from mTris
	FBox CalcBound() const
	{
		FBox box(ForceInit);

		for (int iMesh = 0; iMesh < 3; iMesh++)
		{
			for (int iTri = 0; iTri < mTris[iMesh].Num(); iTri++)
			{
				box += mTris[iMesh][iTri].a;
				box += mTris[iMesh][iTri].b;
				box += mTris[iMesh][iTri].c;
			}
		}

		return box;
	}
	//moves the mTris
	void ApplyAlignment()
	{
		FBox bound = CalcBound();

		FVector v;

		if (mHTA == EText3DHAlign::LEFT)
			v.X = -bound.Min.X;
		else if (mHTA == EText3DHAlign::RIGHT)
			v.X = -bound.Max.X;
		else
			v.X = -bound.GetCenter().X;

		if (mVTA == EText3DVAlign::BOTTOM)
			v.Y = -bound.Min.Y;
		else if (mVTA == EText3DVAlign::TOP)
			v.Y = -bound.Max.Y;
		else
			v.Y = -bound.GetCenter().Y;

		for (int iMesh = 0; iMesh < 3; iMesh++)
			for (FTri& tri : mTris[iMesh])
				tri.Move(v);
	}
	void ApplyTranformation()
	{
		for (int iMesh = 0; iMesh < 3; iMesh++)
			for (FTri& tri : mTris[iMesh])
				tri = tri * mTransform;
	}
	FMeshResultFinal* GetMesh()
	{
		ApplyAlignment();
		ApplyTranformation();

		FMeshResultFinal* result = new FMeshResultFinal;
		for (int iMesh = 0; iMesh < 3; iMesh++)
		{

			UIndexingTriFlatNormal(mTris[iMesh], 
				result->mMeshes[iMesh].vertices, 
				result->mMeshes[iMesh].indices);
		}
		return result;
	}
	void GenSideTri(const double* point0, const double* point1, FVector vOffset)
	{
		FTri t1;
		t1.a = FVector(point0[0], point0[1], 0) / 64.0f + vOffset;
		t1.b = FVector(point1[0], point1[1], 0) / 64.0f + vOffset;
		t1.c = t1.a + FVector(0, 0, mExtrude);

		mTris[2].Add(t1);

		FTri t2;
		t2.a = FVector(point1[0], point1[1], mExtrude) / FVector(64, 64, 1) + vOffset;
		t2.b = FVector(point0[0], point0[1], mExtrude) / FVector(64, 64, 1) + vOffset;
		t2.c = t2.a * FVector(1, 1, 0);

		mTris[2].Add(t2);

	}
};



FT_Library GetFreeTypeLib()
{
	struct FTLib
	{
		FT_Library Instance = nullptr;
		FT_Memory CustomMemory;

		FTLib()
		{
// 			CustomMemory = static_cast<FT_Memory>(FMemory::Malloc(sizeof(*CustomMemory)));
// 
// 			// Init FreeType
// 			CustomMemory->alloc = &FMemory::Alloc;
// 			CustomMemory->realloc = &FreeTypeMemory::Realloc;
// 			CustomMemory->free = &FreeTypeMemory::Free;
// 			CustomMemory->user = nullptr;
// 
// 			FT_Error Error = FT_New_Library(CustomMemory, &FTLibrary);
// 
// 			if (Error)
// 			{
// 				checkf(0, TEXT("Could not init FreeType. Error code: %d"), Error);
// 			}
			FT_Error err = FT_Init_FreeType(&Instance);
			if (err)
			{
				UE_LOG(Text3D, Error, TEXT("Failed to get free type library"));
			}
			FT_Add_Default_Modules(Instance);
		}
		~FTLib()
		{
			FT_Done_Library(Instance);
		}
	};
	static FTLib FTLibrary;
	return FTLibrary.Instance;
};
#endif

UText3DComponent::UText3DComponent()
{
	BezierStep = 3;
	Depth = 10;
	bGenerateBackFace = true;
	bGenerateFronFace = true;
	bGenerateSide = true;
	HorizontalAlignment = EText3DHAlign::CENTER;
	VerticalAlignment = EText3DVAlign::CENTER;
	Transform = FTransform(FRotator(0, 0, -90), FVector(0,0,0), FVector(1,1,1));
	LineSpace = 32;
}






#if WITH_EDITOR
void UText3DComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		FName prpName = PropertyChangedEvent.Property->GetFName();
		if (prpName == GET_MEMBER_NAME_CHECKED(UText3DComponent, Text)
			|| prpName == GET_MEMBER_NAME_CHECKED(UText3DComponent, Font)
			|| prpName == GET_MEMBER_NAME_CHECKED(UText3DComponent, BezierStep)
			)
		{
		}
	}

	UpdateMesh();
}
#endif

void UText3DComponent::UpdateMesh()
{
#if WITH_FREETYPE && WITH_HARFBUZZ
	
	this->MarkRenderStateDirty();

	if (Font == nullptr || Text.IsEmpty()) return;
	if (!Font->FontFaceData->HasData()) return;

	FTextShaper* textShaper = new FTextShaper(this);

	AsyncTask(ENamedThreads::AnyThread, [this, textShaper]() {
		GenerateMesh(textShaper);
		FMeshResultFinal* mesh = textShaper->GetMesh();
		delete textShaper;



		AsyncTask(ENamedThreads::GameThread, [this, mesh]() {
			UE_LOG(Text3D, Log, TEXT("Applying generated mesh"));
			GeneratedMesh = MakeShareable(mesh);
			this->UpdateBounds();
			this->MarkRenderStateDirty();
		});
	});
	
#endif
}

FMeshResultFinal* UText3DComponent::GetGeneratedMesh() const
{
	return GeneratedMesh.Get();
}

int32 UText3DComponent::GetNumMaterials() const
{
	return 3;	//front , back, side
}

FBoxSphereBounds UText3DComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (GeneratedMesh.IsValid())
	{
		FBox box = GeneratedMesh->CalcBound().TransformBy(LocalToWorld);
		return FBoxSphereBounds(box);
	}
	return Super::CalcBounds(LocalToWorld);
}

void UText3DComponent::OnRegister()
{
	Super::OnRegister();
	UpdateMesh();
}

void UText3DComponent::GenerateMesh(FTextShaper* textShaper)
{
#if WITH_FREETYPE && WITH_HARFBUZZ
	{
		//this is the content of a font file
		const TArray<uint8>& fontData = Font->FontFaceData->GetData();
		FT_Library lib = GetFreeTypeLib();
		if (lib == nullptr)
		{
			UE_LOG(Text3D, Error, TEXT("Failed to get true type library"));
		}
		FT_Face face = nullptr;
		FT_Error error = FT_New_Memory_Face(GetFreeTypeLib(), (const FT_Byte*)fontData.GetData(), (FT_Long)fontData.Num(), 0, &face);
		if (error)
		{
			UE_LOG(Text3D, Error, TEXT("Failed to load face"));
			return;
		}

		unsigned width = 64;
		unsigned height = 64;
		FT_Set_Char_Size(face, width << 6, height << 6, 96, 96);

		textShaper->mFontFace = face;
		textShaper->Shape();
	}
#endif
}

