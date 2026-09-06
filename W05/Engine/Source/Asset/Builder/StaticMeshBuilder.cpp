#include "Asset/Builder/StaticMeshBuilder.h"
#include "Asset/Core/AssetNaming.h"
#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct FObjIndex
    {
        // OBJ index rule:
        //  - positive: 1-based absolute index
        //  - negative: relative index from the current end (-1 = last)
        //  - zero/empty: invalid
        int PositionIndex = 0;
        int TexCoordIndex = 0;
    };

    static std::string Trim(const std::string &In)
    {
        const size_t Begin = In.find_first_not_of(" \t\r\n");
        if (Begin == std::string::npos)
        {
            return {};
        }

        const size_t End = In.find_last_not_of(" \t\r\n");
        return In.substr(Begin, End - Begin + 1);
    }

    static bool ParseFaceVertex(const std::string &Token, FObjIndex &OutIndex)
    {
        const size_t Slash0 = Token.find('/');
        if (Slash0 == std::string::npos)
        {
            OutIndex.PositionIndex = std::stoi(Token);
            OutIndex.TexCoordIndex = 0;
            return true;
        }

        const std::string PosPart = Token.substr(0, Slash0);
        const size_t      Slash1 = Token.find('/', Slash0 + 1);

        std::string UvPart;
        if (Slash1 == std::string::npos)
        {
            UvPart = Token.substr(Slash0 + 1);
        }
        else
        {
            UvPart = Token.substr(Slash0 + 1, Slash1 - Slash0 - 1);
        }

        if (PosPart.empty())
        {
            return false;
        }

        OutIndex.PositionIndex = std::stoi(PosPart);
        OutIndex.TexCoordIndex = UvPart.empty() ? 0 : std::stoi(UvPart);
        return true;
    }

    static int ResolveObjIndex(int RawObjIndex, int ElementCount)
    {
        if (RawObjIndex > 0)
        {
            const int ZeroBased = RawObjIndex - 1;
            return (ZeroBased >= 0 && ZeroBased < ElementCount) ? ZeroBased : -1;
        }

        if (RawObjIndex < 0)
        {
            const int ZeroBased = ElementCount + RawObjIndex;
            return (ZeroBased >= 0 && ZeroBased < ElementCount) ? ZeroBased : -1;
        }

        return -1;
    }
} // namespace

namespace Asset
{

