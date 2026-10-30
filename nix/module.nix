{ config, lib, pkgs, ... }:

let
  cfg = config.programs.chaos-hdr;
  busctl = "${pkgs.systemd}/bin/busctl --user call org.kde.KWin";
in
{
  options.programs.chaos-hdr = {
    enable = lib.mkEnableOption "the Chaos HDR KWin effect and its Alt+Z controls";
    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.callPackage ./package.nix { };
      defaultText = lib.literalExpression "pkgs.callPackage ./package.nix { }";
      description = "Chaos HDR package. It must be built against the KWin the system runs.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];

    systemd.user.services.chaos-hdr-effect = {
      description = "Load the Chaos HDR KWin effect";
      wantedBy = [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];
      after = [ "graphical-session.target" "plasma-kwin_wayland.service" ];
      serviceConfig = {
        Type = "oneshot";
        RemainAfterExit = true;
        ExecStart = "${busctl} /Effects org.kde.kwin.Effects loadEffect s truehdr_desktop";
        ExecStop = pkgs.writeShellScript "chaos-hdr-unload" ''
          for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
            reply=$(${busctl} /TrueHDR org.truehdr.Desktop prepareUnload) || break
            [ "$reply" = "b true" ] && break
            ${pkgs.coreutils}/bin/sleep 0.05
          done
          ${busctl} /Effects org.kde.kwin.Effects unloadEffect s truehdr_desktop
        '';
      };
    };

    systemd.user.services.chaos-hdr-controls = {
      description = "Chaos HDR Alt+Z controls";
      wantedBy = [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];
      after = [ "graphical-session.target" ];
      environment.QT_QPA_PLATFORM = "wayland";
      serviceConfig = {
        Type = "simple";
        ExecStartPre = pkgs.writeShellScript "chaos-hdr-prepare" ''
          dir="''${XDG_DATA_HOME:-$HOME/.local/share}/chaos-hdr"
          ${pkgs.coreutils}/bin/mkdir -p -m 700 "$dir"
          [ -e "$dir/profile.json" ] || ${pkgs.coreutils}/bin/install -m 600 ${cfg.package}/share/chaos-hdr/profile.example.json "$dir/profile.json"
          ${pkgs.coreutils}/bin/install -m 644 ${cfg.package}/share/chaos-hdr/release.json "$dir/release.json"
        '';
        ExecStart = "${cfg.package}/bin/truehdr-panel %h/.local/share/chaos-hdr/profile.json --overlay-service";
        Restart = "on-failure";
        RestartSec = 2;
        TimeoutStopSec = 5;
      };
    };
  };
}
