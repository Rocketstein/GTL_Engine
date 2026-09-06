#include "Asset/Builder/TextureBuilder.h"
#include "Asset/Cache/AssetKeyUtils.h"
#include "Asset/Core/AssetNaming.h"
#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Asset/Source/SourceLoader.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include "ThirdParty/stb/stb_image.h"
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace Asset
{
    namespace
    {
        static FTextureMipLevel BuildNextMipLevel(const FTextureMipLevel &ParentMip, uint32 Channels)
        {
            FTextureMipLevel ChildMip;
            ChildMip.Width = std::max(1u, ParentMip.Width / 2u);
            ChildMip.Height = std::max(1u, ParentMip.Height / 2u);

            const size_t PixelCount =
                static_cast<size_t>(ChildMip.Width) * static_cast<size_t>(ChildMip.Height);
            ChildMip.Pixels.resize(PixelCount * static_cast<size_t>(Channels));

            for (uint32 Y = 0; Y < ChildMip.Height; ++Y)
            {
                for (uint32 X = 0; X < ChildMip.Width; ++X)
                {
                    const uint32 SrcX0 = std::min(ParentMip.Width - 1u, X * 2u);
                    const uint32 SrcY0 = std::min(ParentMip.Height - 1u, Y * 2u);
                    const uint32 SrcX1 = std::min(ParentMip.Width - 1u, SrcX0 + 1u);
                    const uint32 SrcY1 = std::min(ParentMip.Height - 1u, SrcY0 + 1u);

                    for (uint32 C = 0; C < Channels; ++C)
                    {
                        const size_t I00 = (static_cast<size_t>(SrcY0) * ParentMip.Width + SrcX0) *
                                               static_cast<size_t>(Channels) +
                                           C;
                        const size_t I10 = (static_cast<size_t>(SrcY0) * ParentMip.Width + SrcX1) *
                                               static_cast<size_t>(Channels) +
                                           C;
                        const size_t I01 = (static_cast<size_t>(SrcY1) * ParentMip.Width + SrcX0) *
                                               static_cast<size_t>(Channels) +
                                           C;
                        const size_t I11 = (static_cast<size_t>(SrcY1) * ParentMip.Width + SrcX1) *
                                               static_cast<size_t>(Channels) +
                                           C;

                        const uint32 Average = static_cast<uint32>(ParentMip.Pixels[I00]) +
                                               static_cast<uint32>(ParentMip.Pixels[I10]) +
                                               static_cast<uint32>(ParentMip.Pixels[I01]) +
                                               static_cast<uint32>(ParentMip.Pixels[I11]);
                        const size_t DestIndex =
                            (static_cast<size_t>(Y) * ChildMip.Width + X) *
                                static_cast<size_t>(Channels) +
                            C;
                        ChildMip.Pixels[DestIndex] = static_cast<uint8>(Average / 4u);
                    }
                }
            }

            return ChildMip;
        }
    } // namespace

    FTextureBuilder::FTextureBuilder(FAssetBuildCache &InCache) : Cache(InCache) {}

    std::shared_ptr<FTextureCookedData>
    FTextureBuilder::Build(const std::filesystem::path &Path, const FTextureBuildSettings &Settings)
    {
        LastBuildReport.Reset();
        const FSourceRecord *Source = Cache.GetSource(FTextureAssetTag{}, Path);
        if (Source == nullptr)
        {
            return nullptr;
        }

        const FString BakedTexturePath =
            MakeBakedAssetPath(FPaths::Utf8FromPath(Source->NormalizedPath));
        if (!BakedTexturePath.empty())
        {
            FTextureCookedData BakedData;
            if (Binary::LoadTextureIfCurrent(BakedTexturePath, *Source, BakedData) &&
                BakedData.IsValid())
            {
                UE_LOG(AssetCache, ELogLevel::Debug,
                       "TextureBuilder: using baked texture cache (%s), mipCount=%u, base=%ux%u",
                       BakedTexturePath.c_str(), static_cast<unsigned>(BakedData.GetMipCount()),
                       static_cast<unsigned>(BakedData.Width), static_cast<unsigned>(BakedData.Height));
                LastBuildReport.bUsedCachedCooked = true;
                LastBuildReport.ResultSource = EAssetBuildResultSource::CookedCache;
                return std::make_shared<FTextureCookedData>(std::move(BakedData));
            }
        }

        // Week4 lightweight path:
        // - baked binary가 있으면 그것을 최우선 사용
        // - 없으면 source -> decode -> cooked 로 바로 진행
        // - intermediate cache는 메모리 계층을 늘리기만 해서 사용하지 않음
        auto Intermediate = std::make_shared<FIntermediateTextureData>();
        if (!DecodeTexture(*Source, *Intermediate))
        {
            return nullptr;
        }

        const FTextureIntermediateKey IntermediateKey =
            KeyUtils::MakeIntermediateKey(*Intermediate);
        const FTextureCookedKey CookedKey = KeyUtils::MakeCookedKey(IntermediateKey, Settings);

        auto                               &CookedCache = Cache.GetCookedCache(FTextureAssetTag{});
        std::shared_ptr<FTextureCookedData> Cooked = CookedCache.Find(CookedKey);

        if (!Cooked)
        {
            LastBuildReport.bBuiltNewCooked = true;
            Cooked = CookTexture(*Source, *Intermediate, Settings);
            if (!Cooked)
            {
                return nullptr;
            }

            CookedCache.Insert(CookedKey, Cooked);
        }
        else
        {
            LastBuildReport.bUsedCachedCooked = true;
        }

        if (LastBuildReport.bUsedCachedCooked)
        {
            LastBuildReport.ResultSource = EAssetBuildResultSource::CookedCache;
        }
        else if (Cooked)
        {
            LastBuildReport.ResultSource = EAssetBuildResultSource::BuiltFromFreshIntermediate;
        }

        if (Cooked && !BakedTexturePath.empty())
        {
            Binary::SaveTexture(*Cooked, BakedTexturePath, Source);
        }

        return Cooked;
    }

    bool FTextureBuilder::DecodeTexture(const FSourceRecord      &Source,
                                        FIntermediateTextureData &OutData) const
    {
        TArray<uint8> SourceBytes;
        if (!FSourceLoader::ReadAllBytes(Source.NormalizedPath, SourceBytes) || SourceBytes.empty())
        {
            return false;
        }

        int Width = 0;
        int Height = 0;
        int ChannelsInFile = 0;

        stbi_uc *DecodedPixels =
            stbi_load_from_memory(SourceBytes.data(), static_cast<int>(SourceBytes.size()), &Width,
                                  &Height, &ChannelsInFile, STBI_rgb_alpha);

        if (DecodedPixels == nullptr)
        {
            return false;
        }

        if (Width <= 0 || Height <= 0)
        {
            stbi_image_free(DecodedPixels);
            return false;
        }

        const size_t PixelBytes = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;

        OutData.Width = static_cast<uint32>(Width);
        OutData.Height = static_cast<uint32>(Height);
        OutData.Channels = 4;
        OutData.Pixels.resize(PixelBytes);

        std::memcpy(OutData.Pixels.data(), DecodedPixels, PixelBytes);
        stbi_image_free(DecodedPixels);
        return true;
    }

    std::shared_ptr<FTextureCookedData>
    FTextureBuilder::CookTexture(const FSourceRecord            &Source,
                                 const FIntermediateTextureData &Intermediate,
                                 const FTextureBuildSettings    &Settings) const
    {
        if (Intermediate.Width == 0 || Intermediate.Height == 0)
        {
            return nullptr;
        }

        if (Intermediate.Pixels.empty())
        {
            return nullptr;
        }

        auto Result = std::make_shared<FTextureCookedData>();
        Result->SourcePath = Source.NormalizedPath;
        Result->Width = Intermediate.Width;
        Result->Height = Intermediate.Height;
        Result->Channels = Intermediate.Channels;
        Result->Pixels = Intermediate.Pixels;
        Result->bSRGB = Settings.bSRGB;

        FTextureMipLevel BaseMip;
        BaseMip.Width = Result->Width;
        BaseMip.Height = Result->Height;
        BaseMip.Pixels = Result->Pixels;
        Result->MipChain.push_back(std::move(BaseMip));

        if (Settings.bGenerateMips)
        {
            while (true)
            {
                const FTextureMipLevel &LastMip = Result->MipChain.back();
                if (LastMip.Width == 1u && LastMip.Height == 1u)
                {
                    break;
                }

                Result->MipChain.push_back(BuildNextMipLevel(LastMip, Result->Channels));
            }
        }

        UE_LOG(AssetCache, ELogLevel::Debug,
               "TextureBuilder: cooked texture mip chain generated (%s), mipCount=%u, base=%ux%u",
               Source.NormalizedPath.string().c_str(), static_cast<unsigned>(Result->GetMipCount()),
               static_cast<unsigned>(Result->Width), static_cast<unsigned>(Result->Height));

        return Result->IsValid() ? Result : nullptr;
    }

} // namespace Asset