    std::shared_ptr<FObjCookedData>
    FStaticMeshBuilder::Build(const std::filesystem::path    &Path,
                              const FStaticMeshBuildSettings &Settings)
    {
        (void)Settings;

        LastBuildReport.Reset();
        const FSourceRecord *Source = Cache.GetSource(FStaticMeshAssetTag{}, Path);
        if (Source == nullptr)
        {
            return nullptr;
        }

        const FString BakedMeshPath = MakeBakedAssetPath(FPaths::Utf8FromPath(Source->NormalizedPath));
        if (!BakedMeshPath.empty())
        {
            FObjCookedData BakedData;
            if (Binary::LoadStaticMeshIfCurrent(BakedMeshPath, *Source, BakedData) &&
                BakedData.IsValid())
            {
                LastBuildReport.bUsedCachedCooked = true;
                LastBuildReport.ResultSource = EAssetBuildResultSource::CookedCache;
                return std::make_shared<FObjCookedData>(std::move(BakedData));
            }
        }

        std::ifstream File(Source->NormalizedPath, std::ios::in);
        if (!File.is_open())
        {
            return nullptr;
        }

        std::vector<FVector3> Positions;
        std::vector<FVector2> TexCoords;

        auto Cooked = std::make_shared<FObjCookedData>();
        Cooked->SourcePath = Source->NormalizedPath;
        Cooked->VertexStride = sizeof(FStaticMeshVertexPT);
        Cooked->VertexCount = 0;
        Cooked->VertexData.clear();
        Cooked->Indices.clear();
        Cooked->Sections.clear();
        Cooked->MaterialLibraries.clear();
        Cooked->Materials.clear();

        std::unordered_map<FString, uint32> MaterialNameToIndex;
        uint32                              CurrentMaterialIndex = 0;
        uint32                              ActiveSectionMaterialIndex = 0;
        uint32                              SectionStartIndex = 0;
        bool                                bHasOpenSection = false;

        std::string Line;
        while (std::getline(File, Line))
        {
            Line = Trim(Line);
            if (Line.empty() || Line[0] == '#')
            {
                continue;
            }

            std::istringstream Stream(Line);
            std::string        Keyword;
            Stream >> Keyword;

            if (Keyword == "v")
            {
                float X = 0.0f, Y = 0.0f, Z = 0.0f;
                Stream >> X >> Y >> Z;
                Positions.emplace_back(X, Y, Z);
            }
            else if (Keyword == "vt")
            {
                float U = 0.0f, V = 0.0f;
                Stream >> U >> V;
                // OBJ UV는 일반적으로 좌하단 원점을 기준으로 저장되고,
                // D3D 텍스처 샘플링은 상단 원점을 사용하므로 V축을 뒤집어서 맞춘다.
                TexCoords.emplace_back(U, 1.0f - V);
                Cooked->bHasUVs = true;
            }
            else if (Keyword == "mtllib")
            {
                FString RelativeMtlPath;
                Stream >> RelativeMtlPath;
                if (!RelativeMtlPath.empty())
                {
                    std::filesystem::path LibraryPath = Source->NormalizedPath.parent_path() / RelativeMtlPath;
                    Cooked->MaterialLibraries.push_back(LibraryPath.lexically_normal());
                }
            }
            else if (Keyword == "usemtl")
            {
                FString MaterialName;
                Stream >> MaterialName;
                if (MaterialName.empty())
                {
                    continue;
                }

                auto FoundMaterial = MaterialNameToIndex.find(MaterialName);
                if (FoundMaterial != MaterialNameToIndex.end())
                {
                    CurrentMaterialIndex = FoundMaterial->second;
                }
                else
                {
                    FObjCookedMaterialRef MaterialRef{};
                    MaterialRef.Name = MaterialName;
                    MaterialRef.LibraryIndex =
                        Cooked->MaterialLibraries.empty()
                            ? 0u
                            : static_cast<uint32>(Cooked->MaterialLibraries.size() - 1);

                    CurrentMaterialIndex = static_cast<uint32>(Cooked->Materials.size());
                    Cooked->Materials.push_back(MaterialRef);
                    MaterialNameToIndex.emplace(MaterialName, CurrentMaterialIndex);
                }

                if (bHasOpenSection && SectionStartIndex < Cooked->Indices.size())
                {
                    FStaticMeshSectionData Section{};
                    Section.StartIndex = SectionStartIndex;
                    Section.IndexCount =
                        static_cast<uint32>(Cooked->Indices.size() - SectionStartIndex);
                    Section.MaterialIndex = ActiveSectionMaterialIndex;
                    Cooked->Sections.push_back(Section);
                    SectionStartIndex = static_cast<uint32>(Cooked->Indices.size());
                }
                ActiveSectionMaterialIndex = CurrentMaterialIndex;
            }
            else if (Keyword == "f")
            {
                std::vector<FObjIndex> Face;
                std::string            Token;
                while (Stream >> Token)
                {
                    FObjIndex Index;
                    if (ParseFaceVertex(Token, Index))
                    {
                        Face.push_back(Index);
                    }
                }

                if (Face.size() < 3)
                {
                    continue;
                }

                auto AppendVertex = [&](const FObjIndex &Index) -> uint32
                {
                    FStaticMeshVertexPT Vertex{};
                    const int PositionIndex =
                        ResolveObjIndex(Index.PositionIndex, static_cast<int>(Positions.size()));
                    const int TexCoordIndex =
                        ResolveObjIndex(Index.TexCoordIndex, static_cast<int>(TexCoords.size()));

                    if (PositionIndex >= 0 && PositionIndex < static_cast<int>(Positions.size()))
                    {
                        Vertex.Position = Positions[PositionIndex];
                    }

                    if (TexCoordIndex >= 0 && TexCoordIndex < static_cast<int>(TexCoords.size()))
                    {
                        Vertex.TexCoord = TexCoords[TexCoordIndex];
                    }
                    else
                    {
                        Vertex.TexCoord = FVector2(0.0f, 0.0f);
                    }

                    const uint32 VertexIndex = static_cast<uint32>(Cooked->VertexData.size() /
                                                                   sizeof(FStaticMeshVertexPT));

                    const uint8 *Bytes = reinterpret_cast<const uint8 *>(&Vertex);
                    Cooked->VertexData.insert(Cooked->VertexData.end(), Bytes,
                                              Bytes + sizeof(FStaticMeshVertexPT));

                    return VertexIndex;
                };

                for (size_t i = 1; i + 1 < Face.size(); ++i)
                {
                    const uint32 I0 = AppendVertex(Face[0]);
                    const uint32 I1 = AppendVertex(Face[i]);
                    const uint32 I2 = AppendVertex(Face[i + 1]);

                    Cooked->Indices.push_back(I0);
                    Cooked->Indices.push_back(I1);
                    Cooked->Indices.push_back(I2);
                }

                bHasOpenSection = true;
            }
        }

        Cooked->VertexCount =
            static_cast<uint32>(Cooked->VertexData.size() / sizeof(FStaticMeshVertexPT));

        if (bHasOpenSection && SectionStartIndex < Cooked->Indices.size())
        {
            FStaticMeshSectionData Section{};
            Section.StartIndex = SectionStartIndex;
            Section.IndexCount = static_cast<uint32>(Cooked->Indices.size() - SectionStartIndex);
            Section.MaterialIndex = ActiveSectionMaterialIndex;
            Cooked->Sections.push_back(Section);
        }

        if (Cooked->Sections.empty() && !Cooked->Indices.empty())
        {
            FStaticMeshSectionData Section{};
            Section.StartIndex = 0;
            Section.IndexCount = static_cast<uint32>(Cooked->Indices.size());
            Section.MaterialIndex = 0;
            Cooked->Sections.push_back(Section);
        }

        UE_LOG(StaticMeshBuilder, ELogLevel::Info,
               "LayoutCheck: FVector3=%zu FVector2=%zu PT=%zu PosOff=%zu UvOff=%zu VertexStride=%u VertexCount=%u IndexCount=%zu",
               sizeof(FVector3), sizeof(FVector2), sizeof(FStaticMeshVertexPT),
               offsetof(FStaticMeshVertexPT, Position), offsetof(FStaticMeshVertexPT, TexCoord),
               Cooked->VertexStride, Cooked->VertexCount, Cooked->Indices.size());

        LastBuildReport.bBuiltNewCooked = true;
        LastBuildReport.ResultSource = EAssetBuildResultSource::BuiltFromFreshIntermediate;

        if (Cooked->IsValid() && !BakedMeshPath.empty())
        {
            Binary::SaveStaticMesh(*Cooked, BakedMeshPath, Source);
        }

        return Cooked;
    }

} // namespace Asset
