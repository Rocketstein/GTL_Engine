"""
Generate Visual Studio project files for the current single-project layout.

Output:
- AFOEngine.sln                (root)
- AFOEngine.vcxproj           (root)
- AFOEngine.vcxproj.filters   (root)

Scans:
- Engine/Source/**
- Engine/Shaders/**

Usage:
    py Scripts\GenerateProjectFiles.py
"""

from __future__ import annotations

import hashlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = ROOT / "Engine"

SOLUTION_NAME = "AFOEngine.sln"
PROJECT_NAME = "AFOEngine"
PROJECT_GUID = "{BD4280E9-79EA-4F64-9443-AF300DDA6733}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
WINDOWS_TARGET_PLATFORM_VERSION = "10.0"
NS = "http://schemas.microsoft.com/developer/msbuild/2003"

CONFIGURATIONS = [
    ("Debug", "Win32"),
    ("Release", "Win32"),
    ("Debug", "x64"),
    ("Release", "x64"),
]

SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".config", ".natstepfilter", ".txt", ".md", ".hlsl"}

@dataclass(frozen=True)
class ProjectSpec:
    name: str
    guid: str
    root_namespace: str
    project_root: Path
    scan_roots: tuple[str, ...]
    extra_items: dict[str, tuple[str, ...]] = field(default_factory=dict)

PROJECT = ProjectSpec(
    name=PROJECT_NAME,
    guid=PROJECT_GUID,
    root_namespace=PROJECT_NAME,
    project_root=ENGINE_ROOT,
    scan_roots=("Source", "Shaders"),
)

def to_windows_path(path: Path) -> str:
    return str(path).replace("/", "\\")

def classify_file(path: Path) -> str | None:
    ext = path.suffix.lower()
    if ext in SOURCE_EXTS:
        return "ClCompile"
    if ext in HEADER_EXTS:
        return "ClInclude"
    if ext in NATVIS_EXTS:
        return "Natvis"
    if ext in NONE_EXTS:
        return "None"
    return None

def sorted_unique(values: set[str]) -> list[str]:
    return sorted(values, key=lambda v: v.lower())

def scan_project_files(spec: ProjectSpec) -> dict[str, list[str]]:
    items = {
        "ClCompile": set(),
        "ClInclude": set(),
        "Natvis": set(),
        "None": set(),
    }

    for scan_root in spec.scan_roots:
        absolute_root = spec.project_root / scan_root
        if not absolute_root.is_dir():
            continue

        for file_path in absolute_root.rglob("*"):
            if not file_path.is_file():
                continue

            item_type = classify_file(file_path)
            if item_type is None:
                continue

            # vcxproj is at ROOT, so store paths relative to ROOT.
            items[item_type].add(to_windows_path(file_path.relative_to(ROOT)))

    for item_type, relative_paths in spec.extra_items.items():
        for relative_path in relative_paths:
            candidate = spec.project_root / relative_path
            if candidate.is_file():
                items[item_type].add(to_windows_path(candidate.relative_to(ROOT)))

    return {item_type: sorted_unique(paths) for item_type, paths in items.items()}

def indent_xml(element: ET.Element, level: int = 0) -> None:
    indent = "\n" + "  " * level
    if len(element):
        if not element.text or not element.text.strip():
            element.text = indent + "  "
        for child in element:
            indent_xml(child, level + 1)
            if not child.tail or not child.tail.strip():
                child.tail = indent + "  "
        if not element[-1].tail or not element[-1].tail.strip():
            element[-1].tail = indent
    elif level and (not element.tail or not element.tail.strip()):
        element.tail = indent

def write_xml(root_element: ET.Element, destination: Path, bom: bool = False) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    indent_xml(root_element)
    xml_body = ET.tostring(root_element, encoding="utf-8")
    xml_bytes = b'<?xml version="1.0" encoding="utf-8"?>\n' + xml_body
    if bom:
        xml_bytes = b"\xef\xbb\xbf" + xml_bytes
    destination.write_bytes(xml_bytes)

