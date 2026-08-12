AQUANEXUS - QUICK START AND CONTROLS
===================================

DESKTOP BUILD (MSYS2 UCRT64)
----------------------------
1. Open the MSYS2 UCRT64 terminal.
2. Open this extracted project folder.
3. Run:  chmod +x build_msys2.sh run_msys2.sh
4. Run:  ./build_msys2.sh
5. Run:  ./run_msys2.sh

KEYBOARD AND MOUSE CONTROLS
---------------------------
Right mouse drag  - Orbit the camera
Mouse wheel       - Zoom in or out
Arrow keys         - Move the camera target
Q / E             - Move the camera target down or up
V                 - Restore the reference camera view
C                 - Start or stop the close cinematic pond tour
1 / 2 / 3         - Start or stop Fan 1 / Fan 2 / Fan 3
F                 - Start or stop all circulation fans
D                 - Switch directly to DAY mode
N                 - Switch directly to NIGHT mode
SPACE             - Pause or resume the simulation
X                 - Cycle simulation speed: 1x -> 5x -> 20x
R                 - Reset the simulation and camera
H                 - Hide or show all HUD cards
B                 - Enlarge or restore the Pond 2 sensor telemetry HUD
F11               - Enter or leave fullscreen
ESC               - Exit the application

CLICKABLE HUD
-------------
The fan rows, all-fans control, speed choices, pause, reset, DAY, NIGHT and
fullscreen buttons can all be clicked. The simulation starts in DAY mode.

POND 2 SENSOR HUD
-----------------
The normal right-side sensor HUD now uses larger, clearer text. Press B to open
the expanded live sensor view. It shows all six readings with large values,
channel descriptions, condition status and the overall water-quality index.
Press B again to return to the normal right-side HUD.

NIGHT ENVIRONMENT MODEL
-----------------------
Night mode gradually cools the pond and slightly lowers dissolved oxygen and
pH because photosynthesis stops while aquatic respiration continues. The
unionised NH3 reading also eases slightly in cooler, lower-pH water. Water
level and hydrostatic pressure continue to respond to circulation normally.

AQUANEXUS WEB LOGIN (OPTIONAL)
------------------------------
Username: aquanexus
Password: aquanexus

The login is a polished presentation/demo gate made with HTML, CSS and
JavaScript. It is not server-side production authentication.

To build the browser simulation, activate an Emscripten SDK shell and run:
    ./web/build_web.sh

Then start the local portal server:
    ./web/run_portal.sh

Open http://localhost:8080 and log in. A local HTTP server is required because
browsers do not load WebAssembly correctly by double-clicking index.html.
