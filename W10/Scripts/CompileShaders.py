import os
import subprocess
import glob

# 셰이더 소스 디렉터리 및 출력 디렉터리 설정
SHADER_SRC_DIR = "CrashEngine/Shaders"
SHADER_OBJ_DIR = "Saved/ShaderCache"

# DXC 또는 FXC 경로 (일반적으로 Windows SDK에 포함됨)
# 여기서는 간단하게 시스템 경로에 있다고 가정하거나 fxc를 사용
COMPILER = "fxc.exe"

def compile_shaders():
    if not os.path.exists(SHADER_OBJ_DIR):
        os.makedirs(SHADER_OBJ_DIR)

    # 모든 .hlsl 파일 검색
    hlsl_files = []
    for root, dirs, files in os.walk(SHADER_SRC_DIR):
        for file in files:
            if file.endswith(".hlsl"):
                hlsl_files.append(os.path.join(root, file))

    print(f"Found {len(hlsl_files)} shader files.")

    for shader_path in hlsl_files:
        # Vertex Shader, Pixel Shader 등을 구분하는 로직이 필요하지만 
        # 여기서는 파일명 접미사로 간단히 구분하는 예시 (VS_, PS_)
        filename = os.path.basename(shader_path)
        
        profiles = []
        if "VS" in filename or "Vertex" in filename:
            profiles = ["vs_5_0"]
        elif "PS" in filename or "Pixel" in filename:
            profiles = ["ps_5_0"]
        elif "CS" in filename or "Compute" in filename:
            profiles = ["cs_5_0"]
        
        if not profiles:
            # 프로파일을 유추할 수 없으면 건너뛰거나 기본값 사용
            continue

        for profile in profiles:
            output_name = filename.replace(".hlsl", "") + "_" + profile + ".cso"
            output_path = os.path.join(SHADER_OBJ_DIR, output_name)
            
            # 컴파일 명령 (최적화 옵션 포함)
            cmd = [
                COMPILER,
                "/T", profile,
                "/E", "main", # 진입점은 main으로 가정
                "/Fo", output_path,
                "/O3", # 최적화 레벨 3
                shader_path
            ]
            
            print(f"Compiling {filename} for {profile}...")
            try:
                subprocess.run(cmd, check=True, capture_output=True)
            except subprocess.CalledProcessError as e:
                print(f"Failed to compile {filename}: {e.stderr.decode()}")

if __name__ == "__main__":
    compile_shaders()