def add_empty_import_group(root: ET.Element, **attributes: str) -> ET.Element:
    element = ET.SubElement(root, "ImportGroup", **attributes)
    element.text = "\n  "
    return element

def add_project_configurations(root: ET.Element) -> None:
    item_group = ET.SubElement(root, "ItemGroup", Label="ProjectConfigurations")
    for configuration, platform in CONFIGURATIONS:
        project_configuration = ET.SubElement(
            item_group, "ProjectConfiguration", Include=f"{configuration}|{platform}"
        )
        ET.SubElement(project_configuration, "Configuration").text = configuration
        ET.SubElement(project_configuration, "Platform").text = platform

def add_configuration_groups(root: ET.Element) -> None:
    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        property_group = ET.SubElement(root, "PropertyGroup", Condition=condition, Label="Configuration")
        ET.SubElement(property_group, "ConfigurationType").text = "Application"
        ET.SubElement(property_group, "UseDebugLibraries").text = "true" if configuration == "Debug" else "false"
        ET.SubElement(property_group, "PlatformToolset").text = "v143"
        if configuration == "Release":
            ET.SubElement(property_group, "WholeProgramOptimization").text = "true"
        ET.SubElement(property_group, "CharacterSet").text = "Unicode"

def add_property_sheet_imports(root: ET.Element) -> None:
    add_empty_import_group(root, Label="ExtensionSettings")
    add_empty_import_group(root, Label="Shared")
    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        import_group = ET.SubElement(root, "ImportGroup", Label="PropertySheets", Condition=condition)
        ET.SubElement(
            import_group,
            "Import",
            Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
            Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
            Label="LocalAppDataPlatform",
        )

