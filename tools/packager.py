

import os
import sys
import json
import shutil
import subprocess
import argparse
import platform
from pathlib import Path

# =============================================================================
# CONFIGURAÇÃO E UTILITÁRIOS
# =============================================================================

def load_json(path):
    if path.exists():
        with open(path, 'r') as f:
            try:
                return json.load(f)
            except json.JSONDecodeError as e:
                print(f"[ERROR] Failed to parse JSON {path}: {e}")
                return {}
    return {}

def merge_dicts(base, overlay):
    """Merge recursivo simples para dicionários."""
    for k, v in overlay.items():
        if isinstance(v, dict) and k in base and isinstance(base[k], dict):
            merge_dicts(base[k], v)
        else:
            base[k] = v
    return base

def get_toolchain_config(repo_root):
    config = load_json(repo_root / "config.json")
    return config.get("Configuration", {}).get("Toolchain", {})

def find_tool(name, search_paths):
    for path in search_paths:
        if not path: continue
        tool = path / name
        if platform.system() == "Windows":
            if (tool.with_suffix(".exe")).exists(): return tool.with_suffix(".exe")
            if (tool.with_suffix(".bat")).exists(): return tool.with_suffix(".bat")
        elif tool.exists():
            return tool
    return None

def run_cmd(cmd, cwd=None, env=None):
    print(f"[CMD] {' '.join(str(c) for c in cmd)}")
    try:
        subprocess.check_call([str(c) for c in cmd], cwd=cwd, env=env)
        return True
    except subprocess.CalledProcessError:
        print(f"[ERROR] Command failed: {cmd[0]}")
        return False

# =============================================================================
# ANDROID PACKAGER
# =============================================================================

