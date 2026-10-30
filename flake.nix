{
  description = "KWin effect that turns SDR windows into HDR using NVIDIA TrueHDR or a shader engine";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
      chaos-hdr = pkgs.callPackage ./nix/package.nix { };
    in
    {
      packages.${system} = {
        inherit chaos-hdr;
        default = chaos-hdr;
      };

      overlays.default = final: _: {
        chaos-hdr = final.callPackage ./nix/package.nix { };
      };

      nixosModules.default = import ./nix/module.nix;

      checks.${system} = {
        inherit chaos-hdr;
        native = pkgs.stdenv.mkDerivation {
          name = "chaos-hdr-native-checks";
          src = pkgs.lib.fileset.toSource {
            root = ./.;
            fileset = pkgs.lib.fileset.unions [ ./CMakeLists.txt ./native ];
          };
          nativeBuildInputs = [ pkgs.cmake ];
          buildInputs = [ pkgs.kdePackages.qtbase ];
          dontWrapQtApps = true;
          cmakeFlags = [ "-DTRUEHDR_TESTS_ONLY=ON" ];
          buildFlags = [ "truehdr-profiles-test" "truehdr-overlay-motion-test" ];
          doCheck = true;
          checkPhase = ''
            ./truehdr-profiles-test "$TMPDIR/profiles"
            ./truehdr-overlay-motion-test
          '';
          installPhase = "touch $out";
        };
      };
    };
}
