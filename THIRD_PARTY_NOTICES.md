# Third-party notices

이 문서는 공개본에 실제로 포함된 외부 소스와 헤더를 기록합니다. 각 사본에 포함된 라이선스 파일 또는 파일 내부 고지를 제거하지 마세요.

## Dear ImGui

MIT 라이선스. 핵심 소스와 Win32/DX11 백엔드만 포함합니다.

- `W01/GTL/ImGui`
- `W02/Week2/ImGui`
- `W03/ImGui`
- `W04/Editor/Source/ThirdParty/ImGui`
- `W05/Engine/Source/ThirdParty/ImGui`
- `W05/Engine/ThirdParty/ImGui`
- `W06/NipsEngine/ThirdParty/ImGui`
- `W07/NipsEngine/ThirdParty/ImGui`
- `W08/KraftonEngine/ThirdParty/ImGui`
- `W09/LunaticEngine/ThirdParty/ImGui`
- `W10/PacificEngine/ThirdParty/ImGui`
- `W11/KraftonEngine/ThirdParty/ImGui`
- `W12/KraftonEngine/ThirdParty/ImGui`
- `W13/KraftonEngine/ThirdParty/ImGui`
- `W14/KraftonEngine/ThirdParty/ImGui`

Upstream: <https://github.com/ocornut/imgui>

사본은 `1.92.6 WIP` 또는 `1.92.7 WIP`로 식별됩니다. 원본 폴더에 함께 있던 글꼴, 생성 아이콘 헤더와 별도 확장 코드는 제외했습니다.

## SoLoud

zlib/libpng 스타일 라이선스. `W01/GTL/soloud/include`의 헤더만 포함하며 `soloud_static.lib`는 제외했습니다.

Upstream: <https://github.com/jarikomppa/soloud>

## SimpleJSON

WTFPL v2. 다음 단일 헤더 사본과 각 폴더의 `LICENSE.txt`를 포함합니다.

- `W02/Week2/SimpleJSON`
- `W03/SimpleJSON`
- `W06/NipsEngine/ThirdParty/SimpleJSON`
- `W07/NipsEngine/ThirdParty/SimpleJSON`
- `W08/KraftonEngine/ThirdParty/SimpleJSON`
- `W09/LunaticEngine/ThirdParty/SimpleJSON`
- `W10/PacificEngine/ThirdParty/SimpleJSON`
- `W11/KraftonEngine/ThirdParty/SimpleJSON`
- `W12/KraftonEngine/ThirdParty/SimpleJSON`
- `W13/KraftonEngine/ThirdParty/SimpleJSON`
- `W14/KraftonEngine/ThirdParty/SimpleJSON`

Upstream: <https://github.com/nbsdx/SimpleJSON>

## nlohmann/json

MIT 라이선스, 버전 `3.12.0`.

- `W04/Engine/Source/ThirdParty/Json`
- `W05/Engine/Source/ThirdParty/nlohmann`
- `W05/Engine/ThirdParty/nlohmann`

Upstream: <https://github.com/nlohmann/json>

## stb_image

Public Domain 또는 MIT 조건을 선택할 수 있습니다. 파일 내부 라이선스 고지를 보존했습니다.

- `W05/Engine/Source/ThirdParty/stb`
- `W05/Engine/ThirdParty/stb`

Upstream: <https://github.com/nothings/stb>

## miniaudio

Public Domain 또는 MIT-0 조건을 선택할 수 있습니다. 파일 내부 라이선스 고지를 보존했습니다.

- `W09/LunaticEngine/ThirdParty/miniaudio`

Upstream: <https://github.com/mackron/miniaudio>

## Lua and LuaJIT

MIT 라이선스.

- Lua `5.4.8`: `W10/PacificEngine/ThirdParty/Lua/src`
- LuaJIT `2.1.1774896198` 헤더: `W11/KraftonEngine/ThirdParty/lua/include`부터 `W14/KraftonEngine/ThirdParty/lua/include`까지

LuaJIT의 DLL/LIB 사본은 포함하지 않았습니다.

Upstream: <https://www.lua.org/> and <https://luajit.org/>

## sol2

MIT 라이선스.

- `W09/LunaticEngine/ThirdParty/sol2` (`3.5.0`)
- `W10/PacificEngine/ThirdParty/Sol` (`3.2.3`)
- `W11/KraftonEngine/ThirdParty/sol2`부터 `W14/KraftonEngine/ThirdParty/sol2`까지 (`3.5.0`)

Upstream: <https://github.com/ThePhD/sol2>

## RmlUi

MIT 라이선스. `W11`–`W14`의 `KraftonEngine/ThirdParty/RmlUi/Include` 헤더만 포함합니다. Debug/Release DLL, LIB, EXP 사본은 제외했습니다. 포함된 컨테이너 구현의 별도 MIT 고지도 보존했습니다.

Upstream: <https://github.com/mikke89/RmlUi>

## imgui-node-editor

MIT 라이선스, 버전 `0.9.4`.

- `W13/KraftonEngine/ThirdParty/imgui-node-editor`
- `W14/KraftonEngine/ThirdParty/imgui-node-editor`

Upstream: <https://github.com/thedmd/imgui-node-editor>

## NVIDIA PhysX

BSD-3-Clause 라이선스, 버전 `4.1.2`.

- `W13/KraftonEngine/ThirdParty/PhysX`: 소스, 헤더, 빌드 설정과 `LICENSE.md`
- `W14/KraftonEngine/ThirdParty/PhysX`: 헤더와 `LICENSE.md`

W13 사본에는 Visual Studio 2022 지원을 위한 빌드 스크립트 수정과 `vc17win64` 프리셋이 포함되어 있습니다. 함께 포함된 Targa 코드와 CMake 모듈의 개별 파일 내 라이선스 고지도 보존했습니다. 사전 빌드 DLL/LIB, `vswhere.exe`, 문서 이미지와 샘플 에셋은 포함하지 않았습니다.

Upstream: <https://github.com/NVIDIAGameWorks/PhysX/tree/4.1>

## NVIDIA NvCloth and PxShared

Nvidia Source Code License (1-Way Commercial). 배포 시 라이선스 전문을 함께 제공하고 기존 저작권·특허·상표·출처 고지를 수정하지 않아야 합니다.

- NvCloth `1.1.6`: `W13/KraftonEngine/ThirdParty/NvCloth`, `W14/KraftonEngine/ThirdParty/NvCloth`
- PxShared: `W14/KraftonEngine/ThirdParty/PxShared`

NvCloth의 `PsAllocator.h`에는 표준 C++ 헤더 호환성을 위한 한 줄 수정이 포함되어 있습니다. DLL/LIB 사본은 포함하지 않았습니다. W13 PhysX 저장소 안의 `pxshared/include`는 해당 PhysX BSD-3-Clause 고지 범위로 포함했습니다.

Upstream: <https://github.com/NVIDIAGameWorks/NvCloth>