class AndroidPackager:
    def __init__(self, repo_root, project_dir, release_config=None, release_name=None):
        self.repo_root = repo_root
        self.project_dir = project_dir
        
        # Carregar main.mk e mergear com release.json se existir
        self.project_spec = load_json(project_dir / "main.mk")
        if release_config:
            print(f"[INFO] Applying release configuration...")
            self.project_spec = merge_dicts(self.project_spec, release_config)
            
        self.name = self.project_spec.get("Name", project_dir.name)
        self.android_spec = self.project_spec.get("Android", {})
        
        # Configurar Toolchain
        tc_config = get_toolchain_config(repo_root)
        self.sdk_root = Path(os.environ.get("ANDROID_SDK_ROOT") or tc_config.get("AndroidSdk") or "")
        self.build_tools_version = tc_config.get("BuildTools")
        
        # Localizar ferramentas
        if self.build_tools_version:
            build_tools = self.sdk_root / "build-tools" / self.build_tools_version
        else:
            # Pega a última versão se não especificada
            build_tools = sorted(list((self.sdk_root / "build-tools").glob("*")))[-1]
            
        self.aapt = find_tool("aapt", [build_tools])
        self.apksigner = find_tool("apksigner", [build_tools])
        self.zipalign = find_tool("zipalign", [build_tools])
        
        # Platform JAR
        platform_ver = tc_config.get("Platform", "android-31")
        self.platform_jar = self.sdk_root / "platforms" / platform_ver / "android.jar"
        if not self.platform_jar.exists():
             # Fallback para a última disponível
            platforms = sorted(list((self.sdk_root / "platforms").glob("android-*")), 
                           key=lambda p: int(p.name.split('-')[-1]))
            if platforms:
                self.platform_jar = platforms[-1] / "android.jar"

        # Output dirs
        out_folder = release_name if release_name else "Package"
        self.out_dir = project_dir / "Android" / out_folder
        self.bin_dir = project_dir / "Android"  # Onde estão os .so compilados
        self.res_dir = self.out_dir / "res"
        self.assets_dir = self.out_dir / "assets"
        self.lib_dir = self.out_dir / "lib"
        self.tmp_dir = self.out_dir / "tmp"

    def prepare_layout(self):
        if self.out_dir.exists():
            shutil.rmtree(self.out_dir)
            
        for d in [self.res_dir, self.assets_dir, self.lib_dir, self.tmp_dir]:
            d.mkdir(parents=True, exist_ok=True)
            
        # Copiar ícones
        icon_path = self.android_spec.get("ICON")
        if icon_path:
            src = self.project_dir / icon_path
            if src.exists():
                mipmap = self.res_dir / "mipmap-hdpi"
                mipmap.mkdir(exist_ok=True)
                shutil.copy(src, mipmap / "ic_launcher.png")
        
        # Copiar Assets e Scripts
        content_root = self.project_dir
        if "CONTENT_ROOT" in self.project_spec:
            content_root = self.project_dir / self.project_spec["CONTENT_ROOT"]
        elif "CONTENT_ROOT" in self.android_spec:
            content_root = self.project_dir / self.android_spec["CONTENT_ROOT"]
            
        folders_to_copy = ["scripts", "assets", "resources", "data", "media"]
        for folder in folders_to_copy:
            src = content_root / folder
            if src.exists():
                print(f"[COPY] {folder} -> assets/{folder}")
                shutil.copytree(src, self.assets_dir / folder, dirs_exist_ok=True)

        # Copiar Bibliotecas Nativas (.so)
        # Assume que já foram compiladas e estão em project/Android/<abi>/lib<name>.so
        abis = ["armeabi-v7a", "arm64-v8a", "x86", "x86_64"]
        found_libs = False
        
        for abi in abis:
            # Tenta encontrar no output padrão do builder C++
            # Pode ser project/Android/<abi>/libName.so ou project/bin/Android/<abi>/...
            lib_name = f"lib{self.name}.so"
            
            candidates = [
                self.project_dir / "Android" / abi / lib_name,
                self.project_dir / "bin" / "Android" / abi / lib_name,
                self.project_dir / "libs" / abi / lib_name
            ]
            
            src_lib = None
            for c in candidates:
                if c.exists():
                    src_lib = c
                    break
            
            if src_lib:
                dst_abi = self.lib_dir / abi
                dst_abi.mkdir(exist_ok=True)
                shutil.copy(src_lib, dst_abi / lib_name)
                print(f"[LIB] Found {abi} library: {src_lib}")
                found_libs = True
                
                # Copiar bibliotecas dependentes (se houver, ex: libc++_shared.so)
                # Aqui assumimos que estão na mesma pasta da lib principal
                for dep in src_lib.parent.glob("*.so"):
                    if dep.name != lib_name:
                        shutil.copy(dep, dst_abi / dep.name)

        if not found_libs:
            print(f"[WARNING] No native libraries (.so) found for {self.name}. APK might crash.")

    def generate_manifest(self):
        package = self.android_spec.get("PACKAGE", "com.example.game")
        activity = self.android_spec.get("ACTIVITY", "android.app.NativeActivity")
        label = self.android_spec.get("LABEL", self.name)
        lib_name = self.name
        
        # Versões do SDK
        min_sdk = self.android_spec.get("MANIFEST_VARS", {}).get("MIN_SDK", "21")
        target_sdk = self.android_spec.get("MANIFEST_VARS", {}).get("TARGET_SDK", "30")

        # Verifica se o ícone existe para adicionar ao manifesto
        icon_attr = ""
        if (self.res_dir / "mipmap-hdpi" / "ic_launcher.png").exists():
            icon_attr = 'android:icon="@mipmap/ic_launcher"'

        manifest = f"""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="{package}"
          android:versionCode="1"
          android:versionName="1.0">
    <uses-sdk android:minSdkVersion="{min_sdk}" android:targetSdkVersion="{target_sdk}" />
    <uses-feature android:glEsVersion="0x00020000" android:required="true" />
    <application android:label="{label}" {icon_attr} android:hasCode="false">
        <activity android:name="{activity}"
                  android:label="{label}"
                  android:configChanges="orientation|keyboardHidden|screenSize"
                  android:screenOrientation="landscape"
                  android:exported="true">
            <meta-data android:name="android.app.lib_name" android:value="{lib_name}" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>"""
        
        manifest_path = self.out_dir / "AndroidManifest.xml"
        with open(manifest_path, "w") as f:
            f.write(manifest)
        return manifest_path

    def package(self):
        print(f"Packaging Android APK for {self.name}...")
        self.prepare_layout()
        manifest = self.generate_manifest()

        unsigned_apk = self.tmp_dir / f"{self.name}.unsigned.apk"
        aligned_apk = self.tmp_dir / f"{self.name}.aligned.apk"
        final_apk = self.out_dir / f"{self.name}.apk"

        # 1. AAPT Package (Resources + Assets + Manifest)
        # -A assets: adiciona a pasta assets ao APK
        cmd = [
            self.aapt, "package", "-f",
            "-M", manifest,
            "-S", self.res_dir,
            "-A", self.assets_dir,
            "-I", self.platform_jar,
            "-F", unsigned_apk
        ]
        if not run_cmd(cmd): return

        # 2. Adicionar bibliotecas nativas manualmente (aapt as vezes é chato com libs)
        # O aapt add espera caminhos relativos.
        cwd = os.getcwd()
        os.chdir(self.out_dir)
        try:
            # Adiciona tudo dentro de lib/
            for lib_file in self.lib_dir.rglob("*.so"):
                rel_path = lib_file.relative_to(self.out_dir)
                run_cmd([self.aapt, "add", unsigned_apk.relative_to(self.out_dir), str(rel_path)])
        finally:
            os.chdir(cwd)

        # 3. Zipalign (Otimização importante para Android)
        if self.zipalign:
            run_cmd([self.zipalign, "-f", "-p", "4", unsigned_apk, aligned_apk])
            target_apk_for_signing = aligned_apk
        else:
            print("[WARN] zipalign not found, skipping.")
            target_apk_for_signing = unsigned_apk

        # 4. Assinar APK (Debug Key)
        keystore = self.out_dir / "debug.keystore"
        if not keystore.exists():
            # Tenta usar keytool do Java
            keytool = "keytool"
            # Se tiver JAVA_HOME, usa o keytool de lá
            java_home = os.environ.get("JAVA_HOME")
            if java_home:
                keytool = Path(java_home) / "bin" / "keytool"
            
            run_cmd([keytool, "-genkeypair", "-keystore", keystore,
                     "-storepass", "android", "-alias", "androiddebugkey",
                     "-keypass", "android", "-dname", "CN=Android Debug,O=Android,C=US",
                     "-validity", "10000"])

        run_cmd([self.apksigner, "sign", "--ks", keystore,
                 "--ks-pass", "pass:android",
                 "--out", final_apk, target_apk_for_signing])
        
        print(f"[SUCCESS] APK created: {final_apk}")

