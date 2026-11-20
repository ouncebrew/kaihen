# HEN

HEN is the resident homebrew environment. The native bootstrap starts it
and hands it the sessions it keeps for the whole boot: the FS proxy root,
FS's SM session, the process handles for game launches, and the control
sessions of the plugins. Build it with `make` in kaiHEN (or `make -C hen`);
`sh hen/test.sh` runs its host tests.

One thread serves HEN's IPC and runs game arming as resumable steps. A
synchronous service call on that thread pauses the whole loop, so keep slow
work off it.

## Control interface

The bootstrap and RO use the control interface (`common/control.h`, HEN's
own CMIF commands). It adopts the sessions and handles above, creates
homebrew sessions, starts game arming and reports its state, and, for each
plugin, creates an SM proxy session and a clone of the FS root.

## Homebrew interface

Homebrew programs get a HEN session through RO
(`kaihenOpenHomebrewSession` in `libkaihen/include/kaihen.h`). Its
commands are declared in `libkaihen/include/kaihen/protocol.h`, protocol
version 7:

| Command | Returns |
|---|---|
| 0 `GetStatus` | HEN's process ID, protocol version and startup result |
| 16 `GetFilesystemRoot` | a new clone of the FS proxy root |
| 17 `GetSettingsSession` | a new clone of HEN's `set:sys` session |
| 18 `GetProcessHandle` | a copy of the caller's process handle |
| 19 `GetServiceManager` | a new session on HEN's SM proxy |
| 20, 21 `GetPluginCount`, `GetPluginName` | the plugins that publish IPC |
| 22 `OpenPlugin` | a new session with a plugin |
| 23 `GetLoaderRoot` | a new clone of the fsp-ldr root |

The FS root is already initialized. libounce installs it as the `fsp-srv`
override and clones it without sending `SetCurrentProcess`, which would
change the process identity that FS uses for every holder of the root.

The loader passes each libounce Switch 2 program a clone of the fsp-ldr
root as its `fsp-ldr` override, so `fsldrInitialize` and
`fsldrOpenCodeFileSystem` work as on Switch 1. libounce does not send
`SetCurrentProcess` on the override either: every clone shares the system
Loader's object. A program without libounce's Switch 2 mark gets no
override.

## SM proxy

Programs never receive FS's SM session: SM closes a session after a request
it cannot process, and a shared session would then take SM from every
program until HEN restarted. Each program gets its own session on HEN's SM
proxy (`source/sm_proxy.cpp`), served on a thread of its own so that a
lookup that waits does not stop HEN's main loop. The proxy answers
RegisterClient and DetachClient itself and forwards GetServiceHandle to SM
as a request it builds from the decoded arguments; it refuses
RegisterService and UnregisterService. A request it cannot decode closes
only that program's session.

## Game arming

When the player holds R while starting a native game, HEN lets the game's
process start with the homebrew loader in place of the game. The first such
launch runs the game normally while HEN saves its signed policy to the SD
card; later launches start hbloader, which runs hbmenu.

A Switch 1 game, which Switch 2 runs in compatibility mode, keeps its
Switch 1 program ID, and Loader mounts its code with no content attributes.
Loader loads all of its modules (`rtld`, `main`, `subsdk*`, `sdk`) from that
mount, so the package that HEN swaps in has to be built from the title's
own ExeFS, with hbloader in place of `rtld`. HEN looks for it at
`/kaihen/bin/s1/<program ID>.nsp` on the SD card, the program ID as 16
lowercase hex digits. With that package, an R launch of the game starts
hbmenu; without it, the game starts normally. HEN does not build these
packages. HEN arms no launch, of either kind, before it has saved a native
game's policy: until then an R launch runs stock, and only a native game's
launch saves it.
