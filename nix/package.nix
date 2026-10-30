{ lib
, stdenv
, fetchurl
, unzip
, cmake
, pkg-config
, glslang
, addDriverRunpath
, kdePackages
, vulkan-headers
, vulkan-loader
, libepoxy
, libdrm
, wayland
, fontconfig
, libxkbcommon
}:

let
  dlssRev = "374959484e79a640feaba44c93ac8cfb0a03f5b5";
  dlssFile = path: sha256: fetchurl {
    url = "https://raw.githubusercontent.com/NVIDIA/DLSS/${dlssRev}/${path}";
    inherit sha256;
  };
  ngxFiles = {
    "LICENSE.txt" = dlssFile "LICENSE.txt" "d4216e39ebef5f9b50a6712ebb37beeb5379862a67733a9999c651f21592aaf0";
    "nvsdk_ngx_vk.h" = dlssFile "include/nvsdk_ngx_vk.h" "2d364ce7132881eb669e9498fd570d74cba563b1fbebc82c235f8e1ad2dd8b6d";
    "nvsdk_ngx_defs.h" = dlssFile "include/nvsdk_ngx_defs.h" "ea23f33497cd274860d1c25a97644fce807dcb0037c594547203343103fad03e";
    "nvsdk_ngx_params.h" = dlssFile "include/nvsdk_ngx_params.h" "943bc8cc5cdae03b6303016fbad3183636f2335ae27a2d18776798c3b4efabbc";
    "nvsdk_ngx_defs_vk.h" = dlssFile "include/nvsdk_ngx_defs_vk.h" "0a24d0861ace7d6b9362a67b7f08bea1b33ea123c5ce730b19133de8a891d031";
    "nvsdk_ngx_helpers_vk.h" = dlssFile "include/nvsdk_ngx_helpers_vk.h" "c192bff72138f12f770db48e12c1d8f712dfc81ea0e8e055ebb27d8dd1b31623";
    "libnvsdk_ngx.a" = dlssFile "lib/Linux_x86_64/libnvsdk_ngx.a" "3321c4ca9adf71f345550d10bf2148ed38003151b03ec5f81c99d00949ab8202";
  };

  truehdrRuntime = stdenv.mkDerivation {
    pname = "nvidia-ngx-truehdr";
    version = "1.2.3";
    src = fetchurl {
      url = "https://pypi.nvidia.com/nvidia-vfx/nvidia_vfx-0.2.0.0-cp312-abi3-manylinux_2_27_x86_64.manylinux_2_28_x86_64.whl";
      sha256 = "0252866a7b798a6f92c8543443d88f716e8c5bbc6a7ddd77a2754728dada9fa8";
    };
    nativeBuildInputs = [ unzip ];
    unpackPhase = "unzip -q $src 'nvvfx/libs/libnvidia-ngx-truehdr.so.1.2.3' 'nvidia_vfx-0.2.0.0.dist-info/*'";
    installPhase = ''
      install -Dm644 nvvfx/libs/libnvidia-ngx-truehdr.so.1.2.3 $out/nvvfx/libs/libnvidia-ngx-truehdr.so.1.2.3
      cp -r nvidia_vfx-0.2.0.0.dist-info $out/
    '';
    dontFixup = true;
    meta.license = lib.licenses.unfree;
  };
in
stdenv.mkDerivation {
  pname = "chaos-hdr";
  version = lib.removePrefix "v" (lib.importJSON ../release.json).tag;

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../LICENSE
      ../THIRD_PARTY_NOTICES.rst
      ../assets
      ../native
      ../profile.example.json
      ../release.json
      ../vendor
    ];
  };

  nativeBuildInputs = [
    cmake
    pkg-config
    glslang
    addDriverRunpath
    kdePackages.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.qtbase
    kdePackages.qtdeclarative
    kdePackages.kwin
    kdePackages.kcoreaddons
    kdePackages.kconfig
    kdePackages.kwindowsystem
    kdePackages.kglobalaccel
    kdePackages.kguiaddons
    vulkan-headers
    vulkan-loader
    libepoxy
    libdrm
    wayland
    fontconfig
    libxkbcommon
  ];

  dontUseCmakeConfigure = true;

  configurePhase = ''
    runHook preConfigure

    mkdir -p $NIX_BUILD_TOP/ngx
    ${lib.concatStringsSep "\n" (lib.mapAttrsToList (name: file: "cp ${file} $NIX_BUILD_TOP/ngx/${name}") ngxFiles)}

    cmake -S native/desktop -B build-desktop -DCMAKE_BUILD_TYPE=Release \
      -DTRUEHDR_SYSTEM_PACKAGE=ON \
      -DTRUEHDR_ROOT=$out/share/chaos-hdr \
      -DTRUEHDR_ASSET_ROOT=$out/share/chaos-hdr \
      -DTRUEHDR_RUNTIME_SITE=${truehdrRuntime} \
      -DTRUEHDR_NGX=$NIX_BUILD_TOP/ngx \
      -DTRUEHDR_KWIN_INCLUDE=${kdePackages.kwin.dev}/include/kwin \
      -DTRUEHDR_SHADER_PATH=$out/share/chaos-hdr/desktop_input.spv \
      -DTRUEHDR_PANEL=$out/bin/truehdr-panel \
      -DTRUEHDR_EFFECT_ID=truehdr_desktop
    cmake -S . -B build-controls -DCMAKE_BUILD_TYPE=Release -DTRUEHDR_SYSTEM_PACKAGE=ON

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build build-desktop --target truehdr_desktop truehdr-desktop-gpu-test --parallel $NIX_BUILD_CORES
    cmake --build build-controls --target truehdr-panel --parallel $NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    install -Dm755 build-desktop/kwin/effects/plugins/truehdr_desktop.so \
      $out/lib/qt-6/plugins/kwin/effects/plugins/truehdr_desktop.so
    install -Dm644 build-desktop/desktop_input.spv $out/share/chaos-hdr/desktop_input.spv
    install -Dm755 build-controls/truehdr-panel $out/bin/truehdr-panel
    install -Dm755 build-desktop/truehdr-desktop-gpu-test $out/libexec/chaos-hdr/truehdr-desktop-gpu-test

    mkdir -p $out/share/chaos-hdr/vendor
    cp -r assets $out/share/chaos-hdr/assets
    cp -r vendor/fonts $out/share/chaos-hdr/vendor/fonts
    cp release.json profile.example.json $out/share/chaos-hdr/

    install -Dm644 LICENSE $out/share/licenses/chaos-hdr/LICENSE
    install -Dm644 $NIX_BUILD_TOP/ngx/LICENSE.txt $out/share/licenses/chaos-hdr/NVIDIA-DLSS-LICENSE.txt
    install -Dm644 THIRD_PARTY_NOTICES.rst $out/share/licenses/chaos-hdr/THIRD_PARTY_NOTICES.rst

    runHook postInstall
  '';

  postFixup = ''
    for f in $out/lib/qt-6/plugins/kwin/effects/plugins/truehdr_desktop.so \
             $out/libexec/chaos-hdr/.truehdr-desktop-gpu-test-wrapped; do
      patchelf --add-needed libnvidia-ml.so.1 "$f"
      addDriverRunpath "$f"
    done
  '';

  passthru = { inherit truehdrRuntime; };

  meta = {
    description = "KWin effect that turns SDR windows into HDR using NVIDIA TrueHDR or a shader engine";
    homepage = "https://github.com/xkeyC/chaos-hdr";
    license = with lib.licenses; [ gpl2Only unfree ];
    platforms = [ "x86_64-linux" ];
    mainProgram = "truehdr-panel";
  };
}