# =============================================================================
# WEB PACKAGER
# =============================================================================

class WebPackager:
    def __init__(self, repo_root, project_dir, release_config=None, release_name=None):
        self.repo_root = repo_root
        self.project_dir = project_dir
        self.project_spec = load_json(project_dir / "main.mk")
        if release_config:
            self.project_spec = merge_dicts(self.project_spec, release_config)
            
        self.name = self.project_spec.get("Name", project_dir.name)
        out_folder = release_name if release_name else "Deploy"
        self.out_dir = project_dir / "Web" / out_folder
        self.src_web_dir = project_dir / "Web" # Onde o build C++ coloca os arquivos

    def package(self):
        print(f"Packaging Web build for {self.name}...")
        
        if self.out_dir.exists():
            shutil.rmtree(self.out_dir)
        self.out_dir.mkdir(parents=True, exist_ok=True)

        # 1. Copiar binários Web (HTML, JS, WASM, DATA)
        # Procura por arquivos gerados pelo build C++
        extensions = [".html", ".js", ".wasm"] # .data será gerado novamente
        found_binary = False
        
        for ext in extensions:
            # Tenta encontrar com o nome do projeto ou index
            candidates = [
                self.src_web_dir / f"{self.name}{ext}",
                self.src_web_dir / f"index{ext}",
                self.src_web_dir / f"main{ext}"
            ]
            for c in candidates:
                if c.exists():
                    shutil.copy(c, self.out_dir / c.name)
                    print(f"[COPY] {c.name}")
                    found_binary = True

        if not found_binary:
            print(f"[WARNING] No Web build artifacts found in {self.src_web_dir}. Did you build C++ first?")

        # 2. Empacotar Assets (file_packager)
        self.package_assets()

        print(f"[SUCCESS] Web deploy created: {self.out_dir}")

    def package_assets(self):
        tc_config = get_toolchain_config(self.repo_root)
        emsdk_path = os.environ.get("EMSDK") or tc_config.get("Emsdk")
        
        if not emsdk_path:
            print("[ERROR] EMSDK path not found in config.json or env. Cannot package assets.")
            return

        emsdk = Path(emsdk_path)
        # Tenta localizar file_packager.py
        file_packager = emsdk / "upstream" / "emscripten" / "tools" / "file_packager.py"
        if not file_packager.exists():
             file_packager = emsdk / "emscripten" / "tools" / "file_packager.py"
        
        if not file_packager.exists():
            print(f"[ERROR] file_packager.py not found in {emsdk}")
            return

        data_file = self.out_dir / f"{self.name}.data"
        js_file = self.out_dir / f"{self.name}.data.js"
        
        content_root = self.project_dir
        if "CONTENT_ROOT" in self.project_spec:
            content_root = self.project_dir / self.project_spec["CONTENT_ROOT"]
        elif "Web" in self.project_spec and "CONTENT_ROOT" in self.project_spec["Web"]:
            content_root = self.project_dir / self.project_spec["Web"]["CONTENT_ROOT"]

        cmd = [sys.executable, str(file_packager), str(data_file)]
        
        folders_to_pack = ["scripts", "assets", "resources", "data", "media"]
        has_assets = False
        
        for folder in folders_to_pack:
            src = content_root / folder
            if src.exists():
                cmd.append(f"--preload")
                cmd.append(f"{src}@{folder}")
                has_assets = True
        
        if has_assets:
            cmd.append(f"--js-output={js_file}")
            cmd.append("--no-heap-copy")
            print(f"[PACK] Running file_packager...")
            if run_cmd(cmd):
                print(f"[PACK] Generated {data_file.name} and {js_file.name}")
                print(f"[INFO] NOTE: Ensure {js_file.name} is loaded in your HTML.")
        else:
            print("[INFO] No assets found to package.")

# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Crosside Packager (No-Compile)")
    parser.add_argument("project", help="Path to project folder")
    parser.add_argument("target", choices=["android", "web"], help="Target platform")
    parser.add_argument("--release", help="Path to release.json configuration file", default=None)
    args = parser.parse_args()

    repo_root = Path(__file__).parent.parent.resolve()
    project_path = Path(args.project).resolve()
    
    release_config = None
    release_name = None
    if args.release:
        release_path = Path(args.release)
        if not release_path.exists():
            # Tenta relativo ao projeto
            release_path = project_path / args.release
        
        if release_path.exists():
            print(f"[INFO] Loading release config: {release_path}")
            release_config = load_json(release_path)
            release_name = release_path.stem
        else:
            print(f"[ERROR] Release file not found: {args.release}")
            sys.exit(1)

    if args.target == "android":
        packager = AndroidPackager(repo_root, project_path, release_config, release_name)
        packager.package()
    elif args.target == "web":
        packager = WebPackager(repo_root, project_path, release_config, release_name)
        packager.package()