def add_item_definition_groups(root: ET.Element) -> None:
    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        item_definition_group = ET.SubElement(root, "ItemDefinitionGroup", Condition=condition)

        cl_compile = ET.SubElement(item_definition_group, "ClCompile")
        ET.SubElement(cl_compile, "WarningLevel").text = "Level3"
        ET.SubElement(cl_compile, "SDLCheck").text = "true"
        ET.SubElement(cl_compile, "ConformanceMode").text = "true"
        ET.SubElement(cl_compile, "LanguageStandard").text = "stdcpp20"
        ET.SubElement(cl_compile, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
        ET.SubElement(cl_compile, "AdditionalIncludeDirectories").text = (
            "$(ProjectDir);"
            "$(ProjectDir)Engine;"
            "$(ProjectDir)Engine\\Source;"
            "$(ProjectDir)Engine\\ThirdParty;"
            "$(ProjectDir)Engine\\Shaders;"
            "%(AdditionalIncludeDirectories)"
        )

        if platform == "Win32":
            preprocessor = (
                "WIN32;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)"
                if configuration == "Debug"
                else "WIN32;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)"
            )
        else:
            preprocessor = (
                "_DEBUG;_CONSOLE;%(PreprocessorDefinitions)"
                if configuration == "Debug"
                else "NDEBUG;_CONSOLE;%(PreprocessorDefinitions)"
            )

        ET.SubElement(cl_compile, "PreprocessorDefinitions").text = preprocessor

        if configuration == "Release":
            ET.SubElement(cl_compile, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl_compile, "IntrinsicFunctions").text = "true"

        link = ET.SubElement(item_definition_group, "Link")
        ET.SubElement(link, "SubSystem").text = "Windows"
        if configuration == "Release":
            ET.SubElement(link, "EnableCOMDATFolding").text = "true"
            ET.SubElement(link, "OptimizeReferences").text = "true"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"
        ET.SubElement(link, "AdditionalDependencies").text = (
            "d3d11.lib;dxgi.lib;d3dcompiler.lib;%(AdditionalDependencies)"
        )

def generate_project(spec: ProjectSpec, files: dict[str, list[str]]) -> None:
    root = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

    add_project_configurations(root)

    globals_group = ET.SubElement(root, "PropertyGroup", Label="Globals")
    ET.SubElement(globals_group, "VCProjectVersion").text = "17.0"
    ET.SubElement(globals_group, "Keyword").text = "Win32Proj"
    ET.SubElement(globals_group, "ProjectGuid").text = spec.guid.lower()
    ET.SubElement(globals_group, "RootNamespace").text = spec.root_namespace
    ET.SubElement(globals_group, "WindowsTargetPlatformVersion").text = WINDOWS_TARGET_PLATFORM_VERSION

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    add_configuration_groups(root)

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")

    add_property_sheet_imports(root)

    ET.SubElement(root, "PropertyGroup", Label="UserMacros")

    add_item_definition_groups(root)

    compile_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClCompile"]:
        ET.SubElement(compile_group, "ClCompile", Include=path)

    include_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClInclude"]:
        ET.SubElement(include_group, "ClInclude", Include=path)

    if files["None"]:
        none_group = ET.SubElement(root, "ItemGroup")
        for path in files["None"]:
            ET.SubElement(none_group, "None", Include=path)

    if files["Natvis"]:
        natvis_group = ET.SubElement(root, "ItemGroup")
        for path in files["Natvis"]:
            ET.SubElement(natvis_group, "Natvis", Include=path)

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    add_empty_import_group(root, Label="ExtensionTargets")

    write_xml(root, ROOT / f"{spec.name}.vcxproj")

def generate_solution(spec: ProjectSpec) -> None:
    lines = [
        "Microsoft Visual Studio Solution File, Format Version 12.00",
        "# Visual Studio Version 17",
        "VisualStudioVersion = 17.12.35707.178 d17.12",
        "MinimumVisualStudioVersion = 10.0.40219.1",
        f'Project("{VS_PROJECT_TYPE}") = "{spec.name}", "{spec.name}.vcxproj", "{spec.guid}"',
        "EndProject",
        "Global",
        "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
        "\t\tDebug|x64 = Debug|x64",
        "\t\tDebug|x86 = Debug|x86",
        "\t\tRelease|x64 = Release|x64",
        "\t\tRelease|x86 = Release|x86",
        "\tEndGlobalSection",
        "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution",
        f"\t\t{spec.guid}.Debug|x64.ActiveCfg = Debug|x64",
        f"\t\t{spec.guid}.Debug|x64.Build.0 = Debug|x64",
        f"\t\t{spec.guid}.Debug|x86.ActiveCfg = Debug|Win32",
        f"\t\t{spec.guid}.Debug|x86.Build.0 = Debug|Win32",
        f"\t\t{spec.guid}.Release|x64.ActiveCfg = Release|x64",
        f"\t\t{spec.guid}.Release|x64.Build.0 = Release|x64",
        f"\t\t{spec.guid}.Release|x86.ActiveCfg = Release|Win32",
        f"\t\t{spec.guid}.Release|x86.Build.0 = Release|Win32",
        "\tEndGlobalSection",
        "\tGlobalSection(SolutionProperties) = preSolution",
        "\t\tHideSolutionNode = FALSE",
        "\tEndGlobalSection",
        "EndGlobal",
    ]
    (ROOT / SOLUTION_NAME).write_text("\n".join(lines), encoding="utf-8-sig", newline="\n")

def main() -> None:
    ET.register_namespace("", NS)

    print("ROOT =", ROOT)
    print("ENGINE_ROOT =", ENGINE_ROOT)

    files = scan_project_files(PROJECT)
    total = sum(len(entries) for entries in files.values())
    print(f"Scanned {total} files")

    generate_project(PROJECT, files)
    generate_solution(PROJECT)

    print("Generated:")
    print(" ", ROOT / SOLUTION_NAME)
    print(" ", ROOT / f"{PROJECT.name}.vcxproj")

if __name__ == "__main__":
    main()
