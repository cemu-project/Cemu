{
  description = "CemuExtend Nix build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    # Cemu expects the complete Dear ImGui source tree, not only installed
    # headers.  Keep it as an explicit input so the flake also works when the
    # git submodules in a checkout have not been initialized yet.
    imgui-src = {
      url = "github:ocornut/imgui/v1.91.3";
      flake = false;
    };

    # Git flakes do not copy an initialized submodule worktree into the Nix
    # source automatically. Fetch the public repository at the same immutable
    # revision as the submodule so standalone/clean CemuExtend clones work.
    libcemuextend-src = {
      url = "git+https://github.com/PinkDiamondTeam/libcemuextend.git?rev=83729235e892ae93d7780eff2d46cfd55114dbe2";
      flake = false;
    };
  };

  outputs =
    inputs@{
      self,
      nixpkgs,
      imgui-src,
      libcemuextend-src,
      ...
    }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      lib = pkgs.lib;

      # Keep the Nix source small and reproducible.  Dependencies that are
      # supplied by Nixpkgs are deliberately omitted and linked in
      # preConfigure below where Cemu expects a vendored source tree.
      source = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./CMakeLists.txt
          ./bin/gameProfiles/default
          ./bin/resources
          ./cmake
          ./dependencies/DirectX_2010
          ./dependencies/gamemode
          ./dependencies/ih264d
          (lib.fileset.maybeMissing ./dependencies/libcemuextend)
          ./dist
          ./src
        ];
      };

      # glslang's CMake package refers to SPIRV-Tools-opt before it is
      # otherwise loaded.  This is the same initialization used by the
      # cemu_pinkd reference flake.
      devCmakeInit = pkgs.writeText "cemu-extend-nix-init.cmake" ''
        if(NOT TARGET SPIRV-Tools-opt)
          find_package(SPIRV-Tools-opt REQUIRED)
        endif()
      '';

      cemu = pkgs.stdenv.mkDerivation {
        pname = "cemu-extend";
        version = "2.0-${self.shortRev or "dirty"}";
        src = source;

        nativeBuildInputs = with pkgs; [
          addDriverRunpath
          cmake
          nasm
          ninja
          pkg-config
          wayland-scanner
          wrapGAppsHook3
          wxwidgets_3_3
        ];

        buildInputs = with pkgs; [
          bluez
          boost
          cubeb
          curl
          fmt_12
          glm
          glslang
          gtk3
          hidapi
          libGL
          libGLU
          libpng
          libusb1
          libxrender
          libzip
          openssl
          pugixml
          rapidjson
          sdl3
          spirv-tools
          vulkan-headers
          vulkan-loader
          wayland
          wayland-protocols
          wxwidgets_3_3
          libx11
          libxrender
          zarchive
          zlib
          zstd
        ];

        cmakeFlags = [
          (lib.cmakeBool "ALLOW_PORTABLE" false)
          (lib.cmakeBool "ENABLE_FERAL_GAMEMODE" true)
          (lib.cmakeBool "ENABLE_VCPKG" false)
          (lib.cmakeFeature "CMAKE_PROJECT_INCLUDE" (toString devCmakeInit))
          (lib.cmakeFeature "EMULATOR_VERSION_MAJOR" "2")
          (lib.cmakeFeature "EMULATOR_VERSION_MINOR" "0")
        ];

        postPatch = ''
          # The Nixpkgs wxWidgets package exposes wx-config rather than the
          # vcpkg wxWidgets::wxWidgets target.  The module already creates a
          # complete wx::base interface target in this case, so provide the
          # compatibility alias expected by CemuExtend.
          substituteInPlace cmake/FindwxWidgets.cmake \
            --replace-fail \
              '	find_package_handle_standard_args(wxWidgets' \
              '	if (NOT TARGET wxWidgets::wxWidgets AND TARGET wx::base)
		add_library(wxWidgets::wxWidgets ALIAS wx::base)
	endif()

	find_package_handle_standard_args(wxWidgets'
        '';

        preConfigure = ''
          rm -rf dependencies/imgui dependencies/Vulkan-Headers dependencies/libcemuextend
          ln -s ${imgui-src} dependencies/imgui
          ln -s ${pkgs.vulkan-headers} dependencies/Vulkan-Headers
          cp -r ${libcemuextend-src} dependencies/libcemuextend
          chmod -R u+w dependencies/libcemuextend

          substituteInPlace dependencies/gamemode/lib/gamemode_client.h \
            --replace-fail "libgamemode.so.0" \
            "${pkgs.gamemode.lib}/lib/libgamemode.so.0"
        '';

        installPhase = ''
          runHook preInstall

          install -Dm755 ../bin/Cemu_release $out/bin/Cemu
          ln -s Cemu $out/bin/cemu

          install -d $out/share/Cemu
          cp -r ../bin/resources ../bin/gameProfiles $out/share/Cemu/

          install -d $out/share/applications
          substitute ../dist/linux/info.cemu.Cemu.desktop \
            $out/share/applications/info.cemu.Cemu.desktop \
            --replace-fail "Exec=Cemu" "Exec=$out/bin/cemu"

          install -Dm644 ../dist/linux/info.cemu.Cemu.metainfo.xml \
            $out/share/metainfo/info.cemu.Cemu.metainfo.xml
          install -Dm644 ../src/resource/logo_icon.png \
            $out/share/icons/hicolor/128x128/apps/info.cemu.Cemu.png

          runHook postInstall
        '';

        preFixup = ''
          gappsWrapperArgs+=(
            --prefix LD_LIBRARY_PATH : "${lib.makeLibraryPath [ pkgs.vulkan-loader ]}"
          )
        '';

        strictDeps = true;
        enableParallelBuilding = true;

        meta = {
          description = "CemuExtend Wii U emulator build for NixOS";
          homepage = "https://github.com/PinkDiamondTeam/CemuExtend";
          license = lib.licenses.mpl20;
          mainProgram = "cemu";
          platforms = [ system ];
        };
      };
    in
    {
      packages.${system} = {
        default = cemu;
        cemu-extend = cemu;
      };

      apps.${system}.default = {
        type = "app";
        program = lib.getExe cemu;
        meta.description = "Run CemuExtend";
      };

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ cemu ];
        packages = [ pkgs.git ];
        CMAKE_PROJECT_INCLUDE = devCmakeInit;

        shellHook = ''
          echo "CemuExtend Nix development shell"
          echo "Configure with: cmake -S . -B build/nix -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_VCPKG=OFF -DALLOW_PORTABLE=OFF"
        '';
      };

      checks.${system}.default = cemu;
      formatter.${system} = pkgs.nixfmt;
    };
}
