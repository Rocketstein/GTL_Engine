# 에셋과 외부 의존성

이 저장소는 공개용 소스 스냅샷입니다. 원본 파일은 소유자의 비공개 저장소와 로컬 스테이징에 보존되어 있습니다.

## 제외 범위

- 에셋: 이미지·텍스처, OBJ/FBX 등 메시, MTL/머티리얼, 폰트, 음원, 씬, Unreal 형식 패키지
- 빌드 산출물: EXE, DLL, LIB, PDB, OBJ, 캐시, `Bin`, `Build`, `Intermediate`, `x64`
- 패키지 복원 결과: `packages`와 번들 Python 런타임
- 상용·제한적 SDK: FMOD, Autodesk FBX SDK
- 라이선스를 정확히 대응시키지 못한 구형 DirectXTK 벤더 사본
- PhysX·NvCloth·RmlUi·LuaJIT·SoLoud 등에서 소스와 함께 들어 있던 사전 빌드 바이너리

## 포함한 외부 코드

| 구성 요소 | 포함 범위 | 확인한 조건 |
| --- | --- | --- |
| Dear ImGui `1.92.6 WIP` / `1.92.7 WIP` | 핵심 소스, Win32/DX11 백엔드 | MIT |
| SoLoud | W01의 공개 헤더 | zlib/libpng 스타일 라이선스 |
| SimpleJSON | W02–W03, W06–W14의 단일 헤더 | WTFPL v2 |
| nlohmann/json `3.12.0` | W04–W05의 단일 헤더 | MIT |
| stb_image `2.30` | W05의 헤더와 구현 파일 | Public Domain 또는 MIT |
| miniaudio `0.11.22` | W09의 단일 파일 라이브러리 | Public Domain 또는 MIT-0 |
| Lua `5.4.8` | W10의 소스와 헤더 | MIT |
| LuaJIT `2.1.1774896198` | W11–W14의 공개 헤더만 | MIT |
| sol2 `3.2.3` / `3.5.0` | W09–W14의 헤더 | MIT |
| RmlUi | W11–W14의 공개 헤더만 | MIT |
| imgui-node-editor `0.9.4` | W13–W14의 소스와 헤더 | MIT |
| NVIDIA PhysX `4.1.2` | W13의 소스·헤더·빌드 설정, W14의 헤더 | BSD-3-Clause |
| NVIDIA NvCloth `1.1.6` | W13–W14의 소스와 헤더 | Nvidia Source Code License (1-Way Commercial) |
| NVIDIA PxShared | W13의 PhysX 포함 헤더, W14의 헤더 | PhysX BSD-3-Clause 또는 Nvidia Source Code License (사본별 고지 참조) |

각 사본의 라이선스 파일 또는 파일 내부 고지를 보존했습니다. ImGui 폴더에 섞여 있던 Font Awesome·NanumGothic 글꼴, 생성 아이콘 헤더와 별도 확장 코드는 포함하지 않았습니다. 세부 경로와 원 배포처는 [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)를 참고하세요.

## 주차별 주요 의존성

| 주차 | 원본에서 확인된 주요 의존성 |
| --- | --- |
| W01 | Dear ImGui, SoLoud, DirectXTK Desktop 2019 `2025.10.28.2` |
| W02 | Dear ImGui, SimpleJSON |
| W03 | Dear ImGui, SimpleJSON, DirectXTK |
| W04 | Dear ImGui, nlohmann/json, DirectXTK Desktop Win10 `2025.10.28.2` |
| W05 | Dear ImGui, stb_image, nlohmann/json |
| W06–W08 | Dear ImGui, SimpleJSON, DirectXTK Desktop Win10 `2025.10.28.2` |
| W09 | 위 항목 + miniaudio, LuaJIT Native `2.1.1739213504`, sol2 |
| W10 | 위 항목 + FMOD, Autodesk FBX SDK, Lua, sol2 |
| W11–W12 | 위 항목 + RmlUi, NVIDIA PhysX `4.1.229882250`, DirectXTK Desktop Win10 `2026.5.8.1` |
| W13 | 위 항목 + PhysX 소스, PxShared, NvCloth, imgui-node-editor; DirectXTK `2025.10.28.2` |
| W14 | 위 항목 + PxShared, NvCloth, imgui-node-editor; DirectXTK `2025.10.28.2` |

제외된 SDK와 패키지는 원 배포처에서 사용자가 직접 받아 해당 라이선스에 따라 설치해야 합니다. `packages.config`가 남아 있는 주차는 NuGet 버전 확인용으로 사용할 수 있습니다. 프로젝트 파일의 경로 참조는 원본 구조를 설명하기 위해 그대로 두었으므로, 로컬에서 복원할 때 그 경로에 맞춰야 합니다.
