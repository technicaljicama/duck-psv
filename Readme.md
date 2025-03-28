This is a port of "Ducktales Remastered" the android version.
This port is in an incomplete state, only a "Proof of concept".

# Issues
- No audio (theoretically there but it causes to crash the game after the tutorial state)
- Missing HUD/UI elements (they are there but not displayed. No idea why, saving is also impacted by this)
- Disabled graphical particles (they caused the vita GPU to crash for unknown reasons)
- The game runs slow (the vita CPU is the fault, overclocking helps a bit)

# To play the game
- Get the game's apk and obb. It HAS to be version 1.0.3
- Create a new folder called `duck` in `ux0:data`.
- Then, in `ux0:data/duck` create a folder called `cache`.
- Open the apk in a zip program and extract `libDuckTales.so` from `lib/armv7`.
- Copy `main.124.com.disney.ducktalesremastered_goo.obb` and `libDuckTales.so` to `ux0:data/duck`.
- Install [kubridge](https://github.com/TheOfficialFloW/kubridge/releases/) (You already have this if you installed any other vita android port.)
- Install the vpk from the releases tab.
- Launch
## Note: The first launch will take longer. After the initial boot, the game should start in 30 seconds the next time you open it.

# Compiling the loader code (not needed if you want to just play the game)
- Make sure you have vitasdk-softfp
- Get FMOD and run `build_fmod_linux.sh` [FMOD-PSV](https://github.com/GrapheneCt/FMOD-PSV)
- `cmake .`
- `make`

# Credits
Thank you to Capcom and Wayforward for making this wonderful game.

Thank you to JulaDDR, Graphene, SonicMastr and more I probably missed that helped me along this journey a lot.

# THIS REPOSITORY DOES NOT INCLUDE ANY ASSETS OR LOGOS RELATED TO THE ORIGINAL GAME. OWN COPIES OF THE GAME HAVE TO BE SUPPLIED.
# THIS PROGRAM IS AN INDEPENDENT FAN-MADE LOADER AND IS IN NO WAY AFFILIATED WITH, ENDORSED BY, OR SPONSORED BY CAPCOM, DISNEY, OR WAYFORWARD TECHNOLOGIES. ALL TRADEMARKS, LOGOS, AND INTELLECTUAL PROPERTY RIGHTS RELATED TO CAPCOM, DISNEY, AND WAYFORWARD BELONG TO THEIR RESPECTIVE OWNERS.

