{
  description = "Meshtastic Beacon Module Development Environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    actions-nix.url = "github:nialov/actions.nix";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];

      imports = [
        # actions-nix bundles its own git-hooks module (provides the
        # perSystem pre-commit options); importing git-hooks-nix here as
        # well would double-declare them.
        inputs.actions-nix.flakeModules.default
      ];

      flake = {
        actions-nix = {
          pre-commit.enable = true;

          workflows = {
            ".github/workflows/build.yaml" = {
              name = "Compile Firmware";
              on = {
                push.branches = [ "main" ];
                pull_request.branches = [ "main" ];
                workflow_dispatch = {};
              };
              jobs.build = {
                runs-on = "ubuntu-latest";
                steps = [
                  { name = "Checkout repository"; uses = "actions/checkout@v4"; }
                  { name = "Install Nix"; uses = "cachix/install-nix-action@v31"; }
                  {
                    name = "Build WisMesh Tag Binary";
                    run = "nix develop --command ./build.sh rak_wismeshtag";
                  }
                  {
                    name = "Upload Firmware Artifact";
                    uses = "actions/upload-artifact@v4";
                    "with" = {
                      name = "rak-wismeshtag-firmware";
                      path = "firmware_build/.pio/build/rak_wismeshtag/*.uf2";
                    };
                  }
                ];
              };
            };
          };
        };
      };

      perSystem = { config, pkgs, ... }: {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [ platformio git python3 ];

          shellHook = ''
            ${config.pre-commit.installationScript}
            echo "Beacon Module Dev Environment Loaded."
            echo "GitHub Actions YAMLs will auto-generate on your next 'git commit'."
          '';
        };
      };
    };
}
