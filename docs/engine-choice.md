# Engine choice for the Quest game (QuestMark study)

Investigation date: 2026-11-06. Status: investigation only. The Quest game has not started; this repository (QuestMark) holds the engine study and benchmark.

Everything below comes from reading source code (study clones of each repo), reading talk captions, and web reports. **Nothing was built, run or measured on a Steam Deck.** Every ranking is a judgment, not a benchmark result.

Labels used in this file:

- **Proved**: read in source or on a fetched page; the evidence is cited in section 14.
- **Inference / hypothesis / not verified**: reasoning or a source that was not checked. Marked in the text.
- "Snippet": the claim comes from a web-search result summary and the page was not opened.

---

## 1. Summary

**Recommendation: Godot 4.6 or newer with Jolt physics.**

Final ranking for a physics-heavy 3D game at 60 fps on a Steam Deck LCD, built largely by AI agents:

| Rank | Choice | Why (short) |
|---|---|---|
| 1 | **Godot 4.6+ with Jolt** | Best Deck-relevant renderer features (TAA, FSR2, ubershaders, auto-instancing, CPU occlusion culling), the most agent-usable knowledge, Jolt solver on all cores. Its CPU problems have known workarounds. |
| 2 | **Stride** (via Proton) | Most parallel complete engine with an editor: transforms, culling, draw recording, Bepu step and physics write-back all run in parallel. Risks: beta, D3D through DXVK under Proton, no ragdoll. Its Linux crashes and Windows-only editor do not matter for a Windows-only game. |
| 3 | **ezEngine** (Windows build via Proton/DXVK) | Richest Jolt components, update overlaps render, CPU occlusion culling, agent tooling. But serial transform update, serial Jolt write-back, DX11 immediate context only, MSAA-only renderer. |
| 4 | **Bevy + Avian** | Best threading design. But physics is third-party, no editor, f32 coordinates by default, API breaks about every 3 months, no ragdoll builder. |
| 5 | **O3DE** | Strongest renderer, but serial gameplay, serial write-back by default, heavy boilerplate, declining project funding. |
| Building blocks, not engines | The Forge, BRUTAL, SDL3, raylib | You would build the engine yourself. The Forge has the strongest renderer base of these (section 3.5), but no physics, scene, TAA/upscaler or editor. |

Why in one paragraph: all candidates share the main risk of this goal, which is main-thread CPU work per physics object on four Zen 2 cores. Godot has the weakest threading of the complete engines (serial dirty-instance loop, serial Jolt write-back to nodes, gameplay on the main thread), but each problem has a documented workaround: physics server bodies (RIDs) plus MultiMesh, the experimental separate render thread, and C++ in a GDExtension. In return it has the best renderer features for a Deck-class GPU and the largest body of material that AI agents know. The engines that thread better (Stride, Bevy) give up built-in physics, ragdolls or API stability, or (Stride) need a D3D-to-Vulkan translation layer under Proton. The only way to settle the order is a benchmark on a real Deck (section 12).

**Source freshness.** Rankings rest on pinned clones taken at the default-branch heads of 2026-10-31 to 2026-11-05. Head check on 2026-11-06 (section 3.6, Jolt in 3.7): every repo with a gap was diffed, nothing changed a claim or the ranking. Godot results are for 4.8-beta master, not stable 4.6.

---

## 2. Goal, constraints, method, limits

**Goal.** A high-performance, physics-heavy 3D game ("Quest"). Much of the code is written by AI agents. Target: 60 fps (16.7 ms frame) on a Steam Deck LCD. All candidates had to be open source.

**Platform (user, 2026-11-06).** Quest ships as a **Windows build only**. There is no native Linux build. The Steam Deck is a performance target, reached through Proton. The game is not Deck-first. Native Linux or Linux-Vulkan support therefore counts for nothing in this comparison. What counts is how each engine's Windows renderer runs under Proton on the Deck.

**Steam Deck LCD (Valve spec page).** Zen 2, 4 cores / 8 threads, 2.4-3.5 GHz. 8 RDNA2 compute units at 1.6 GHz. 16 GB LPDDR5 at 5500 MT/s. APU power 4-15 W. Display 1280x800 at 60 Hz. The OLED model differs; the two were not mixed.

**Proton.** Every candidate's Windows build reaches the Deck through Proton. The path depends on the graphics API the Windows build uses:

| Windows API | Proton layer | Candidates (Windows default) |
|---|---|---|
| Vulkan | winevulkan, a thin pass-through to Mesa RADV | Godot (`driver.windows` defaults to `vulkan`, `main.cpp:2388`), Bevy/wgpu (backend choice not checked), BRUTAL, SDL_GPU (backend choice not checked) |
| D3D11 | DXVK | ezEngine (DX11 is its only complete renderer), Stride (default API not checked) |
| D3D12 | vkd3d-proton | O3DE (default RHI not checked), The Forge 1.64 (D3D12 is its only public runtime, section 3.5) |
| OpenGL | Wine's OpenGL path | raylib |

Translation overhead was not measured. That D3D translation costs more CPU than Vulkan pass-through is an assumption (not checked); the Deck benchmark should settle it. The Deck's system-level FSR1 is spatial only.

**Method.**
- Study clones of each repo were read at a pinned commit, the default-branch head at the time (section 3). Head check on 2026-11-06: section 3.6. Godot and ezEngine were full clones. O3DE, Bevy and Stride were blobless sparse clones read with `git show HEAD:<path>`. BRUTAL, SDL3, raylib, J.E.N.O.V.A and Avian were shallow clones.
- Sonnet-class subagents read code per question. Claims that decide the ranking were spot-checked by the main session against the files.
- Official Godot talks were read from YouTube auto-captions (names and version numbers in captions are unreliable).
- Community problems were collected from the web; many statuses are snippet-level.

**Limits.**
- No build, no run, no measurement on any machine. No engine has published Deck CPU numbers that were found.
- Much web evidence is at search-snippet level.
- Not read or not obtainable: the Godot licence text in full; the FidelityFX licence file; the Jolt multicore-scaling PDF (unreadable); the Jolt GDC 2022 talk; the SteamOS Mesa version; DXVK and vkd3d-proton overhead on the Deck.
- Ranks are judgment. Where the text says "hypothesis" it was not tested.

---

## 3. Candidates studied

### 3.1 Versions, language, physics, renderer, editor, licence

| Candidate | Version / commit | Language | Physics | Renderer / graphics API | Platforms | Editor | Licence |
|---|---|---|---|---|---|---|---|
| **Godot** | 4.8-beta master, `95c0fa5` | GDScript, C#, C++ via GDExtension | Jolt 5.6.0 (one backport patch), default for projects created from 4.6; GodotPhysics3D still available | Vulkan, D3D12, Metal, GLES3 | Linux, Windows, macOS, others | Yes, all desktop OSes | MIT (full licence text not read) |
| **ezEngine** | 26.9, `fb0a247` | C++, AngelScript, visual scripts | Jolt 5.5.1-dev snapshot (about 2026-06-06, section 3.7) | DX11 (complete), Vulkan (in progress, API 1.1). No DX12 | Full engine Windows only per README; non-rendering code builds on Windows, Android, Linux, macOS | Yes (Qt) | MIT |
| **O3DE** | 2.8.0, `703bbae` (2026-10-29) | C++, Lua, Script Canvas | PhysX 5.1.1-rev4 | Atom: DX12, Vulkan, Metal | Windows, Linux, others | Yes (Qt) | Apache-2.0 OR MIT (`LICENSE.txt:1-5`) |
| **Stride** | 4.5.0-beta01, `27f25ec` | C# (.NET 10) | Bepu 2.5.0-beta.28 (Bullet kept alongside) | D3D11, D3D12, Vulkan | Windows, Linux (crashes reported) | Windows-only WPF | MIT |
| **Bevy** | 0.20-dev, `ae61521` | Rust | None built in | wgpu 30 (Vulkan, D3D12, Metal) | Desktop, others | None | MIT or Apache-2.0 |
| **Avian** (Bevy physics) | 0.8.0, `2c2d693` (2026-11-04) | Rust | Own solver, Parry for narrow phase | n/a | with Bevy 0.20 | None | not read |
| **BRUTAL** | `7a3b72d` (2026-10-27) | C# (.NET 10) | Full Jolt binding (same Jolt commit as Godot's 5.6.0, `e77f175`); Box2D | Vulkan 1.3 bindings only | README: Windows or Linux; samples README also lists macOS (conflict, section 10) | None | MIT plus required credit/logo; not OSI-approved |
| **SDL3** | 3.5.0, `c58854d` | C | None | SDL_GPU on Vulkan, D3D12, Metal | All major | None | zlib |
| **raylib** | 6.5-dev, `4bfb72d` | C | None | OpenGL 1.1-4.3 / ES, software renderer | All major | None | zlib |
| **The Forge** | 1.64, Codeberg `5abcdd9` (2026-08); GitHub frozen at 1.63 | C API, C++ apps, FSL shaders | None | D3D12 only in the public repo (Vulkan and Metal removed in 1.64) | Public repo: Windows only. README lists Deck, mobile and consoles (console code licensed separately) | None public (Gladiator editor named in README only) | Apache-2.0 (PC); consoles commercial |
| **J.E.N.O.V.A** | `4bd544d`; Jenova-Runtime `f9cb794` | C++ scripting for Godot | n/a (Godot's) | n/a | Windows x64, Linux x64 | Godot editor | Runtime MIT; other parts proprietary or custom (section 10) |

### 3.2 Physics detail

| | Physics engine | Rigid / constraints | Character | Ragdoll | Vehicle | Cloth / soft | Multithreaded step |
|---|---|---|---|---|---|---|---|
| **Godot 4.8 beta** | Jolt 5.6.0 | Yes / cone twist, 6DOF, hinge, pin, slider | Yes | PhysicalBone3D | Godot's own raycast-wheel `VehicleBody3D`; Jolt vehicle sources compiled but no wrapper | SoftBody3D (some limits) | Yes, Jolt jobs on the WorkerThreadPool |
| **ezEngine 26.9** | Jolt 5.5.1-dev snapshot (section 3.7) | Yes / 9 joint types + rope, breakable slab | Yes | Yes | No vehicle component (Jolt vehicle sources compiled) | Jolt soft body, cloth sheet | Yes |
| **O3DE 2.8** | PhysX 5.1.1 | Yes / 5 joint types + articulations | Yes | Yes | No | NvCloth | Yes (job-based CPU dispatcher) |
| **Stride 4.5 beta** | Bepu 2.5.0-beta.28 | Yes / 35 constraint types | Yes | No | No | Minimal | Yes, fixed 1/60 s step |
| **Bevy 0.20-dev** | None (Avian or Rapier) | - | - | - | - | - | ECS is multithreaded |
| **Avian 0.8.0** | Own solver | Fixed, distance, prismatic, revolute, spherical; XPBD | `MoveAndSlide` kinematic helper | No builder, no articulations | None found in `src/` (not grepped outside) | Not read | Yes, parallel within graph colours |
| **BRUTAL** | Jolt binding | Yes / about 14 types | Yes | Yes | All Jolt vehicle types | Yes | Job system bound; Jolt thread count configurable |
| **SDL3 / raylib / The Forge** | None (The Forge has ozz animation with IK, but no physics) | - | - | - | - | - | - |

### 3.3 Engine, renderer and agent fit

| | Language | Graphics APIs | Renderer highlights | Scene format | Agent fit | Main risk |
|---|---|---|---|---|---|---|
| **Godot** | GDScript, C#, C++ | Vulkan, D3D12, Metal, GL | SDFGI, VoxelGI, SSR, SSIL, SSAO, volumetric fog, FSR1, FSR2, TAA, SMAA, MetalFX, VRS | Text `.tscn` | Best: most training data, `--headless`, doctest, 833 class-doc XML files | No GPU-driven rendering; scene-tree overhead |
| **ezEngine** | C++, AngelScript | Vulkan, DX11 | Clustered lighting, SSAO, baked probes; **MSAA only** | OpenDDL text | Good: `AGENTS.md`, MCP editor plugin | Weakest renderer of the engines; small community |
| **O3DE** | C++, Lua, Script Canvas | DX12, Vulkan, Metal | Ray tracing, probe GI, TAA, meshlets (proof of concept) | JSON prefab | Weak: about 16k C++ files, GUID/Reflect/editor-twin boilerplate per component | Heaviest engine; declining community |
| **Stride** | C# (.NET 10) | D3D11, D3D12, Vulkan | Clustered lighting, SSR, TAA, FXAA, light probes | YAML | C# suits agents; 57 test projects | Beta packages; no ragdoll; Windows-only editor |
| **Bevy** | Rust | Vulkan, DX12, Metal | GPU-driven culling, occlusion culling (opt-in); meshlets and Solari experimental | Code / BSN (new) | Strong compiler feedback; live inspection over JSON-RPC (BRP) | No physics; API breaks about every 3 months |
| **BRUTAL** | C# | Vulkan 1.3 | None | glTF only | No tests, no `AGENTS.md`; mostly generated code | NuGet only, no build files yet; logo licence |
| **SDL3** | C | Vulkan, D3D12, Metal | None | None | Has an `AGENTS.md` (lines 1-4) that bans AI-written contributions to SDL itself, not use in games | You build the whole engine |
| **raylib** | C | OpenGL <= 4.3 | Example code for PBR, shadows, instancing | None | Small, readable code | OpenGL caps performance |
| **The Forge** | C API, C++ apps | D3D12 (public repo) | GPU-driven visibility buffer, GPU pre-skinning, offline shaders + pipeline library, texture streaming. SSR, shadows and light clusters only in unit tests. No TAA, upscaler or occlusion culling | None (own `GeometryTF` mesh format) | Weak: VS2019 solutions only, no CI, no `AGENTS.md`, own shader language | No physics, scene or editor; public scope shrank in 1.64 |

### 3.4 Render reference repos (read for porting, section 7)

Also cloned and read for shader and algorithm sources, not as candidates: filament, Lumina, Lumix, Wicked Engine, ASSAO, DiligentFX, Falcor, Flax, Overload, Piccolo, The-Forge, XeGTAO, and the AMD FidelityFX repos (CACAO, CAS, FSR, SDK `7539ae8`, SSSR). The FidelityFX licence file was not read. The Forge was later also studied as a candidate (section 3.5).

### 3.5 The Forge (framework, studied 2026-11-06)

**Repos.** Development moved from GitHub to Codeberg.
- **Codeberg** `The-Forge/The-Forge` is release 1.64 (2026-08-12), commit `5abcdd9`. Its history starts at "Initial setup", `9b71579` (2026-08-14). No earlier history was imported.
- **GitHub** `ConfettiFX/The-Forge` is release 1.63, commit `cd50468`. It has had no code changes since `6c0aa87` (2025-03-20); every commit after that changes only README.md.
- Both are shallow clones, read with `git grep`, plus two Sonnet subagent reports. The main session spot-checked the claims that decide the verdict. Nothing was built or run.
- **Licence:** Apache-2.0 for PC (`LICENSE.txt`). Consoles need a commercial licence.

**Scope of the public code (proved).**
- 1.64 holds only the Windows D3D12 runtime. README.md:24 says "This repository only holds the PC / DirectX 12 runtime".
  - `Common_3/Graphics` has `Direct3D12`, `FSL`, `Interfaces` and `ThirdParty` only. `Common_3/OS` has `Windows` plus shared code.
  - Removed against 1.63: the Vulkan, Metal, OpenXR and Quest backends; the Linux, Android, Darwin and Quest OS layers; the ParticleSystem renderer. Also removed are the SteamOS, Android, Quest and macOS project folders.
  - Added in 1.64: texture streaming (`Common_3/Resources/Streaming`), a gizmo, a node graph and reflection.
- The README still lists the Steam Deck (Vulkan 1.1) as a platform. No public code backs that any more. The last public Deck port is 1.63:
  - a native X11/evdev Linux layer of about 4,000 lines and a Vulkan 1.1 backend of about 10,400 lines;
  - CodeLite projects built on a Deck;
  - Deck workarounds keyed on device `0x163F`.
- Own code: about 209k lines in 1.64 and 198k in 1.63 (`.c/.cpp/.h/.fsl` outside ThirdParty).

**For Quest (Windows build, Deck through Proton).** The 1.64 D3D12 runtime is the build that would ship, through vkd3d-proton on the Deck.
- **D3D12 needs** (agent-read):
  - Shader Model 6.0 with wave intrinsics is a hard requirement (`Direct3D12.c:3034-3075`).
  - Agility SDK 715 (`TF_Shared.props:110`). Pipelines are cached with `ID3D12PipelineLibrary` (`Direct3D12.c:5246-5754`). Drawing uses `ExecuteIndirect` with counts.
  - Optional: DXR / inline ray query, work graphs and cooperative vectors.
  - Not used: enhanced barriers, mesh shaders, hardware VRS, sampler feedback, DirectStorage.
  - **Whether vkd3d-proton runs this on RADV was not tested.**
- **GPU preset:** the Windows GPU list rates the Deck GPU (`0x1002;0x163F`) **`verylow`** (`Common_3/OS/Windows/pc_gpu.data:306`, proved). A game would override the preset.

**What it provides (proved unless marked).**
- **Frame loop:** `Update` then `Draw` in sequence on the main thread (`Common_3/OS/Windows/WindowsBase.cpp:783,794`). There is no render thread. Two frames are in flight (`IApp.h:103`, agent-read).
  - A flat thread pool, `ThreadSystem`, has one worker per core and no dependency graph (`Common_3/Utilities/Threading/ThreadSystem.h:93-124`, agent-read).
  - Recording command buffers on several threads is shown in `03_MultiThread` (agent-read).
- **Renderer library** (`Common_3/Renderer/VisibilityBuffer`, `VisibilityBuffer2`): a GPU-driven visibility buffer.
  - Compute triangle filtering (backface, frustum, small-primitive) and indirect multi-draw (agent-read).
  - GPU pre-skinning.
  - Assets use its own `GeometryTF` meshlet format, made with `Common_3/Tools/AssetPipeline` (cgltf-based).
  - **Missing:** Hi-Z or occlusion culling, mesh shaders.
- **Effects only in unit tests,** not as libraries: SSR (`10_ScreenSpaceReflections`), shadow techniques (`09_LightShadowPlayground`: ASM, ESM, MSM, SDF, VSM), light clusters (`15a_VisibilityBufferOIT`).
  - **No TAA, FSR, DLSS, XeSS or any upscaler.** A grep over `Common_3` and `Examples_3` found only a jitter comment, `Camera.cpp:990`.
  - Aura (GI), PixelPuzzle (post effects), Ephemeris (sky) and the Gladiator editor are named in the 1.64 README, but their code is not in the repo.
- **Shaders:** FSL, a superset of HLSL, compiled offline with DXC by a bundled Python 3.6. The runtime loads bytecode only. Together with the pipeline library this targets first-use shader stutter; its effect was not measured.
- **Loading:** an async resource loader on its own thread (`ResourceLoader.cpp:2586-2599`). Texture streaming (`Streaming.cpp`) is used only by `15a`. There is no mesh streaming, LOD system or sparse texture support.
- **Game layer:**
  - Lua scripting.
  - flecs 4.0.1, vendored and used by one unit test only (`19_CodeHotReload_Main.cpp:26,180-230`).
  - **No scene graph, transform hierarchy, scene format or editor.**
- **Animation:** ozz with blending, additive layers and AimIK/TwoBonesIK (agent-read).
- **Missing:** physics, audio and networking. A grep for `jolt|physx|bullet|bepu|havok` found no engine. Ray tests exist only for the gizmo and the UI.
- **Other:**
  - Input supports gamepads (HID parsers for PS4, PS5 and Switch controllers).
  - Profiler: built in, CPU plus GPU timestamps, MicroProfile-derived. No Tracy.
- **Build and agent fit:**
  - Visual Studio 2019 solutions only, no CMake. `PRE_BUILD.bat` downloads art from the vendor's server.
  - No CI configuration, no `AGENTS.md`.
  - Public API is C (`extern "C"`, `IGraphics.h:2704`); apps are C++ `IApp` classes.
  - Little public material for agents compared with Godot (assumption).

**Godot problem cross-check:** see The Forge column in section 4.4. The Forge also appears in the tables of sections 3.1-3.3, 4.2, 4.5, 5 and 8.

**Verdict (judgment).** Of the frameworks studied, The Forge has the strongest renderer base: a GPU-driven visibility buffer, offline shaders with a pipeline cache, async loading and animation. BRUTAL, SDL3 and raylib have no renderer at all. It is still a building block, not an engine. Quest would have to add:
- physics (for example Jolt in C++);
- a scene and transform layer, and gameplay threading;
- TAA and an upscaler, which matter at 1280x800 on the Deck;
- occlusion culling;
- mesh streaming;
- an editor or level tooling;
- audio;
- agent-friendly build and test tooling.

More risks (community issues: section 8):
- **Public scope shrinking:** the Vulkan, Linux and Mac backends left the public repo in 1.64, and the newest features are README-only.
- **Single vendor** with a fresh repo history.
- **Slow and thin support (proved from issues):** 17 months without a release before 1.64; bug reports wait for the next release (details: section 8).
- **GPU gate (hypothesis for the Deck):** a D3D12 sample refused an AMD RX 570 (#327, open, cause unknown). Whether vkd3d-proton on the Deck passes it was not tested (details: section 8).

It does not change the ranking in section 1. It ranks above BRUTAL, SDL3 and raylib as a base for a custom engine, and it fits only if Quest decides to build its own engine.
### 3.6 Head check (2026-11-06): pinned commit versus current head

Rule: the latest source (default-branch head) is used, not stable releases. The Godot results are for 4.8-beta master, not for the stable 4.6 that a shipped game would use; that 4.6-versus-master difference was not checked.

| Repo | Pinned | Head on 2026-11-06 | Commits behind | Changes relevant to this doc |
|---|---|---|---|---|
| Godot | `95c0fa5` | `b47a795` | 92 | Net tree diff read (2026-11-06). `modules/jolt_physics` unchanged. No scene, renderer-RD feature or threading-model change. `CommandQueueMT` lock-handling refactor/race fix (`core/templates/command_queue_mt.h`); `RenderingDevice` push-constant set invalidation; VMA flag for AMD integrated GPUs (assumption: the Deck APU benefits, and whether it applies under Proton/Vulkan translation is unchecked). Cited lines in `command_queue_mt.h` have shifted. |
| ezEngine | `fb0a247` | `205f28c` | 11 | Full diff of `Code/` read (2026-11-06): particles (spawn areas, mesh position initializer, one allocation per stream group), editor dialogs and export, a reworked Asteroids sample, mini-audio preview, and a mesh-query message so particles can read a component's mesh (`ezMsgQueryMesh`, LOD and animated mesh components, `CpuMeshResource` fallback). No change to `Core/World`, `JoltPlugin`, `Foundation/Threading`, `RendererDX11`, `RendererFoundation`, the render pipeline or `Code/ThirdParty`. All doc claims about ezEngine stand. (The "Inverse Depth" commit `fb0a247` is the pinned commit itself, not a later change; an earlier version of this row listed it wrongly.) |
| Jolt (upstream) | ezEngine copy matches about 2026-06-06; Godot `e77f175` (5.6.0, 2026-07-11) | `a76f1d7` (5.6.1-dev, 2026-11-06) | 95 after 5.6.0 | See section 3.7. |
| O3DE | `703bbae` | `19c096e` | 29 | Net tree diff read (2026-11-06): a "26.10.0" splash/logo update (a stabilization-branch name; `engine.json` still says 2.8.0, `display_version` "00.00"), zlib/expat updates, cleanup. No net change to articulation code, gameplay threading or renderer features. |
| Stride | `27f25ec` | `2fd38d5` | 258 | Per-file list and key diffs read (2026-11-06). Shader compiler fixes, build determinism, CLI/release tooling, Avalonia editor groundwork (mostly editor folders). Fallback effects keep instancing (#3779). D3D11 keyed-mutex texture sharing; Vulkan `ExportableTexture` (opt-in external memory/semaphore FDs, off by default). Bullet plugin: heightfield collider and navigation determinism. `FastList`/`UnmanagedArray` replaced by `List<T>`/`Span<T>` and profiler docs only. `plugins/BepuPhysics` has no diff, and the cited `Processors`, `RenderSystem`/`VisibilityGroup` logic is unchanged. Not read: the whole editor tree. |
| Bevy | `ae61521` | `159fa8d` | 29 | `bevy_transform/src/systems.rs`: `mark_dirty_trees` traversal task count helper (no behaviour change; cited lines after `:196` shift by about 8). `render/mesh.rs`: wireframe specialization. No threading or culling change. |
| Avian, SDL3, raylib, BRUTAL, The Forge (Codeberg) | `2c2d693`, `c58854d`, `4bfb72d`, `7a3b72d`, `5abcdd9` | same | 0 | The pinned commit is the head. |

**Method and limits.** Heads from `git ls-remote`; gaps from the GitHub compare and commit-by-path APIs. Proved: the commit counts and the commit messages above. Limits: the compare file list is capped at 300 files (ezEngine, Stride), so for those two the check relied on commit-by-path queries on the files and folders the doc cites. Net diffs were read for Godot, ezEngine and O3DE, and per-file lists plus key diffs for Stride (not the editor tree); Bevy was read only for `systems.rs`/`mesh.rs`. Only the cited areas were checked, not every claim. Nothing was built or run.

### 3.7 Jolt versions in the candidates versus Jolt upstream (2026-11-06)

Jolt upstream head: `a76f1d7` (2026-11-06), `Core.h` version 5.6.1-dev. Latest tag: v5.6.0 (`e77f175`, 2026-07-11); v5.5.0 is `23dadd0` (2025-12-28). Cloned blobless into the project temp folder (not built).

| User | Jolt version | Gap to upstream head |
|---|---|---|
| Godot (`b47a795` head and the pinned commit) | 5.6.0, `e77f175` (`thirdparty/README.md:517`; `Core.h:8-10` reads 5.6.0) | 95 commits |
| ezEngine (`Code/ThirdParty/Jolt`) | `Core.h` says 5.5.1 (the 5.5.1-dev line). Files match upstream by blob hash at 2026-06-06 (`PhysicsSystem.cpp` = `651a094`), 2026-05-31 (`ContactConstraintManager.cpp` = `0f58921`) and 2026-03-20 (`JobSystemThreadPool.cpp` = `6576239`). It is an ancestor of 5.6.0, 24 commits before it; the difference to 5.6.0 is 261 added and 110 removed lines in 18 files under `Jolt/` | about 119 commits |
| BRUTAL | same Jolt commit as Godot's 5.6.0 (`e77f175`) per the doc's earlier reading | 95 commits |
| JoltPhysicsSharp 2.23.0 | not checked | - |

**What upstream has since 5.5.0 (from `Docs/ReleaseNotes.md` and `git log`; proved as written, effect on Quest not measured):**
- In 5.6.0, so in Godot and (mostly) in ezEngine: a new friction model (15% faster, 40% less memory in the Pyramid test), "up to 40% performance improvement and up to 70% memory reduction (scene dependent)", faster kinematic bodies (no shared islands with constrained bodies), `Ragdoll::DriveToPoseUsingMotors` with velocity, `RagdollSettings::CalculateConstraintPriorities` (5.5.0), GPU compute and hair interfaces (DX12, Vulkan, Metal; hair marked work in progress).
- Only in master after 5.6.0 (neither Godot nor ezEngine has them): a lock-free MPMC queue for `JobSystemThreadPool` (#2179, fixes #2158: a worker that does not wake up could block new jobs) and batched `JobSystem::CreateJobs` (#2180); per-body contact points as friction lever arms (#2154); small optimizations and fused jacobian dot products (#2153, #2151); cached contact tangents (#2141); clamped warm start for very different delta times (#2136); `PhysicsSettings::mDeterministicSimulation` removed (always deterministic); 64-bit `ObjectLayer`; fixes for `LinearCast` tunnelling with an offset centre of mass, `GearConstraint` torque, `HeightFieldShape::SetHeights`.
- **Who is affected by the job queue change:** Godot (`JoltJobSystem`, `spaces/jolt_job_system.cpp`) and ezEngine (`ezJoltJobSystem : JPH::JobSystemWithBarrier`, `JoltJobSystem.h:9`) use their own job systems on their own thread pools, so they do not use `JobSystemThreadPool` and the queue fix does not apply to them. BRUTAL's binding includes `JobSystemThreadPool.gen.cs` (evidence 14.6), so a BRUTAL- or JoltPhysicsSharp-based setup would get it with a Jolt update (assumption: which job system those bindings use by default was not checked).

**Effect on the ranking:** none. Godot and ezEngine run nearly the same Jolt (ezEngine is 24 commits older than 5.6.0, mostly character-controller fixes and tooling). Jolt master is 95 commits ahead of both; the gains are in speed and stability fixes, none of them is a new capability for the Quest physics goal. Whether Godot will update past 5.6.0 before Quest ships was not checked.

**Limits.** The ezEngine match is by blob hash of three files, so it shows the copy is a snapshot from about 2026-06-06 and may include small local edits in other files. No Jolt code was built or benchmarked; the "40%" and "15%" figures are the Jolt author's numbers from the release notes, not measured here. Only the commit titles after 5.6.0 were read, not the diffs.


---

## 4. Full comparison tables (latest "remade" version)

### 4.1 Features

Moved: type, language, licence, physics and renderer per candidate are in sections 3.1, 3.2 and 3.3 (no separate table, to avoid keeping three copies).

### 4.2 Deck and agent fit

CPU design per engine is in section 5; reported problems per engine are in section 8.

| | How it runs on the Deck | Fit for AI agents |
|---|---|---|
| **Godot** | Windows Vulkan build under Proton (winevulkan pass-through) | **Best**: largest knowledge base, text scenes |
| **ezEngine** | Windows D3D11 build under Proton (DXVK); its Vulkan renderer is unfinished | Good: `AGENTS.md`, MCP plugin, text scenes |
| **O3DE** | Windows build under Proton (DX12 via vkd3d-proton, or Vulkan; default RHI not checked) | Weak: about 16k files, boilerplate |
| **Stride** | Windows build under Proton (Distant Worlds 2 rated Playable); D3D11 via DXVK or Vulkan (default not checked) | Good C#; Windows-only WPF editor |
| **Bevy** | Windows build under Proton; wgpu backend choice under Proton not checked | Code-only, BRP inspection; API breaks about every 3 months |
| **BRUTAL** | Possible | Mostly generated code; no tests |
| **The Forge** | Windows D3D12 build under Proton (vkd3d-proton), not tested; Deck GPU rated `verylow` in its GPU list | Weak: VS2019 solutions only, no CI, no `AGENTS.md`, own shader language, little public material (assumption) |
| **SDL3** | Windows build under Proton | You build the engine |
| **raylib** | Windows OpenGL build under Proton | Simple but limited |

### 4.3 Final ranking table

See section 1. History of the rank is in section 13.

### 4.4 Godot problem cross-check (which engine solves what)

Key: Yes = solved; Partly = partly solved or opt-in; Same = same problem or missing; "-" = you build it yourself. Source-read only; nothing measured.

| Godot problem | Godot 4.8 | ezEngine | O3DE | Stride | Bevy | BRUTAL / SDL / raylib | The Forge 1.64 |
|---|---|---|---|---|---|---|---|
| **Thousands of moving objects** | Same: serial `update_dirty_instances` loop | Same: serial once a spatial system exists, i.e. in normal worlds (`WorldData.cpp:395`) | Partly: parallel jobs, but they queue on one exclusive octree lock | Yes: parallel transforms, bounds and culling | Yes: parallel, change-driven | - | - (no scene layer); GPU-driven visibility buffer handles many objects on the GPU side |
| **Rendering on the main thread** | Partly: separate render thread experimental | Partly: update overlaps render; DX11 records on one thread | Yes: async simulate; Vulkan records on several threads | Partly: update then draw in sequence; draw recording parallel | Yes: pipelined render thread on by default | Partly: SDL_GPU allows one command buffer per thread | Same: `Draw` on the main thread after `Update`; multi-threaded recording possible (unit test) |
| **Gameplay on one thread** | Same: thread groups experimental | Partly: Async phase opt-in (11 built-in components); scripts run in PreAsync (serial) | Same: serial (TickBus not opened) | Same: scripts serial | Yes: parallel by default | Partly: BRUTAL has a job scheduler | - (your code); flat `ThreadSystem` pool |
| **Sync stalls** | Same: value-returning server calls flush the queue | Partly: one wait at frame end | Partly: several waits per frame | Yes: no render thread to wait on | Partly: short main-thread stall during extract | - | - (your code) |
| **Physics step** | Jolt jobs, parallel | Jolt jobs, parallel | PhysX jobs | Bepu, parallel | Avian parallel (broad, narrow, solver) | BRUTAL: Jolt threads configurable | - (no physics) |
| **Physics write-back to objects** | Same: serial callback per awake body (RIDs avoid it) | Same: serial | Partly: parallel only if `physx_batchTransformSync`, default off | Yes: parallel | Same: serial, changed bodies only | BRUTAL: batch pose read (`GetBodyStates`) | - (no physics) |
| **Many identical meshes** | Yes: auto-merge of identical draws | Yes: auto-instancing | Yes: auto-instancing | Partly: manual only | Yes: GPU culling and indirect draws by default | Partly: API only | Yes: GPU-driven indirect multi-draw |
| **World larger than VRAM** | Partly: opt-in texture streaming (default off); no mesh streaming | Partly: mip loading, LOD meshes | Partly: mip streaming | Partly: texture streaming, 512 MB budget | Same: f32 only, no streaming found | Same | Partly: texture streaming module (one test uses it); no mesh streaming |
| **Shader stutter** | Yes (mostly): ubershaders since 4.4 | Partly: precompiled cache; runtime compile off by default | Partly: falls back to root variant while loading | Partly: async compile plus cache | Partly: objects pop in rather than hitching | Partly: you create pipelines yourself | Partly: offline shaders plus `ID3D12PipelineLibrary` cache (not measured) |
| **TAA and upscaling** | Yes: TAA, FSR2 | Same: MSAA only | Partly: TAA, no FSR | Partly: TAA, FXAA | Partly: TAA, SMAA, CAS; DLSS NVIDIA only | Same (SDL: MSAA only; raylib: MSAA hint only) | Same: none |
| **Occlusion culling** | Yes: CPU raster | Yes: CPU raster with authored occluders (needs AVX1+FMA3 x86) | Partly: software masked culling, only with hand-placed planes | Same: none found | Partly: GPU two-phase, opt-in, needs depth prepass | Same | Same: none (triangle and frustum culling only) |
| **Profiler** | Yes | Yes: Tracy | Yes | Yes | Yes: Tracy | - | Yes: built in, CPU and GPU |

### 4.5 GPU-driven drawing on the Deck (after open items)

| Row | Godot | ezEngine | O3DE | Stride | Bevy + Avian | The Forge |
|---|---|---|---|---|---|---|
| **GPU-driven drawing on Deck** | No | No | Partly | No | Expected from RADV source if wgpu picks Vulkan under Proton (not run, backend choice not checked) | Yes in source (visibility buffer, indirect draws); through vkd3d-proton, not run |

---

## 5. CPU and threading on the Steam Deck

No engine has published Deck CPU measurements that were found. Architecture comparison is from source; the rank is a hypothesis.

| Engine | Main loop / threading | Physics sync to the game | Likely main-thread bottleneck | Rank (hypothesis, first pass) |
|---|---|---|---|---|
| **Bevy** | Pipelined rendering on by default; ECS systems parallel; transform propagation and visibility culling parallel | No built-in physics | Extract step on the main thread | 1 (no physics included) |
| **ezEngine** | Update runs as a task while the main thread renders the previous frame (`GameApplication.cpp:610-635`, default on). Async phase and per-view extraction parallel. 6 workers on 8 threads. | Jolt step parallel. Write-back serial, O(active bodies) (`JoltDynamicActorComponent.cpp:30-55`) | Transform update single-threaded when a spatial system exists, which worlds create by default (`WorldData.cpp:394`). Culling within one view serial. | 2 |
| **Godot + Jolt** | Jolt jobs on WorkerThreadPool (about 8 threads on the Deck). Culling parallel only above 1000 instances. Render thread model defaults to "Safe" (`main.cpp:2787`); drawing is suspected to stay on the main thread (inference). | One Callable plus a node transform set per awake body per step, on the main thread (`jolt_body_3d.cpp:1076-1105`). Sleeping bodies cost nothing. Separate physics thread off by default. | `_process` / `_physics_process` on the main thread unless thread groups; interpreted GDScript | 3 |
| **Stride** | One game thread runs Update then Draw (`GameBase.cs:655-703`). Culling, extraction, preparation and command-list recording on the Dispatcher. | Bepu multithreaded, fixed 1/60 s | Entity processors keyed by dictionary; .NET GC pauses (hypothesis). Linux Vulkan CI runs on a software renderer only. | 4 |
| **O3DE** | Render on the main thread before the game/physics tick, fork/join jobs. About 4 job workers on 8 threads (`0.6 x (8-2)`, arithmetic by the investigator). Windows uses the same 0.6 ratio as Linux (`AzCore_Traits_Windows.h:103`, `AzCore_Traits_Linux.h:115`), so this holds for the Windows build under Proton. TaskGraph off. | PhysX blocks the main thread. Per-body sync inline by default (`physx_batchTransformSync=false`). Can catch up 6 steps after a long frame. | One TickBus virtual call per component; recursive transform-notification fan-out; Lua and Script Canvas interpreted | 5 |
| **The Forge** | App loop: `Update` then `Draw` in sequence on the main thread (`WindowsBase.cpp:783,794`), 2 frames in flight; flat `ThreadSystem` pool, one worker per core; parallel command recording possible | No physics | Whatever the game puts in `Update`; draw submission on the main thread | n/a (framework) |
| BRUTAL / SDL3 / raylib | No engine loop | n/a | n/a | n/a |

This first-pass rank was later revised after the cross-check (section 4.4, 6). The revised ranking is in section 1.

**Deck data points found (thin):**
- Distant Worlds 2 (Stride) is rated Deck Playable, but it is a 4X strategy game with no physics load.
- A Bevy developer reports 90 fps at 13 W on the Deck, for an undescribed scene.
- The often-quoted 4.1x Jolt scaling at 8 threads comes from a secondary summary; the original PDF was unreadable. Unverified.

**Godot thread facts (proved, source):**
- Jolt jobs: `jolt_job_system.cpp:106,163`; `JoltJobSystem` is created unconditionally (`jolt_physics_server_3d.cpp:1627`). Physics threading is therefore not opt-in. Only the placement of the step is opt-in: `physics/3d/run_on_separate_thread` defaults to false (`project_settings.cpp:1803`, `register_types.cpp:44/49`).
- Worker pool: one thread per logical core (talk and source agree); low-priority ratio 0.3 (`register_core_types.cpp:367-368`). Quest runs the Windows build, so the Windows core count applies. What Proton reports to it on the Deck was not checked (assumed 8 logical cores).
- Culling goes parallel at 1000 instances (`threaded_cull_minimum_instances`, `rendering_server.cpp:3827`; `renderer_scene_cull.cpp:3466-3473,3365-3367`).
- Physics ticks per second 60 (`main.cpp:2241`), max steps 8 (`:2242`).
- Render thread model `main.cpp:2787`, warning `:3556-3559`; `rendering_server_default.cpp:280`.

**Implication for Quest (judgment).** On 4 Zen 2 cores the limit is more likely serial per-body or per-node main-thread work than the physics solver. Workarounds per engine: Godot: PhysicsServer3D + RIDs + MultiMesh, and C++ GDExtension for hot code. ezEngine: keep moving objects few or parallelise write-back. O3DE: turn on `physx_batchTransformSync`.

---

## 6. Godot problems versus other engines

### 6.1 Per-engine findings (source read, with limits)

**ezEngine**
- P1 moving objects: `UpdateGlobalTransforms` walks the hierarchy level by level (`WorldData.cpp:337-410`). It uses the multithreaded traversal only if there is no spatial system (`m_pSpatialSystem == nullptr`, around `:395`). Otherwise the serial variant runs and updates spatial bounds per object. The spatial system is a regular grid with a mutex-guarded cache. Proved by reading; not measured.
- P2: `Rendering.Multithreading` defaults true (`RenderWorld.cpp:18`). `Run_WorldUpdateAndRender` starts the update task, renders on the calling thread, then waits (`GameApplication.cpp:610-632`). Rendering uses frame N-1 data. Extraction is a task per view (`View.cpp:41`). Command recording is single-threaded: only `m_pImmediateContext` exists in the DX11 device (`DeviceDX11.cpp:129-168`); no `CreateDeferredContext` in RendererDX11.
- P3: Update phases are PreAsync, Async, PostAsync (`WorldModule.h:16-18`). `UpdateAsynchronous` splits Async functions into batches and waits (`World.cpp:1159-1205`). Async functions may not declare dependencies (`World.cpp:1116`).
- P4 physics: `StartSimulation` runs in PreAsync; `FetchResults` blocks in PostAsync (`JoltWorldModule.cpp:359-370,778,786-787`).
- P5 Jolt: stepping modes Variable, Fixed, SemiFixed with a max-substep cap (`JoltWorldModule.cpp:848-901`). Equal-dt steps are batched into one `Update` call (`:906-937`). Write-back is a serial loop over `GetActiveActors()` with a `BodyLockRead` and `SetGlobalTransform` each (`JoltDynamicActorComponent.cpp:28-57`); it then feeds the serial transform update.
- P6: render data is sorted and batched; same mesh and material are instanced (`MeshRenderer.cpp:32-96`). `InstancedMeshComponent` exists. `DrawIndexedInstancedIndirect` and `DispatchIndirect` exist in the API (`CommandEncoder.h:209,234`); whether the main scene path uses them was not checked.
- P7: async prioritised resource loading (`ResourceManager.h:173-194`); selectable highest mip (`Texture2DResource.cpp:63-74`); LOD mesh component exists; no large-world double precision found (grep only).
- P8: permutations are separate binaries; runtime compile is off by default (`ShaderManager.cpp:10,179,208`). Whether the cache is fully pre-generated and whether D3D11 pipeline-state creation hitches: not checked.
- P9: `AntialiasingPass`, `MsaaResolvePass`, `UpscalePass`, `HistorySource/Target`; `Antialiasing.ezShader:94` has `#error Undefined MSAA Sample count`; no TAA, FSR or DLSS found. `App.RenderScale` cvar exists (`GameApplication.cpp:49`).
- P10: CPU raster occlusion at half resolution (`RenderPipeline.cpp:756-800`), needs x86 AVX1 and FMA3 (`:759-763`), occluders must be authored (`ezOccluderComponent`).
- P11: `EZ_PROFILE_SCOPE`, Tracy in all configs except Release (`Foundation/CMakeLists.txt:80-82`), Inspector and RenderDoc plugins.

**O3DE**
- P1: the mesh feature processor runs update work as jobs (`MeshFeatureProcessor.cpp:188-219,239-261,344-370`). `RegisterOrUpdateCullable` inserts into the octree under an exclusive `shared_mutex` lock (`Culling.cpp:127-135`, `OctreeSystemComponent.cpp:403-405`), so parallel jobs serialise there (read from code, not profiled). Culling is parallel (`Culling.cpp:683-826`, `r_numEntriesPerCullingJob`=750, `r_numNodesPerCullingJob`=25).
- P2: `Scene::Simulate` is async and waits only for the previous frame (`Scene.cpp:484-536`). Contexts record in parallel jobs (`FrameScheduler.cpp:589-629`). Vulkan executer defaults to `JobPolicy::Parallel` with secondary command lists (`Gems/Atom/RHI/Vulkan/Code/Source/RHI/FrameGraphExecuter.cpp:30-39,83-84,183-184`). No dedicated render thread; waits at `Scene.cpp:118,499,671`.
- P3: not verified; TickBus not opened. Only the renderer and PhysX use jobs.
- P4: `Scene::Simulate` blocks on the previous simulation (`Scene.cpp:497-504`). Physics events are queued and flushed after `fetchResults` (`PhysXScene.cpp:652-669,1183-1207`); queries take scene locks.
- P5: PhysX tasks via an O3DE job-based CPU dispatcher (`PhysXCpuDispatcher.cpp:19-27`). `simulate` at `PhysXScene.cpp:577`, `fetchResults` at `:608`. Write-back is serial unless `physx_batchTransformSync` (default **false**, `PhysXSystem.cpp:34`). Fixed timestep exists (`SystemConfiguration.cpp:34-35`).
- P6: automatic CPU mesh instancing via instance groups (`MeshFeatureProcessor.cpp:53-79,127-148,239-261`). Meshlets gem is a proof of concept. No GPU-driven indirect path found (not a full search).
- P7: `StreamingImageController` mip streaming with LRU (`StreamingImageController.cpp:85-172`). Mesh streaming and large worlds not checked.
- P8: precompiled shader variants, async loader thread (`ShaderVariantAsyncLoader.cpp:64-101`), fallback to root variant while loading (`Shader.cpp:430-449`). PSO caching not checked.
- P9: TAA built in (`Taa.pass`, `TaaPass.cpp`); no FSR or DLSS found (filename search only).
- P10: vendored Masked Occlusion Culling, enabled on Linux and Windows (`Atom_RPI_Traits_Linux.h:10`, `Atom_RPI_Traits_Windows.h:10`), only with user-placed `OcclusionCullingPlane` entities (`Culling.cpp:623-667`). No Hi-Z pass found.
- P11: AzCore profiler, Gems/Profiler with ImGui CPU profiler, RenderDoc and PIX integration.
- Other CPU lines from the earlier O3DE CPU report: `Launcher.cpp:108,117`; `PhysXSystem.cpp:34,191-197,221-236`; `PhysXScene.cpp:46-48,600-632,1330-1352`; `JobManagerComponent.cpp:27-70`; `AzCore_Traits_Linux.h:115`; `TaskGraphSystemComponent.cpp:27-31`; `TransformComponent.cpp:170,717,756-768`; `RigidBodyComponent.cpp:268-300`.

**Stride**
- P1: `TransformProcessor.Draw` runs `Dispatcher.ForBatched(..., UpdateTransformationsRecursive)` over all roots (`TransformProcessor.cs:96-131`); no dirty list seen. Bounds update parallel (`ModelRenderProcessor.cs:105,174`). Culling is a parallel flat frustum scan, O(N) (`VisibilityGroup.cs:133,138,189-191`).
- P2: `GameBase.Tick` runs Update, Draw, Present serially (`GameBase.cs:660-703`). Inside the frame: extract, prepare and draw are parallel (`RenderSystem.cs:203-215,273,395-447`), one command list per batch, executed with `ExecuteCommandLists` (`:581`). Whether the backends benefit was not checked.
- P3: scripts run on a cooperative scheduler on the main thread (`ScriptSystem.cs:63-142`).
- P4: no render thread. Bepu update in `PhysicsGameSystem.cs:27-35`; static-pose push ordering relies on `TransformProcessor` (`CollidableProcessor.cs:51`). Queries run on the main thread.
- P5: fixed step default 1/60 s (`BepuSimulation.cs:40`), `MaxStepPerFrame` (`:734`). `Simulation.Timestep(dt, _threadDispatcher)` with a wrapper sized by `Dispatcher.MaxDegreeOfParallelism` (`:301-304,756`); `#warning` at `:303` says Bepu's own dispatcher is not yet replaced. Write-back parallel when `ParallelUpdate` (default true, `:126`), active bodies only (`:785-796`). Contact events flush on the main thread (`:758`). (`ParallelUpdate` parallelises entity sync only; the solver parallelism comes from the dispatcher.)
- P6: `InstancingComponent` is manual only (`InstancingRenderFeature.cs:75-100`); no auto-batching or indirect draw found (not a full-tree grep).
- P7: `StreamingManager` streams textures, 512 MB default (`StreamingManager.cs:29,63`). No mesh LOD or mesh streaming found. Large worlds not checked.
- P8: async effect compile (`EffectSystem.cs:137-163`); bytecode cache (`EffectCompilerCache.cs:24-42`). Full precompilation of a shipped game unproven.
- P9: FXAA and TAA; no upscaler found (filename list only).
- P10: no occlusion culling found.
- P11: `Profiler.Begin`, in-game profiling overlay, RenderDoc plugin.

**Bevy**
- Default features include `multi_threaded` (`Cargo.toml:139,188-196`).
- P1: transform propagation parallel (`crates/bevy_transform/src/systems.rs:482-491`, "2-5 times faster"); visibility parallel (`crates/bevy_camera/src/visibility/mod.rs:748-786`); mesh extraction parallel and change-driven (`crates/bevy_pbr/src/render/mesh.rs:1952-1977,2075,2494-2580`). Whether many moving entities bottleneck in practice: not read.
- P2: `PipelinedRenderingPlugin` in `DefaultPlugins` (`default_plugins.rs:55-57`), thread "Render thread" (`pipelined_rendering.rs:181-183`), frame N render alongside frame N+1 simulation (`:99-136`). Command buffers finished in parallel (`render_context.rs:114-125`).
- P3: `MultiThreadedExecutor` default (`executor/mod.rs:44-55`).
- P4: extract runs on the main thread and blocks it (`extract_plugin.rs:136-146`); the main thread waits for the render app (`pipelined_rendering.rs:58-64`).
- P6: GPU instance-buffer building on by default (`crates/bevy_pbr/src/lib.rs:148`); `GpuPreprocessingMode::Culling` "used by default" (`gpu_preprocessing.rs:162-165`); mode selection `:1545-1602`; `multi_draw_indirect_count` used when supported. Meshlets are opt-in, need `TEXTURE_INT64_ATOMIC`, Vulkan and Metal only (`meshlet/mod.rs:94-96`).
- P7: `Transform.translation` is `Vec3` f32 (`transform.rs:92`), `GlobalTransform` wraps `Affine3A`. No texture or mesh streaming found (filename search).
- P8: async pipeline compile (`pipeline_cache.rs:50-54`); `synchronous_pipeline_compilation` default false (`crates/bevy_render/src/lib.rs:158-165`); items skip until ready (`crates/bevy_render/src/render_phase/mod.rs:2270-2277`). No warm-up or disk pipeline cache found.
- P9: FXAA, SMAA, TAA, CAS, DLSS (`crates/bevy_anti_alias/src/lib.rs:25-33`); DLSS NVIDIA only. No FSR found (grep).
- P10: two-phase GPU occlusion culling, opt-in, needs `DepthPrepass`, not deferred, needs `Culling` mode (12 storage textures, 10 storage buffers, `gpu_preprocessing.rs:1566-1574`).
- P11: `trace_tracy`, `trace_tracy_memory` (`Cargo.toml:459-462`), Tracy GPU timing.

**BRUTAL / SDL3 / raylib** (none solves moving objects, render threading, large worlds or AA/occlusion for you)
- BRUTAL: scheduler with N runners on the system ThreadPool, no parallel-for or dependency graph in the part read (`JobScheduler.cs:6-12`). Jolt thread count configurable (`JobSystemThreadPool.gen.cs:51,144`); custom job system possible (`JobSystem.gen.cs:52-57`). Batch pose read `BodyLockInterface.GetBodyStates` (`BodyLockInterface.gen.cs:160`) into 64-byte structs (`BodyState.cs:20-30`). Position type looks single precision (`Body.gen.cs:1114`). Vulkan 1.3 requirement at `README.md:87`; whether the Deck driver meets it: not checked.
- SDL3: command buffers are per thread (`SDL_gpu.h:51-53,541-542,3503-3507`); indirect draws `:3971,3993`; compute `:2006-2008`; MSAA `:950-956`; no temporal AA, upscaler or occlusion culling found.
- raylib: `DrawMeshInstanced` takes a CPU matrix array per call (`raylib.h:1633`, `rlgl.h:754`); no indirect draw found; MSAA hint only (`raylib.h:570`). Single GL context is inferred from design, not from a code statement.

### 6.2 Open-item results (closed 2026-11-06)

| Open item | Result | Evidence |
|---|---|---|
| **Godot Jolt-to-node write-back** | Serial, on the main thread. `Main::iteration` calls `flush_queries` (`main.cpp:5031`), which loops over moved bodies (`jolt_space_3d.cpp:221-233`) and calls back into `RigidBody3D::_body_state_changed` (callback set at `rigid_body_3d.cpp:835`), which syncs state and sets the node transform (`:171`). Bodies created directly on `PhysicsServer3D` without a callback skip this loop (RID workaround is backed by code). | Proved, source |
| **ezEngine Async-phase users** | 11 built-in component types use the Async phase: animation controller, simple animation, AI sensors, Detour crowd agents, cloth, fake rope, Jolt breakable slabs, Kraut trees, procedural placement, FMOD events, volume samplers. Script components run in PreAsync (serial; `ScriptWorldModule.cpp:25`). | Proved, source |
| **Avian internals** | Parallel via `par_for_each` over `ComputeTaskPool` (`utils.rs:33-141`), gated by the `parallel` feature and minimum length: broad phase BVH (`bvh_broad_phase.rs:71`), narrow phase in chunks of 64 (`system_param.rs:445`), solver parallel within each graph colour (`plugin.rs:688,791,887,992`), integrator, CCD, solver-body sync, collider-tree update, `transform_to_position`. **Serial**: `position_to_transform` write-back (`physics_transform/mod.rs:384-409`, changed bodies only), `trigger_collision_events` (exclusive system), and the XPBD joint solver (grep only: no `par_`/`for_each` in `solver/xpbd/*.rs`; not read in full). Default `FixedPostUpdate` schedule; `SubstepCount` default 6 (`schedule.rs:191-195`); timestep from `Time<Fixed>`, Bevy default 64 Hz (`bevy_time/src/fixed.rs:69,76`), set to 60 for the Deck. Joints: fixed, distance, prismatic, revolute, spherical, XPBD (`xpbd_joints` feature); no articulations, no ragdoll builder (doc line only, `spherical.rs:16`). Swept CCD on by default for fast bodies; speculative CCD opt-in. `f64` and `enhanced-determinism` features exist (off by default); cross-platform hash test is 2D only. Version 0.8.0 with a `0.8-to-main` migration guide. 93 `#[test]` occurrences in `src/`; benches exist. Hypothesis: solver scaling on 4c/8t depends on how well the scene colours; not measured. | Proved by reading; parts flagged |
| **RADV features** (for Bevy GPU culling and meshlets) | In Mesa main `src/amd/vulkan/radv_physical_device.c`: `multiDrawIndirect`, `drawIndirectFirstInstance` (`:1022-1023`), `drawIndirectCount` (`:1081`), `KHR_draw_indirect_count` (`:719`), `shaderImageInt64Atomics` (`:1254`), `EXT_shader_image_atomic_int64` (`:937`) are enabled unconditionally; push constants 256 bytes (`radv_constants.h:26`); per-stage storage image and buffer limits in the millions (`:1855-1857`). **Inference**: Bevy should pick `Culling` mode and meshlets should be possible on the Deck. **Not verified**: SteamOS ships a different Mesa version; how wgpu maps these features; nothing run on a Deck. The vulkan.gpuinfo.org reports (device "AMD Custom GPU 0405 (RADV VANGOGH)") could not be read (JavaScript tables; the agent stopped at a bot check and did not work around it). | Partly proved (Mesa main), partly inference |

---

## 7. Porting Godot / O3DE render features to ezEngine

Source read only; nothing built. Sizes are measured from source; effort ratings are judgment.

**Missing in ezEngine and needed first (searched, none found):**
- **P1**: camera jitter, previous-frame matrices and a motion-vector pass. Touches render-data extraction and every material vertex shader.
- **P2**: a normal/roughness buffer (ezEngine is forward-only).
- **P3**: a Hi-Z depth pyramid.

**What ezEngine already has:** compute and indirect dispatch, UAVs, atomics, 3D textures, render-graph compute passes, HistorySource/HistoryTarget passes. Evidence: `CommandEncoder.h:207,209,234,240`; `Enumerations.h:41-47,51-52`; `RenderGraphBuildHistogram.ezShader:29`; `RenderGraphBuildMinMax.ezShader:35`; `RenderPipelinePass.h:83`; `ReflectionFilterPass.cpp:120-167`; `HistorySourcePass.h:6-8,54`.

**What it lacks:** ray tracing, mesh shaders, bindless, multi-draw indirect. Vulkan apiVersion 1.1 (`DeviceVulkan.cpp:415`), DX11 SM5.0. Inverse depth is used (`GraphicsUtils.h:8-17`, `Camera.h:123,189`, `GraphicsUtils.cpp:421-427`). GAL notes: `RendererFoundationDLL.h:147-156,178-187`; `ShaderCompilerHLSL.cpp:389-399`; `ShaderCompilerDXC.cpp:186-201,287-289`. MSAA-only AA: `AntialiasingPass.h:8`, `.cpp:49-66`.

| Feature | Best source | Size of source | Needs | Effort | Deck value |
|---|---|---|---|---|---|
| TAA | O3DE `Taa.azsl` (HLSL-like) | about 280 lines (Godot: 190 C++ + 385 GLSL) | P1 | **Small** once P1 exists | High |
| FSR2/3 upscaling | AMD FidelityFX SDK (HLSL) instead of Godot's 1076-line backend (SDK: 3064 C++/headers, 17208 shader lines; `fsr2.cpp:171-458,812-819`) | SDK | P1 | **Medium**; DX11 SM5 path is a risk | **Highest** |
| SSR | Godot (663 GLSL) or FidelityFX SSSR | medium | P2, P3 | **Medium** | Medium; cheap mode only |
| SSAO + SSIL | Godot (2132 GLSL, Intel ASSAO, MIT) | medium | P2 | **Medium**; ezEngine already has SSAO | Low-medium |
| Volumetric fog | Godot (1192 GLSL + 1608 C++) | large | rewire to ezEngine light and shadow data | **Large** | Low; GPU cost |
| VoxelGI | Godot (1074 GLSL) | large | needs a baker | **Large** | Low |
| SDFGI | Godot (2686 GLSL) | very large | renders the scene into 3D textures | **XL** | Too heavy for 8 CUs (hypothesis) |
| Ray tracing / hybrid SSR | O3DE (DX12 13 files, Vulkan 17) | very large | GAL ray tracing, Vulkan 1.2+, SM6.3+; impossible on DX11 | **XL** | Not realistic at 60 fps (hypothesis) |
| DDGI probe GI | O3DE `DiffuseProbeGrid` | n/a | n/a | **Avoid**: NVIDIA proprietary licence (`DiffuseProbeGridFeatureProcessor.cpp:25-26`, `DiffuseProbeGrid.cpp:201`, "Nvidia RTX-GI License.txt") | n/a |
| Meshlets | O3DE (gem v0.0.0, proof of concept) | n/a | n/a | Not worth it | none |

**Licence warnings:** O3DE DiffuseProbeGrid is NVIDIA-licensed: do not copy. Godot's SSAO/SSIL is Intel ASSAO (MIT per the investigation). The FidelityFX licence file was not read; check it before copying SDK code.

**Verdict:** P1 + TAA + FSR is a realistic project and would cover what matters most on the Deck. Everything past SSR is a renderer project of its own. Suggested model split for such work: Sonnet-class agents for shader ports, Opus-class for motion-vector and render-graph integration.

---

## 8. Community-reported problems per engine

Many statuses are snippet-level. None was measured on a Deck. Web research covered GitHub pages, releases, issues, the O3DE FAQ and the Godot Jolt docs page directly; the rest came from search summaries. Not checked: O3DE PhysX gem issues, Stride GC and WPF editor complaints, Bepu-on-Linux performance, any Deck specifics, ezEngine Jolt bugs.

| Engine | Problem | Reference | How solid |
|---|---|---|---|
| **Godot + Jolt** | Overlapping Area3Ds cost CPU even with monitoring off; runtime shape changes took over 30 s versus near-instant on GodotPhysics | godot #106482, #118047, #105642 | snippets |
| | Some joint properties unsupported (only warn); docs call Jolt thread safety "experimental" | Godot Jolt docs page | fetched |
| | SoftBody bugs: wrong vertex normals; Linear Softness has no effect | #107831, #123569 | snippets |
| | First-use shader stutter on Vulkan; ubershaders (4.4) help but do not cover everything | 4.4 release notes, #90400 | fetched / snippet |
| | 2023 critique: C# raycasts cost about 50x the raw engine call; what was fixed since not verified | Sam Pruden article; HN item 37598985 | fetched |
| | One shipped 3D game reported editor crashes 2-3 times a day; built game did not crash | Pantopie postmortem | fetched |
| **ezEngine** | Full engine Windows/D3D11 only; Vulkan port ongoing; Linux editor not usable (`README.md:15`) | README | checked locally |
| | Release 26.9 (2026-08-20) added a render graph that inserts Vulkan barriers, a step toward full Vulkan switch; 25.3 added SteamRT Sniper cross-compile | Releases page | fetched |
| | About 72 contributors, about 2k stars, about 265 forks; no shipped commercial game found (only a sample game) | Open Hub, GitHub | fetched; small-team risk is inference |
| | Jolt migration finished Oct 2022; only Jolt issue found was #630 (allocator hook request, not a bug) | JoltPhysics discussion #292 | fetched |
| **O3DE** | Foundation says limited funds as membership declined; active contributors fell from 797 to 531 a year | o3de.org FAQ, LFX Insights | fetched |
| | Asset Processor crashes or hangs; one commenter reports 12 minutes for a minimal project | o3de #18920, #17857 | fetched |
| | Linux SDK install of about 100 GB reported | #2677 | user report |
| **Stride** | Linux runtime crashes still reported; reopened May 2026; TopDownRPG crashes on click | stride #2496 | fetched |
| | Navigation does not support Bepu yet; moving static Bepu objects caused a hang (fix not checked) | #3778, #2648 | fetched |
| | Building needs Visual Studio 2026; editor Windows-only | README | fetched |
| **Bevy** | Breaking release about every 3 months | README (via search) | snippet |
| | `3d_scene` frame time rose from 2.3 ms to 16.7 ms on an integrated AMD GPU with RADV; no maintainer answer | bevy Discussion #23341 | one laptop, unverified |
| | Occlusion-culling example freezes on an RX 9070 (RADV GFX1201, Mesa 25.3.5); not a Deck; not read | bevy #23108 | snippet |
| | Avian struggles past about 35k colliders; Rapier non-deterministic even with the determinism flag | avian #1081, bevy_rapier #79 | fetched |
| | No Bevy/wgpu/RADV report specific to the Deck or Van Gogh found (searches not exhaustive) | - | - |
| **BRUTAL** | Weeks old; binding bugs (Box2D, Vulkan type mapping); build scripts "coming" | brutal #16, #28, README | fetched |
| | Logo-before-gameplay licence disputed, not OSI-approved | #14 (18 comments, open) | fetched |
| **SDL3 GPU** | No bindless, mesh shaders or ray tracing | HN thread | official docs not checked |
| | Validation and crash bugs in SDL_shadercross and Vulkan | SDL #12085, #12192 | fetched |
| **raylib** | Animation per model on the CPU; skinned meshes hard to instance | raylib Discussions #4606, #5675 | fetched |
| **The Forge** | Project health: 17 months without a release (1.63 2025-03-20 to 1.64 2026-08-12). The maintainer answered "Is the project dead?" and "why no updates anymore?" on 2026-06-13: "We are moving away from GitHub ... We are going to host on a different platform and announce it." No reason was given. | GitHub #337, #339 | fetched (main session) |
| | Public repo moved to Codeberg with a fresh history. GitHub has had no code changes since 1.63; 1.64 removed the Vulkan, Linux and Mac code from the public repo. Codeberg issue #4 "Vulkan and Metal graphics backends are missing ??" (2026-10-14) has no reply. No vendor reason for the removal was found beyond the README line "only holds the PC / DirectX 12 runtime". | both repos (section 3.5); Codeberg #4 | checked locally; fetched (main session) |
| | Small new home: Codeberg 7 stars, 3 forks, 1 open issue. GitHub: 5,809 stars, 593 forks, 15 open issues (API, 2026-11-06). | GitHub and Codeberg APIs | fetched (main session) |
| | D3D12 sample refuses an AMD RX 570: "GPU Unsupported. Application does not run on this device." With the check removed the scene ran (about 8-10 ms; the program then picked the integrated GPU, so which GPU ran it is unclear). Maintainer guessed missing DX12 features. Still open. | GitHub #327 (2025-04) | fetched (main session) |
| | Newer D3D12 reports unanswered: fence-signal exception in `15a_VisibilityBufferOIT` on NVIDIA (#333, 2025-09); exception at `CreateDXGIFactory2` with the debug layer, release 1.60 (#319) | #333, #319 | fetched (agent) |
| | Older D3D12 bugs were fixed only in the next release: RTV descriptor leak when removing array/3D render targets (#293, reporter supplied a PR; maintainer said "Fixed with latest release" 5 weeks later); HDAO-off regression 1.50 to 1.53 (#280) | #293, #280 | fetched (agent) |
| | Setup friction: samples assert or miss files when the art download (`PRE_BUILD`) was not run or was incomplete (#313 macOS, #317 SteamOS; same theme in #284, #258, #203 by title); build instructions and wiki called unclear (#331, #326, #153) | listed issues | #313, #317, #331 fetched (agent); others titles only |
| | FSL quirks: own preprocessor, `#extension` needs `@fsl_extension` (answered by a user, not the vendor); more FSL and reflection issues by title (#320, #271, #267, #234, #189) | #315 | fetched (agent); others titles only |
| | Maintainer response: build and asset questions answered within hours to days; bug reports usually wait for the next release; several 2024-2025 reports (#309, #319, #333) have no vendor reply | sample of about 20 issues | fetched (agent); sample, not all 173 issues |
| | Vulkan/Linux history (not relevant to a Windows-only game): RADV crash in 2019, vendor first refused Mesa support, then tested on it (#117); Linux build and loader breakages 2024-2026 (#309, #336) | #117, #309, #336 | fetched (agent) |
| | **No report of The Forge under Proton or vkd3d-proton was found** (web and issue search). Its own Deck support was the native Vulkan build, which the D3D12 path under Proton does not use | searches | absence; searches not exhaustive |

---

## 9. Talks

All read from YouTube auto-captions unless noted. Speaker names and version numbers in captions are unverified. The caption-reading tool was an Invidious-based `youtube-transcript` skill after a first agent could read only titles.

### 9.1 Godot render talks (Clay John, rendering lead)

| Talk | Event / date | Link or ID | Read? |
|---|---|---|---|
| The Future of Rendering in Godot (keynote, 53 min) | GodotCon 2023 Munich | media.ccc.de `godotcon2023-57791-the-future-of-rendering-in-godot` | abstract only |
| Godot Rendering Update (50 min) | GodotCon 2024, 2024-10-13 | `6ak1pmQXJbg`; talks.godotengine.org `godotcon-24/talk/YX78Q8` | captions read per the investigation; details not separately recorded |
| How Godot and Arm Are Enabling Better Mobile Gaming | GDC 2025 | `Ylk1G45hsbo` | captions read; short (4.4 mobile CPU/GPU gains, Arm profiler); nothing on desktop or Deck |
| Godot Rendering Team Update | GodotCon Boston 2026, Clay John (verified from the YouTube description, SceneTree channel, published 2026-08-03) | `AX3lWk-P0UA` | covers last two years of changes, limits and a preview with "new high-end features" |
| Where Every Millisecond Counts: Optimizing Godot for Low-Spec Devices (with W4 Games, Pablo Selener, David Snopek) | GodotCon Boston 2026 | `d8HF2WPcHqU` | captions read |
| From Alpha to Z-Test: a trip through Godot's renderer | GodotFest 2026 | `6qumdUbUrKo`; godotfest.com/talks/from-alpha-to-z-test | captions read |
| Arm mobile talk (From Desktop Dreams to Mobile Reality) | GodotCon 2026 | `MScP3F0S3NQ` | captions read |

**Key points (proved from captions; Deck relevance is judgment):**
- The renderer assumes the loaded level fits in VRAM; "complex or massive, not both".
- The opaque pass and depth pre-pass are usually the bottleneck. Upscalers will come as hooks only because of SDK licences.
- *Alpha to Z-Test*: every moved object has its bounds recomputed, is re-inserted into the scene's spatial tree and re-matched with lights; "can be really, really slow" with thousands of moving objects and many moving lights. Design assumes few objects that move a lot and many that never move. Source check: `update_dirty_instances()` is a plain serial loop (`renderer_scene_cull.cpp:4385`).
- Separate render thread overlaps the next frame's physics and logic with rendering ("an extra 50% frame budget"); off by default; "there are bugs lurking"; planned default-on in 4.9 dev 1; reading data back from the RenderingServer stalls it.
- Shadows: a directional light re-culls the scene per cascade (4); point and spot shadows re-render when the light or a touched object moves.
- Culling: occlusion on the CPU (Embree); frustum culling multi-threaded above 1000 instances (matches source).
- Forward+ has a depth pre-pass and FSR/TAA; Mobile has no depth pre-pass, keeps per-mesh light lists, bilinear upscaling only.
- *Every Millisecond Counts*: measured on Meta Quest 3 and Galaxy XR, not desktop or Deck. Specialization constants saved about 6 ms per frame; baking a texture-heavy material about 6 ms; dropping dynamic skies about 0.5 ms or more; multi-threaded rendering "almost always" helped Vulkan XR apps but gives nothing when render-bound; Godot 4.6+ has a tracing profiler (Perfetto).

**Official written sources (content read by an agent; main session did not re-check):**
- Rendering Priorities, January 2024: SDFGI refactor to HDDA, to be renamed "Dynamic GI"; FSR 2.2 and TAA improvements merged for 4.2.
- Rendering Priorities, September 2024: pipeline compilation and RenderingDevice multithreading (GH-90400) merged in 4.4 ("we fallback on the ubershader and compile the specialized shader in the background"); HDDAGI (GH-86267) in progress, "Both performance and quality are better than SDFGI"; D3D12 and Metal drivers, acyclic render graph and Compositor effects done; "Performance is our number one priority"; screen-space effects move to temporal reprojection, team "won't require TAA"; "none of us are working on the renderer full-time"; Compatibility renderer is "feature complete".
- Priorities page (undated, 2007-2026): overhaul SDFGI; expose a ray-tracing API (RenderingDevice first, then Forward+); improve scene multithreading; core optimisations. No upscaling or mobile item.
- Ubershaders: 4.4 dev 4 notes and 4.4 release page; PR #102125 makes ubershader compilation high priority (reduces, does not eliminate, stutter); 4.5 adds a shader baker (search summary only).
- "Optimizing CPU-side Rendering Code" (2026-09-15): 2D and load-time case studies only; no Godot version named; no Deck.
- Unverified secondary: HDDAGI PR #86267 by reduz, PR #119869 revives it, "no ETA" (May 2026, search summary); a third-party blog claims 24-33% Deck gains from 4.5 to 4.6 (method unstated, duplicate table row, **discounted**); 4.6 made D3D12 default on Windows and rewrote SSR, 4.7 (2026-06-18) added HDR output, AreaLight3D (search summaries).

**Outlook (proved: from posts; suspected: inference).** Proved: performance is stated top priority but team is small; Forward+/Mobile stay the main paths; ubershaders shipped; SDFGI overhaul has no ETA; ray-tracing API is a listed priority. Suspected: HDDAGI may not land in the next 1-2 releases; ray tracing is irrelevant for a Deck game (no hardware RT; inference); Mobile/Compatibility with baked lighting is the safe choice; CPU cost is the likelier limit than GPU.

### 9.2 Godot CPU talks

| Talk | Speaker, event | ID | What matters |
|---|---|---|---|
| Squeezing the CPU: Multi-Threading in Godot | Pedro J. Estebanez, GodotCon 2024 | `k0yrP5Av0fg` | WorkerThreadPool has one thread per logical core (8 on the Deck). Default server is "single-safe": calls queue and flush at end of frame; **any server call returning a value forces an early flush**. Separate render thread pumps its queue on the pool. Buffering physics-server commands is only an idea for a future PR. Waiting tasks can deadlock; fibers would fix it but are not implemented. No measurements. |
| Multi-threaded Scene Processing in Godot 4.1+ | Pedro, Godot Sprint 2023 | `WuH3TiVnuaw` | Thread groups run each group on its own thread. Nodes in one group cannot call nodes in another; resources and scripts "not protected". "Heavily under construction... experimental". |
| 1000 rigged AnimationPlayers in 3D, impossible? | Bertrand Bordage, GodotCon 2026 | `IRqR3lpAuOI` | 700 static characters 3 ms CPU; 700 skinned and animated characters 7 ms, about 10 fps. Fix: animations baked to textures plus MultiMesh, about 3000 characters. Works only for looping baked animations. |
| The long road to a million trees | Bart Wilmink, GodotCon 2026 | `DjNHFXtF8rs` | Tens of millions of nodes "not feasible". A worker thread computes which objects changed visibility; the main thread only applies the change to a pool of reused nodes, under 1 ms. Built-in profiler hides worker-thread time. |
| (LOD talk) | Wilmink | `etvs8CynGqg` | **Not read** |
| (history talk) | Juan Linietsky / Ariel | `hKLl03A9Kws` | **Not read** |
| GDScript: Past, Present, and Future | George Marques, GodotCon 2023 | `WNR4Tne5DRE` | Typed GDScript skips runtime type checks; "two times faster, some... five times" (hard to quantify). AOT/JIT only researched. C# pays marshalling cost. |
| C++ for Godot with GDExtension | David Snopek, GodotCon 2024 | `4R0uoBJ5XSk` | GDExtension for "native performance for some select parts"; godot-cpp mirrors the engine API; no overhead numbers. |
| Godot at the limit of grip! | Marcel Offermans, GodotCon 2026 | `IB6dzaGZu5E` | Racing sim replaced Godot physics with its own isolated physics module (Project Chrono based; how it binds to Godot is not stated, see 9.4): 1000 Hz, double precision, own thread; rendering reads latest state. Standard single-precision Godot build; track moved to the origin instead. |
| From Desktop Dreams to Mobile Reality | Arm, GodotCon 2026 | `MScP3F0S3NQ` (listed in the memory as "Arm mobile talk") | Mostly mobile GPU advice; watch `Performance` monitors; build a benchmark mode. |
| Architecting Jolt Physics for Horizon Forbidden West | Jorrit Rouwe, GDC 2022 | n/a | **Not read**. A search snippet says Jolt doubled simulation rate in that game with less CPU. Not Godot. |

Result of the search: **no official talk** covers Jolt in Godot, physics-server cost, scaling to many bodies, or C#/GDExtension call overhead (the agent could not search YouTube directly, so this absence is not fully proved). No talk found from Lukas Tenbrink or a core engineer at GDC.

### 9.3 Juan Linietsky, GodotCon 2026 (`LDSwP37y_W4`)

"Help make Godot better by thinking like a Godot Developer". Whole caption transcript read. Not a performance talk.
- Godot gets more pull requests and proposals than it can review. Proposed fix: score each from 0 to 10 with four factors (complexity, reach, impediment, stakeholder); geometric mean times 10; about 5 anonymous trusted reviewers vote; a bot publishes. He calls it only a suggestion.
- High bar: "the bigger the software, the more unstable it is"; editor zip grew 9 MB to 73 MB; about 2 million lines; GDScript about 8k to 50k lines. Merge test: "this is useful right now to somebody". Visual scripting was built, barely used, removed in Godot 4.
- Q&A on performance: "if scene is too slow, you can use the servers. If the feature is too difficult to add, then you can use an extension."
- Large studios: "no specifically technical limitation"; the barrier is perceived risk and switching cost.
- AI: "absolutely fantastic for code review". His team does "pretty much zero" vibe coding on high-performance code, because engine code needs "a human to judge". The Godot Foundation will publish an AI code policy "at some point".
- Inference (not tested): do not count on upstream merges; keep Quest engine needs in your own GDExtension or a small fork. AI writes most gameplay code; physics and render hot paths still need a person to profile on the Deck.


### 9.4 Godot at the limit of grip! (Marcel Offermans, GodotCon 2026, IB6dzaGZu5E)

Source: auto-captions (about 5,100 words, roughly a 30 minute talk plus Q&A). Everything under "stated in talk" comes from the transcript. The talk is high level: it does NOT cover tyre model details, GDExtension, track/world data formats, or VR (not mentioned; absence is scoped to this transcript). Caption note: "threats" = threads; "the 8" in a Q&A answer is unclear (caption unclear).

#### Speaker and company (stated in talk)
- Marcel Offermans. Started programming on Commodore 64 and Amiga; studied mechanical engineering in Delft, then moved to software; sim racer; involved in the Apache Software Foundation (open source).
- About 10 years ago he took over development of rFactor 2 and "added an order of magnitude to the amount of users" over 6 years. Also helped on Automobilista, worked with Amazon Games on The Grand Tour Game ("wasn't a big success"), and on a NASCAR game with Motorsport Games. The studio was sold about 4 years ago.
- Founded The Last Garage; builds "a racing simulation engine that could be used for games and other applications using Godot and a few other things". Has worked on it for 3 years.

#### Architecture (stated in talk)
- Uses the "4+1 view" model. Use case: one architecturally relevant case, a driver driving a car. Deployment: single PC, or plus server/matchmaking/achievements for multiplayer.
- Modules: graphics and sound; physics simulation (isolated: car physics, collisions); multiplayer (client/server, racing "session management": qualifying, race, rally stage, training); controllers and force feedback; one module per vehicle and per track.
- Controller support exists in Godot, but "force feedback is not available in Godot". He wrote his own FFB module, tightly coupled to physics, currently Windows only. Everything else is platform independent because Godot is.
- Process view: graphics runs in Godot (process functions). Physics is a loop "running in my case at 1,000 hertz in a separate thread". Because the rate is so high, no extrapolation: the renderer just grabs the current physics state. Might change if displays exceed 1,000 Hz.
- Running the physics loop "strictly real time" is a challenge. Inputs must be read at the same rate, and it is linked to multiplayer so packets get no extra latency.
- Physics is CPU only, "on a single core in fact".
- Physics engine: a bespoke engine is needed for high fidelity vehicles, but he builds on open source rather than writing all maths himself: Project Chrono (toolbox for vehicles, robots, finite element analysis, terramechanics; many simulations are not real time, e.g. the deformable terrain demo runs slower than real time). He believes he is the first to build racing cars on Chrono and added missing pieces himself. The physics is all double precision.
- Not stated in the talk: how Chrono is bound to Godot (GDExtension is not mentioned), tyre model, vehicle model internals.

#### Why Godot (stated in talk)
- Wanted forward / Forward+ rendering, not deferred: "deferred rendering engines tend to have all kinds of motion artifacts when things move quickly" (blurry effects behind cars).
- Lightweight, since he runs multiple instances for multiplayer testing on one machine.
- Community: involved since Godot 4 alphas; good feedback; open source gives freedom to use the code any way later.

#### Timeline and projects (stated in talk)
- First 1.5 years: foundation and first vertical slice (one car, one track). Shown publicly January 2024 at a sim racing event to test driving feel; positive feedback.
- Next year: 8 rigs, multiplayer competition, live YouTube broadcast; added multiplayer and broadcasting tools. Result: a reusable core for several projects.
- Motorsport in Motion (founded by a rally driver): rally simulator with driver and navigator in two rigs, run from one computer; pace notes workflow.
- Dutch army: truck driver simulator (Scania, 8 wheels, all-wheel drive, about 3,500 trucks). Training in fog, snow, rain, rough terrain, reversing. Driver behind 65-inch 4K screens, instructor station with missions; mobile on wheels. Still in progress.
- Donkervoort and Heusinkveld (pedals, wheels, shifters): digital versions of Donkervoort cars, using the sim for vehicle testing instead of only real-track prototype tests; "the jury's still out" but "very encouraging".
- MonoSim: first game, hoped for release Q4 this year; BAC Mono car, modelled with BAC; BAC has a motion-platform sim with a 180 degree curved screen; made with a company called Flatout Simulators/Simulations (caption unclear on exact name).

#### Requests to the Godot team (stated in talk)
1. Optimised rendering for sets of viewports: viewports are independent, so with 3 screens auto exposure differs per screen (one brighter); shadows could also be shared when views come from the same world/perspective.
2. Lower latency in graphics and audio.
3. Multiple sound cards: the second card drives bass shakers (vibration feedback); Godot only accesses one, so he wrote his own sound engine for the second.
4. Large world support: a 20 km road/track loses accuracy with single-precision floats far from the origin.

#### Problems and solutions (stated in talk, incl. Q&A)
- Large worlds: double-precision Godot builds exist, but he did not want to use a build "almost nobody uses" (audience: none raised hands), worried about how well tested it is. His current solution: move the track to the origin instead of moving the car 20 km. This conflicts with the Terrain3D plugin, which does not support it, causing other problems.
- Multi-screen: three screens (viewports) onto one world; exposure mismatch as above.

#### Q&A (stated in talk)
1. Q: Is the deferred artifact issue really only TAA? A: Mostly TAA and typical setups; he has seen other companies building racing sims on Unreal struggle with performance and artifacts; he is not a graphics expert, and does not claim it is impossible with deferred. His earlier in-house forward engine had none of those problems; forward has other problems (many lights at once), somewhat better in Godot.
2. Q: What specifically fails with large scenes? A: See above (double build avoided, origin shifting of the track, Terrain3D conflict).
3. Q: Is physics on CPU or GPU? A: CPU, single core. Q: Plans for GPU? A: A friend building vehicle physics has tried for over half a year to port to GPU without success; he keeps in touch.
4. Q: Motion simulators, and compensation for static ones? A: Worked with two motion rigs: BAC (about 6 degrees of freedom) and a limited-DOF one with vibration and seatbelt tensioners. Motion adds latency; if too large it does not help the driver. Substitutes for static rigs: louder tyre noise to hear the limit of grip, steering force feedback, force feedback/vibration in pedals (for traction control/ABS feel). "We'll never get perfect there."

#### Relevance for Quest

Stated in talk:
- A shipping-oriented, physics-heavy 3D sim (1,000 Hz physics) is built on Godot 4 with Forward+ rendering by a developer with prior commercial engine experience, and he is "still very happy" with the choice.
- Godot rendering and a custom physics engine run decoupled in separate threads; no interpolation needed at 1,000 Hz.
- Godot gaps he hit: force feedback, multiple audio devices, shared multi-viewport exposure, large-world precision.
- Double-precision Godot builds exist but he avoids them as little used.

Inference (not stated):
- Godot is viable for physics-heavy 3D; the decoupled fixed-rate physics thread pattern could apply to Quest if built-in physics is not enough, but the talk does not discuss Jolt at all.
- The large-world workaround (shift the world, not the actor) matters only if Quest worlds are very large (about 20 km scale).
- The talk gives no evidence about Steam Deck LCD, 60 fps, low-power GPUs, or AI agents writing code. His target is desktop PCs with high-end rigs; performance conclusions for Steam Deck cannot be drawn from it.
- Forward+ is heavier than Godot's Mobile/Compatibility renderers; for Steam Deck the renderer choice needs its own test (hypothesis, not checked).

---

## 10. BRUTAL motivation and J.E.N.O.V.A verdict

### 10.1 BRUTAL

Sources: local clone at `7a3b72d` (read: README, CONTRIBUTING, NOTICE, samples README), the forum announcement, a trade-press interview and a community wiki. Press and forum statements come from fetched summaries, not verbatim; search-snippet items are marked.

**Stated problems it targets**
- **Tool dependence.** README:20-22: "You are only as independent as your tools are. Developers have surrendered an extraordinary amount of control over their work to companies whose interests are not necessarily aligned with their own." Goal: let every developer have their own engine "without having to spend four years building the foundations first".
- **Engine bloat.** README:33 and CONTRIBUTING: "adding everything is how the industry ended up with the kind of engines we don't want to use."
- **Large-world precision and scene model** (interview summary, not verbatim): precision for spaceflight at real scale; "the camera is actually always at 0-0, it never moves, and everything is drawn into it".
- **Scale limits in Unity** (community wiki, secondary): earlier titles hit scale limits.
- **Unity licence dispute, May 2025** (search summary): Unity threatened to revoke the studio's licence; the CEO said he spent over $300k on Unity in 10 years.
- **Performance and overhead.** README:28: avoids allocations and uses unmanaged memory and unsafe code. A partner studio claims 2 to 4 times the performance of its Unity build in its own tests (README:46); that is a 2D colony simulation, a different workload, unverified.

**Design (proved from the repo unless noted)**
- C#, .NET 10 SDK (README:86). Described as JIT-compiled (interview summary); no NativeAOT/PublishAot setting found (two sample files not inspected).
- Platforms: README:89 says Windows or Linux; samples README lists Windows x64, Linux x64 and macOS Apple silicon (MoltenVK). **The two files disagree.**
- Rendering: Vulkan 1.3 requirement (README:87). Bindings: Brutal.Vulkan, Vulkan.Abstractions, Vulkan.Vma, OpenGL. Shaders: Brutal.ShaderC (runtime compile), Brutal.Slang. No renderer or scene graph.
- Libraries: custom interop layer, unmanaged memory, non-allocating collections and strings, job scheduler, maths, logging, localisation; windowing GLFW and SDL; ImGui/ImPlot, FMOD, OpenAL, RakNet, glTF, KTX; Jolt (3D) and Box2D (2D). Jolt and Slang have `Generated/` folders; the generator is not in the repo.
- Packaging: NuGet packages `Ahwoo.Brutal.*` (README:98). No csproj, solution or build scripts: "coming" (README:141). Samples are single-file `dotnet run X.cs`.
- Licence: MIT plus an attribution clause (README:149-158): credits line, logo before gameplay, ship NOTICE.txt; not OSI-approved yet.
- Users (README:37-44): RimWorld via the studio's own engine Waylith; Kitten Space Agency (free pre-alpha, README:48), Torpedia, Art of the Rail, Gruntz and Afterskies directly on BRUTAL.

**What it does not try to be.** README:33: "There is no editor, no scene graph and no renderer, and we're not adding them." CONTRIBUTING: PRs adding scene graphs, editors, high-level renderers, gameplay systems or abstractions are declined; AI-assisted PRs must be disclosed and understood; large untested AI PRs are closed. README:16: harder than Unity, Unreal or Godot; "You actually have to know what you want to do first."

**Status.** Open-sourced 2026-10-15 (forum post). Development began "in 2022 or about four years earlier" (**sources differ**). Roadmap: build files and OSI licence only. A video dated 2026-11-05 (search summary, unverified) says the owner wants to port another game to BRUTAL.

**Relevance to Quest**
- Proved: BRUTAL is bindings and low-level libraries, not an engine. You would write the renderer, scene, asset pipeline, input mapping and tools. Jolt and Vulkan bindings, job system and low-allocation C# are good building blocks, but the repo has no tests or benchmarks. Steam Deck is not mentioned. Agents have almost no training data on its API.
- Suspected: the camera-at-origin idea helps only if Quest has huge worlds; whether Slang/Vulkan plus .NET JIT hits 60 fps on the Deck is unknown.
- Judgment: motivation fits a studio with years of engine staff; not a fit for Quest.

### 10.2 J.E.N.O.V.A (Jenova Framework)

Makes C++ a scripting language in the Godot editor: `.cpp` scripts attached to nodes, hot reload, a package manager that downloads compilers and SDKs, IDE integration (Visual Studio, VS Code, CLion, Neovim). Runs as a GDExtension.

Checked locally: ships as `Jenova.Runtime.gdextension` (`compatibility_minimum = "4.1"`); own interpreter and JIT layer (`InterpreterBackend` enum: AsmJIT, TinyCC, TinyCC2, LibFFI; `Source/Jenova.hpp:467`); `_rt/ReadMe.md:57`: version 0.4.0.0 needs Godot 4.7 stable minimum; `Source/gdextension_exporter.cpp:46` (export code read handles Windows only).

Checked by an agent, not re-checked:

| Aspect | Finding |
|---|---|
| Source and licence | Runtime MIT; umbrella repo 689 stars; last push 2026-11-05 |
| Maintainers | One person, 208 commits |
| Closed parts | A.K.I.R.A JIT, code encryption, sandbox not public; RTX, Fibers, Emulator proprietary; Carbon custom "PJPL1" licence |
| Status | Every release labelled Beta; each Godot upgrade needs a matching Jenova release (4.4, 4.5, 4.6, then 4.7-only) |
| Platforms | Windows x64, Linux x64. Linux issues found are closed. A prebuilt `.so` needed GLIBC 2.38 (#12). **Export and running on the Deck are not documented.** |
| Bugs | Open crash #20 (2026-09): SIGSEGV registering a third class that uses `GDVIRTUAL`, Godot 4.6.1 |
| Performance | Self-published, on a desktop CPU, Godot 4.5 beta: maths 20x faster than GDScript; spawn/delete about equal (engine-bound). No comparison with plain godot-cpp, no physics or Deck numbers. |

Verdict (judgment): its gain is iteration speed. For hot paths in a physics-heavy game, **plain godot-cpp GDExtension is safer**: maintained by the Godot team, native code, no interpreter layer, runs on the Deck in the Windows build under Proton, and better known to AI agents. Jenova adds a single maintainer, lock-step Godot version dependence and an undocumented export path. Watch item only. A cheap test would be a small Windows export run on a Deck under Proton.

### 10.3 Combining engines and frameworks (added 2026-11-06)

**Judgment.** Combining helps only when it swaps a physics library inside an engine. The main Quest risk is per-object CPU work in the glue between physics, scene and renderer (section 1); adding a library does not remove that glue, you write it yourself.

| Combination | Solves | Does not solve | Verdict |
|---|---|---|---|
| Stride + Jolt (BRUTAL binding or JoltPhysicsSharp) | Stride has no ragdoll and no vehicle (3.2); Jolt has both, plus soft body. Both are C# on .NET 10. | No occlusion culling, manual instancing only, beta packages, serial scripts. You lose Stride's Bepu components, editor support and parallel write-back, and must rewrite them. | Only combination worth considering; a fallback, not a new rank. |
| Godot + BRUTAL | Nothing: Godot already ships Jolt at the same Jolt commit (`e77f175`). | Serial write-back and scene loop. | No |
| ezEngine + anything | It already has Jolt. | Weak point is the renderer (MSAA only, DX11 on one thread); a renderer cannot be added. | No |
| Any engine + The Forge renderer | - | Drops the engine's materials, lighting, post effects and editor viewport; The Forge is D3D12-only with the #327 gate risk. | No |
| The Forge + Jolt + SDL3 (or BRUTAL) | Renderer, physics, input, audio fit together. | No scene system, asset pipeline, editor, TAA or upscaler; agents know little of BRUTAL or FSL. | Only if Quest builds its own engine. |
| Bevy + Avian | Already a combination (rank 4). | - | Unchanged |

**Stride + BRUTAL specifics.** BRUTAL's risks remain: credit-logo licence (dispute #14), weeks-old bindings with known bugs, no tests. JoltPhysicsSharp is the alternative (hypothesis: more mature; code not read).

**Evidence (NuGet search and registration APIs, 2026-11-06).** Proved: BRUTAL ships as about 40 NuGet packages; `Ahwoo.BRUTAL.Jolt` 2026.10.4 depends only on `Jolt.Native`, `Core`, `Interop`, `Logging`, `Memory`, `Numerics`, `Strings` (no Vulkan or windowing package). `JoltPhysicsSharp` 2.23.0 has 136,643 total downloads versus 24,929 for `Ahwoo.BRUTAL.Core`. Not proved: that either binding works inside Stride; pose-sync cost; the size of the rewrite (the NuGet search did not find Stride's physics package). Nothing was built or measured.

**Cheaper test first (hypothesis).** Bepu in Stride has 35 constraint types; a ragdoll might be hand-built from them, which would make Jolt unnecessary. Not checked.

---

## 11. Practices for Quest if Godot is chosen

Combines the talks (section 9) and source reading. **None was measured on a Deck.**

1. **Use few nodes.** Drive physics props as `PhysicsServer3D` bodies (RIDs) drawn with MultiMesh, not thousands of `RigidBody3D` plus `MeshInstance3D` nodes. Reason: serial dirty-instance loop (`renderer_scene_cull.cpp:4385`) and serial per-body node callback (`jolt_space_3d.cpp:221-233`). Note: Godot already merges identical draws (`render_forward_clustered.cpp:896-900`), so MultiMesh mainly saves CPU per node, not GPU draw calls. Keep bodies asleep where possible.
2. **Never read values back from a server mid-frame.** It forces a queue flush.
3. **Test the separate render thread early** (experimental; planned default in 4.9).
4. **Avoid thread groups for now.** Use the WorkerThreadPool for batch jobs; expect 8 threads competing with Jolt on 4 physical cores.
5. **Bake animations into textures** for crowds (Bordage pattern); use a worker-thread visible-set delta for huge object counts (Wilmink pattern).
6. **Lighting:** few shadowed lights, fewer cascades, baked LightmapGI; skip SDFGI and area lights. Compare Forward+ and Mobile on the Deck itself (its GPU is desktop-style RDNA2, so mobile advice may not carry over).
7. **Typed GDScript** for gameplay; **C++ GDExtension (plain godot-cpp)** for hot paths.
8. **Profile** with the tracing profiler (Perfetto or Tracy via build option); the built-in profiler hides worker-thread time (`SConstruct:216`, `core/profiling/profiling.h:63-72`).
9. **Escape route:** a custom C++ physics thread, for example in a GDExtension (the racing sim used its own isolated physics module; the binding method is not stated in the talk) if the Godot physics loop becomes the bottleneck.
10. **Do not rely on upstream merges** for engine changes; keep needs in your own extension or small fork.
11. **Test early:** runtime shape changes, Area3D-heavy designs, ragdoll joints, SoftBody, first-use shader stutter with a cold cache.
12. **Texture streaming:** the opt-in module exists on master (default off); there is no mesh streaming.
13. **Jolt gaps to know:** unsupported items at `jolt_physics_server_3d.cpp:131,724,916,1372`; `jolt_shape_3d.cpp:90,178-185`; `jolt_body_3d.cpp:218,712-732,890-950`; `jolt_soft_body_3d.cpp:658-684,644-649,513-851`. Joint files ignore bias, softness, relaxation, damping, restitution, impulse clamp, angular ERP and force limit.
14. **Agent fit:** `--headless` (`main.cpp:570,1464`), doctest (`tests/test_main.cpp:120`), 833 class-doc XML files, text scene loader (`resource_format_text.h:149`, `register_server_types.cpp:345`). C# targets net10.0 (`GodotSharp.csproj:5`); NativeAOT only for iOS (`iOS.props:3-5`).

---

## 12. Open items and the Steam Deck benchmark plan

**Open (not proved):**
- No candidate has numbers from a real Deck. Every CPU comparison is source reading.
- SteamOS Mesa version and whether wgpu enables Bevy's GPU-culling mode on a real Deck (`vulkaninfo` on the Deck would settle the first).
- Avian XPBD joint solver serial: grep only. Bevy `Time<Fixed>` default of 64 Hz: read in the main session at `fixed.rs:76`; the Avian report itself left it unread.
- Stride/O3DE/Bevy points not checked: Stride large-world support; O3DE mesh streaming and TickBus parallelism; O3DE PSO caching; default graphics API of the Windows builds of O3DE, Stride, Bevy/wgpu and SDL_GPU (decides DXVK, vkd3d-proton or Vulkan pass-through under Proton); ezEngine Jolt query/overlap cost and D3D11 pipeline-state hitches.
- Godot: how many worker threads the Windows build gets under Proton on the Deck; Jolt GDC 2022 talk, the Jolt multicore PDF and the Wilmink LOD and Juan/Ariel history talks not read.
- Whether a vehicle module exists for Avian outside `src/` (not grepped).
- The Forge (only if a custom engine is considered): whether a 1.64 D3D12 sample starts under vkd3d-proton on the Deck or hits the "GPU Unsupported" gate (#327); most of its 173 GitHub issues, its PRs, Discord and the Codeberg wiki were not read.
- Combinations (section 10.3): whether a Jolt binding (BRUTAL or JoltPhysicsSharp) works inside Stride and what its pose sync costs; whether a ragdoll can be built from Bepu constraints; JoltPhysicsSharp code not read.
- Whether the Godot-on-Deck guidance from the mobile and XR talks carries over (no talk measured a Deck).

**Benchmark plan (do when Quest starts).** Same scene in each candidate: about 2000 rigid bodies, joint-chain ragdolls, hundreds of raycasts per frame. Measure frame time, physics step time, first-run (cold shader cache) stutter, and agent buildability. All candidates run as Windows builds under Proton (Quest ships no native Linux build). Candidates: Godot (Vulkan versus D3D12 driver; nodes versus MultiMesh + RIDs; render thread off versus on), Stride, ezEngine, Bevy + Avian (fixed step set to 60 Hz). Also record which Proton layer each one uses (winevulkan, DXVK or vkd3d-proton). Variants if Stride is competitive: Stride + Jolt binding; first check whether a ragdoll from Bepu constraints is good enough.

### 12.1 Benchmark status (2026-11-06)

What exists (PROVED by the files in `benchmark/`; see `benchmark/README.md`): a shared spec (`benchmark/SPEC.md`), a Godot implementation (C++ GDExtension, Jolt by default; PhysX fork and GodotPhysics selectable, see `benchmark/godot/PHYSX.md`) and a Stride implementation (Bepu, JIT and Native AOT), scenarios A to G, run and merge scripts, and a render check. Only Godot and Stride are implemented; ezEngine and Bevy + Avian are not in the benchmark.

Proved:
- Both engines run all scenarios in short smoke runs at the current settings (200 Hz physics, unlocked render loop, shared camera rule, game and stress profiles). Smoke runs show the code works. They are not results (godot and stride NOTES).
- Earlier full runs (5 s warm-up, 20 s measured, 3 repeats, scenarios A to D) exist only at 60 Hz physics, for Godot (GDScript and C++) and Stride JIT. They are not comparable with the 200 Hz setup and are not used for any claim here.

Open:
- The full protocol has not been run at 200 Hz, for any engine or variant. No timing verdict exists yet.
- No run on a Steam Deck under Proton; desktop numbers do not transfer to it.
- Scenario G and the PhysX fork have smoke runs only; scale and debris ladders were not completed.
- Some settings are not equal between the engines (solver, damping formulas, restitution, light units; list in `benchmark/SPEC.md`, "Settings parity"), so cross-engine ratios will carry those caveats.

Effect on this document: the ranking in section 1 and the conclusions in sections 4 to 6 come from the source-read study and stay unchanged until measured numbers exist. Nothing in the benchmark has contradicted or confirmed them yet.

---

## 13. Corrections log

Later statements supersede earlier ones. Items are in the order found.

1. **Godot physics threading** was first described as opt-in. Fix: Jolt jobs always run on the WorkerThreadPool; only the step placement is opt-in (`run_on_separate_thread`, default false).
2. **Stride `ParallelUpdate`** was claimed to make the simulation multithreaded. Fix: it only parallelises entity sync; the solver uses a thread dispatcher (`BepuSimulation.cs:761`).
3. **ezEngine renderer** was first not checked, then found weak (MSAA only, no TAA, upscaler, dynamic GI or DX12), which put it below Godot.
4. **ezEngine and Steam Deck.** It was wrongly dropped as a Deck candidate because the README says Windows/D3D11 only (`README.md:15`). Correction (user, 2026-11-06): the Windows D3D11 build can ship on the Deck through Proton/DXVK, like most Deck games. Rank restored.
5. **Jolt as Godot's default** was doubted by a subagent. Verified: `editor/editor_node.cpp:8521` writes Jolt into new projects; the `DEFAULT` value in ProjectSettings still means GodotPhysics3D (`ProjectSettings.xml:2666/2670`); the module is registered without `set_default_server` (`modules/jolt_physics/register_types.cpp:60`).
6. **Godot texture streaming.** "Godot has no mesh/texture streaming" and "level must fit in VRAM" are out of date for textures: master has an opt-in texture streaming module (`modules/texture_streaming/texture_streaming.cpp:226`, default false). There is still no mesh streaming. The VRAM statement is from the talks and remains true for meshes.
7. **MultiMesh** mainly saves per-node CPU cost, not draw calls (Godot already auto-instances identical draws).
8. **O3DE `physx_batchTransformSync`** defaults to false (`PhysXSystem.cpp:34`); the subagent had not checked it. Write-back is serial by default.
9. **RADV feature lines** were revised: `multiDrawIndirect`, `drawIndirectFirstInstance`, `drawIndirectCount` and `shaderImageInt64Atomics` are unconditional in Mesa main (not gated by GPU generation). The SteamOS Mesa version was not checked. An earlier web attempt via the gpuinfo site had failed (section 6.2).
10. **Rank changes.** First pass: Godot, ezEngine, Bevy/Stride, O3DE (BRUTAL, SDL, raylib, Bevy-without-physics not a fit). After the Proton correction: Godot, ezEngine, Bevy+Avian or Stride, O3DE. After the cross-check (ezEngine's transform update and write-back are serial, DX11 is single-threaded, MSAA only): **ezEngine moved from rank 2 to rank 3; Stride to 2; Bevy+Avian 4; O3DE 5.** Section 1 is current.
11. **Event label for `AX3lWk-P0UA`** ("GodotCon Boston 2026") was unverified, then checked on 2026-11-06: the video description says "Presented by Clay John at GodotCon Boston 2026" (SceneTree channel, published 2026-08-03). Label confirmed.
12. **Agent-launch hook** denied subagent calls that lacked tracing and evidence sentences; they were added and re-sent. (Process note, no effect on findings.)
13. **Clone coverage.** O3DE and Bevy clones were sparse; they were read via `git ls-tree` / `git show` / `git grep` on HEAD, so absence claims for those two cover only files present in the checkout or fetched by blob.
14. **Racing sim binding (grip talk).** An earlier summary said the racing sim ran its physics "in a GDExtension". Re-reading the transcript: the talk only says an "isolated module"; how it binds to Godot is not stated. Fixed in section 9.4 and the related table row and practice.
15. **Evidence paths.** Section 14 paths were re-checked against the clones before deletion; abbreviated or wrong paths were replaced by full repository paths (for example Bevy `crates/` prefixes, O3DE `Gems/Atom/RPI/.../Scene.cpp` and the Vulkan `FrameGraphExecuter.cpp`). The clones were deleted afterwards, so the paths can no longer be re-checked locally; use the commits in section 14.
16. **Platform target (user, 2026-11-06).** The doc had counted native Linux/Vulkan as a plus for Godot, O3DE, Stride, Bevy, SDL3 and raylib, and an earlier answer called The Forge a fit for a "Deck-first" game. Correction: Quest ships a Windows build only and reaches the Deck through Proton. Changes:
    - Section 2 holds the constraint and the Proton layers; the 4.2 Deck column and the benchmark plan follow it. Linux-only source facts were re-checked for Windows (Godot `main.cpp:2388`; O3DE `AzCore_Traits_Windows.h:103`, `Atom_RPI_Traits_Windows.h:10`).
    - The ranking is unchanged. Stride's Linux crashes no longer count against it, but it now relies on DXVK; that is a judgment, not measured.
17. **The Forge (README-only answer, then source).** A first answer, from the GitHub README alone, said it targets the Deck natively on Vulkan 1.1 and has flecs for gameplay data. Source shows otherwise. The current public repo (Codeberg 1.64) contains only the Windows D3D12 runtime; the Vulkan and Linux code exists only in the frozen GitHub 1.63. flecs is vendored and used by one unit test only. A subagent first reported flecs as unused outside its folder; a spot-check found it in `19_CodeHotReload_Main.cpp`. Section 3.5 is current.
18. **Pinned commits versus current heads (user rule, 2026-11-06: always check the latest source, stable releases are outdated).** The doc did not say that the clones were pinned at the default-branch head of 2026-10-31 to 2026-11-05. A head check on 2026-11-06 found 5 of 10 repos had moved (section 3.6). No claim in the doc changed; some cited line numbers shifted. Rule from now on: read the current head of each repo, record its commit, and re-check the gap before reusing old findings.
19. **ezEngine and Jolt head check (user request, 2026-11-06).** (a) The first head check listed the ezEngine "Inverse Depth" commit as a later change; it is the pinned commit `fb0a247`. (b) The doc's "Jolt 5.5.1" for ezEngine was only the `Core.h` label; the files match upstream at about 2026-06-06 (section 3.7). Results and ranking effect: sections 3.6 and 3.7.
20. **Godot, O3DE and Stride head check (user request, 2026-11-06).** The first O3DE row said the version label changed and articulation inertia calculation was added. Both were wrong (`engine.json` still says 2.8.0; the net diff has no articulation change). Section 3.6 is corrected and holds the Godot and Stride results.

**Source conflicts not resolved by any correction:**
- BRUTAL platforms: README:89 (Windows or Linux) versus the samples README (also macOS).
- BRUTAL start date: "2022" versus "about four years earlier".

---

## 14. Evidence references

Format: `<engine>: <repo-relative path>:<line>`. All paths are inside the study clones. Items marked "web" were not read in the clones.

### 14.1 Godot (`95c0fa5`)
- `godot: thirdparty/README.md:517-521` (Jolt 5.6.0, one backport)
- `godot: doc/classes/ProjectSettings.xml:2666,2670,2680,3336`; `godot: editor/editor_node.cpp:8521`
- `godot: modules/godot_physics_3d/register_types.cpp:58`; `godot: modules/jolt_physics/register_types.cpp:44,49,60`
- `godot: modules/jolt_physics/jolt_physics_server_3d.cpp:131,724,916,1372,1627,1665-1673` (2046 lines)
- `godot: modules/jolt_physics/spaces/jolt_job_system.cpp:106,163`; `.../spaces/jolt_space_3d.cpp:187-216,221-233`; `.../objects/jolt_body_3d.cpp:218,270-274,712-732,890-950,1076-1105,1142-1143`; `.../shapes/jolt_shape_3d.cpp:90,178-185`; `.../objects/jolt_soft_body_3d.cpp:513-851,644-649,658-684`
- `godot: modules/jolt_physics/SCsub:121-124` (Jolt vehicle compiled, no wrapper)
- `godot: core/config/project_settings.cpp:1803`
- `godot: main/main.cpp:570,1464,2241,2242,2388,2389,2491-2508,2657-2658,2787,3556-3559,4961,5031`
- `godot: scene/3d/physics/rigid_body_3d.cpp:171,835`; `godot: scene/3d/physics/vehicle_body_3d.cpp:156-176,397-409`
- `godot: servers/rendering/renderer_scene_cull.cpp:3365-3367,3466-3473,4385`; `godot: servers/rendering/rendering_server.cpp:3827`; `godot: servers/rendering/rendering_server_default.cpp:280`; `godot: servers/rendering/renderer_scene_occlusion_cull.cpp`
- `godot: servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:896,2275,3203`; `servers/rendering/renderer_rd/environment/gi.cpp:67-99,444-458`; `.../effects/fsr2.cpp:171-458,812-819`
- `godot: modules/texture_streaming/texture_streaming.cpp:226`
- `godot: scene/main/scene_tree.cpp:1313-1330`; `godot: scene/main/node.h:232`; `godot: scene/3d/node_3d.cpp:112-139,648-666`; `godot: scene/3d/visual_instance_3d.h:118-120`
- `godot: core/register_core_types.cpp:367-368`; `godot: core/input/gamecontrollerdb.txt:1747-1748`; `godot: drivers/sdl/SCsub:117`
- `godot: modules/gdscript/gdscript_vm.cpp:268`; `godot: modules/mono/glue/GodotSharp/GodotSharp/GodotSharp.csproj:5`; `.../iOS.props:3-5`; `.../Android.props:3`
- `godot: SConstruct:216`; `godot: core/profiling/profiling.h:63-72`; `godot: tests/test_main.cpp:120`; `godot: scene/resources/resource_format_text.h:149`; `godot: servers/register_server_types.cpp:345`
- `godot: doc/classes/Environment.xml:219,266,288,307,364` (Forward+ only features)
- `godot: scene/resources/3d/importer_mesh.cpp:570` (auto mesh LOD); `godot: servers/rendering/rendering_device.h:1571`; `godot: servers/rendering/rendering_device_commons.h:1036`
- File counts at that commit: core 422, scene 690, servers 390, modules 975, drivers 167, platform 285, editor 633.

### 14.2 ezEngine (`fb0a247`)
- `ezengine: README.md:15`; `ezengine: AGENTS.md:5,50-58`; `ezengine: CMakePresets.json:190-224`
- `ezengine: Code/ThirdParty/Jolt/Core/Core.h:8-10`; `ezengine: Code/ThirdParty/Jolt/Jolt.cmake:410-412`
- `ezengine: Code/EnginePlugins/JoltPlugin/System/JoltWorldModule.cpp:58-59,359-373,744,757,778,781-790,787,803,848-937`; `.../System/JoltJobSystem.h:3-9,22`, `JoltJobSystem.cpp:85` (job dispatch via `ezTaskSystem::StartSingleTask`); `.../Actors/Implementation/JoltDynamicActorComponent.cpp:28-57`; `.../Actors/Implementation/JoltTriggerComponent.cpp:99`; `.../Components/Implementation/JoltClothSheetComponent.cpp:6-7,284-285`
- `ezengine: Code/Engine/Core/World/Implementation/World.cpp:524-566,1116,1159-1205`; `.../WorldData.cpp:91-93,337-410`; `.../WorldDesc.h:23`; `.../WorldModule.h:16-18`
- `ezengine: Code/Engine/Core/Scripting/Implementation/ScriptWorldModule.cpp:24-25`
- `ezengine: Code/Engine/RendererCore/RenderWorld/Implementation/RenderWorld.cpp:18,601,686`; `.../Pipeline/Implementation/View.cpp:41`; `.../RenderPipeline.cpp:412+,439-458,756-800`; `.../Meshes/Implementation/MeshRenderer.cpp:32-96`; `.../Shader/Implementation/ShaderPermutationResource.cpp:168-195,245-252`; `.../ShaderCompiler/Implementation/ShaderManager.cpp:10,179,208`; `.../Textures/Texture2DResource.cpp:63-74`
- `ezengine: Code/Engine/GameEngine/GameApplication/Implementation/GameApplication.cpp:49,610-635`
- `ezengine: Code/Engine/Foundation/Threading/Implementation/TaskSystemThreads.cpp:19-58`; `.../Profiling/Profiling.h:220-221`; `.../Foundation/CMakeLists.txt:80-82`; `Code/ThirdParty/tracy/init.cmake:1-8`
- `ezengine: Code/Engine/RendererDX11/Device/Implementation/DeviceDX11.cpp:128-168`; `ezengine: Code/Engine/RendererVulkan/Device/Implementation/DeviceVulkan.cpp:415`
- GAL/render: see the evidence list inline in section 7.

### 14.3 O3DE (`703bbae`)
- `o3de: LICENSE.txt:1-5`; `o3de: Gems/PhysX/Core/PhysX5/Source/Platform/Windows/PAL_windows.cmake:11` (PhysX 5.1.1-rev4 package)
- PhysX: `PhysXScene.cpp:46-48,73,96,137,577,600-634,652-669,1183-1207,1330-1352`; `PhysXSystem.h:79`; `PhysXSystem.cpp:34,191-197,221-236`; `PhysXCpuDispatcher.cpp:19-27`; `SystemConfiguration.cpp:34-35`
- Render: `MeshFeatureProcessor.cpp:53-79,127-148,188-219,239-261,344-370,1260,2065`; `Culling.cpp:46-54,127-135,606-667,683-826,1090-1103`; `OctreeSystemComponent.cpp:403-405`; `Gems/Atom/RPI/Code/Source/RPI.Public/Scene.cpp:118,484-536,497-504,632-707`; `RPISystem.cpp:269-283,321-338`; `FrameScheduler.cpp:589-629`; `FrameGraphExecuter.cpp:30-39,83-84,183-184`; `Gems/Atom/RHI/Code/Include/Atom/RHI/Debug.h:20`; `View.cpp:68-69`; `Atom_RPI_Traits_Linux.h:10`; `Gems/Atom/RPI/Code/Source/Platform/Windows/Atom_RPI_Traits_Windows.h:10` (web, raw file at the commit); `StreamingImageController.cpp:85-172`; `ShaderVariantAsyncLoader.cpp:64-101`; `Shader.cpp:430-449`; `Taa.azsl` (278 lines); `Taa.pass`; `TaaPass.cpp`
- GI/RT: `DiffuseProbeGridFeatureProcessor.cpp:25-26`; `DiffuseProbeGrid.cpp:201`; "Nvidia RTX-GI License.txt"; `ReflectionScreenSpaceRayTracing.azsl:60`; Meshlets gem (v0.0.0)
- CPU: `Launcher.cpp:108,117`; `JobManagerComponent.cpp:27-70`; `AzCore_Traits_Linux.h:115`; `Code/Framework/AzCore/Platform/Windows/AzCore/AzCore_Traits_Windows.h:103` (web, raw file at the commit); `TaskGraphSystemComponent.cpp:27-31`; `TransformComponent.cpp:170,717,756-768`; `RigidBodyComponent.cpp:268-300`; `lua.h:17`; `GraphToLua.cpp`
- Profiler: `Gems/Profiler/Code/Source/CpuProfiler.cpp`; `ImGuiCpuProfiler.cpp`; `ImGuiHeapMemoryProfiler.cpp`
- Issues: o3de #18920, #17857, #2677

### 14.4 Stride (`27f25ec`)
- `stride: sources/plugins/BepuPhysics/Stride.BepuPhysics/BepuSimulation.cs:40,49-50,125-126,252,301-304,421,734,756,758,761,785-806`; `.../Systems/PhysicsGameSystem.cs:27-35`; `.../Systems/CollidableProcessor.cs:51,57-79`
- `stride: sources/engine/Stride.Games/GameBase.cs:655-703`
- `stride: sources/engine/Stride.Rendering/Rendering/VisibilityGroup.cs:133,138,189-191`; `.../RenderSystem.cs:24,203,215,273,299,303,385-447,566,581-582`; `.../EffectSystem.cs:29,137-163`; `.../InstancingRenderFeature.cs:75-100,155`; `.../Streaming/StreamingManager.cs:29,63,179-200`
- `stride: sources/engine/Stride.Engine/Engine/Processors/TransformProcessor.cs:96-131`; `.../AnimationProcessor.cs:70`; `.../InstancingProcessor.cs:42`; `.../ModelRenderProcessor.cs:105,174`; `.../ScriptSystem.cs:49,63-142`; `.../EntityProcessor.cs:221`; `.../Profiling/GameProfilingSystem.cs`
- `stride: sources/shaders/Stride.Shaders.Effects/Compiler/EffectCompilerCache.cs:24-42`
- `stride: docs/build/aot.md:7-10,18`; `stride: .github/workflows/test-linux-game.yml:110-114`; `Directory.Packages.props:11`; `Stride.Frameworks.props:5`
- Issues: stride #2496, #3778, #2648

### 14.5 Bevy (`ae61521`) and Avian (`2c2d693`)
- `bevy: Cargo.toml:139,188-204,344,413,456,459-462,711`; `bevy: crates/bevy_app/src/main_schedule.rs`; `bevy: crates/bevy_time/src/fixed.rs:69,76`; `bevy: crates/bevy_render/Cargo.toml:90-95`
- `bevy: crates/bevy_internal/src/default_plugins.rs:55-57`; `crates/bevy_render/src/pipelined_rendering.rs:58-64,99-136,181-183`; `crates/bevy_render/src/renderer/render_context.rs:102,114-125`; `crates/bevy_extract/src/extract_plugin.rs:136-146`; `crates/bevy_app/src/task_pool_plugin.rs:113-148`; `crates/bevy_ecs/src/schedule/executor/mod.rs:44-55`
- `crates/bevy_transform/src/systems.rs:60,111,273,350,482-491,521`; `crates/bevy_transform/src/plugins.rs:37-46`; `crates/bevy_camera/src/visibility/mod.rs:531,748-786,884`; `crates/bevy_pbr/src/render/mesh.rs:247-250,1952-1977,1972,2075,2494-2580`; `crates/bevy_pbr/src/lib.rs:11,18,148`; `crates/bevy_render/src/batching/gpu_preprocessing.rs:162-165,1545-1602,1566-1574`; `gpu_preprocess.rs:300`; `build_indirect_params.wesl:95-98`; `crates/bevy_pbr/src/meshlet/mod.rs:94-96`
- `crates/bevy_render/src/render_resource/pipeline_cache.rs:50-54`; `crates/bevy_render/src/lib.rs:158-165`; `crates/bevy_render/src/render_phase/mod.rs:2270-2277`; `crates/bevy_anti_alias/src/lib.rs:25-33`; `crates/bevy_anti_alias/src/dlss/mod.rs:5`; `crates/bevy_render/src/occlusion_culling/mod.rs`; `crates/bevy_render/src/diagnostic/tracy_gpu.rs`; `crates/bevy_transform/src/components/transform.rs:92`, `global_transform.rs:60`; `CONTRIBUTING.md:3-4`
- `avian: crates/avian3d/Cargo.toml:3,15-24,29-37,80`; `avian: src/lib.rs:53,56,67,68,227,813`; `src/utils.rs:33-141`; `src/collision/broad_phase/bvh_broad_phase.rs:71`; `src/collision/narrow_phase/system_param.rs:445`, `src/collision/narrow_phase/mod.rs:286`; `src/dynamics/solver/plugin.rs:45,675,688,791,887,992`; `src/dynamics/solver/schedule.rs:191-195`; `src/dynamics/integrator/mod.rs:279,324,361,527`; `src/dynamics/ccd/mod.rs:23-27,641`; `src/physics_transform/mod.rs:118-125,209,321-324,384-409`; `src/schedule/mod.rs:44,54,111,235-263`; `src/schedule/time.rs:7`; `src/collider_tree/optimization.rs:67,288`; `src/collision/collision_events.rs:171,268,297`; `src/collision/collider/mod.rs:484`; `src/character_controller/move_and_slide.rs:68`; `src/dynamics/joints/spherical.rs:16`; `src/tests/determinism_2d.rs`
- Issues: bevy Discussion #23341, #23108; PRs #16757, #17211, #16427, #16670; avian #1081; bevy_rapier #79; wgpu #6854, #8662, #948, #2431

### 14.6 BRUTAL (`7a3b72d`), SDL3 (`c58854d`), raylib (`4bfb72d`), J.E.N.O.V.A
- `brutal: README.md:14-48,84-90,98,141,149-158` (16, 20-22, 26-29, 33, 37-44, 46, 48, 86-89); `brutal: CONTRIBUTING.md:44`; `brutal: samples/README.md`; `brutal: NOTICE.txt`
- `brutal: Brutal.Concurrency/Jobs/JobScheduler.cs:1-40`; `Brutal.Jolt/Generated/Classes/JobSystemThreadPool.gen.cs:51,144`, `JobSystem.gen.cs:52-57`, `BodyLockInterface.gen.cs:160`, `Body.gen.cs:1114`, `BodyInterface.gen.cs:755`; `Brutal.Jolt/Manual/BodyState.cs:20-30`, `BodyAddBatch.cs`, `ContactEvent*.cs`; `DemoJolt.cs:58`
- `sdl: include/SDL3/SDL_gpu.h:51-53,82-83,105-115,541-542,950-956,1047,2006-2008,2626,3276,3299,3503-3507,3971,3993`; `sdl: CMakeLists.txt:3865`; `sdl: AGENTS.md:1-4`
- `raylib: src/raylib.h:570,1633`; `raylib: src/rlgl.h:24-30,754`; `raylib: CMakeLists.txt:33`
- `jenova: _rt/ReadMe.md:57`; `jenova: _rt/Source/Jenova.hpp:467`; `jenova: _rt/Source/gdextension_exporter.cpp:46`

### 14.6a The Forge: Codeberg 1.64 (`5abcdd9`) and GitHub 1.63 (`cd50468`, last code commit `6c0aa87`)
Paths are 1.64 unless marked 1.63. "Agent" means read by a subagent and not re-checked by the main session.
- Scope: `README.md:11-24,36-38`; `LICENSE.txt` (Apache-2.0); directory listings of `Common_3/Graphics`, `Common_3/OS`, `Common_3/Renderer` and `Examples_3/Unit_Tests` in both trees
- Loop: `Common_3/OS/Windows/WindowsBase.cpp:783,794`; `Common_3/Application/Interfaces/IApp.h:87-94,103,245-246` (agent)
- Threads: `Common_3/Utilities/Threading/ThreadSystem.h:43-51,93-124`, `ThreadSystem.c:156-163`; `Examples_3/Unit_Tests/src/03_MultiThread/03_MultiThread.cpp:233,313-316,817-824,975,1007` (agent)
- Renderer: `Common_3/Renderer/VisibilityBuffer2/VisibilityBuffer2.cpp:84-97,188-205,216,261,304`; `.../Shaders/FSL/TriangleFiltering.h.fsl:47-58`; `Common_3/Renderer/Interfaces/IVisibilityBuffer2.h:214-220`; `Common_3/Graphics/Direct3D12/Direct3D12.c:2223-2224,6797` (agent)
- D3D12: `Direct3D12.c:2028-2031,2111-2151,2173,2902,3034-3075,5246-5754`; `Examples_3/Build_Props/VS/TF_Shared.props:78,110,113`; `Common_3/Tools/ForgeShadingLanguage/compilers.py:43,93-118` (agent)
- Deck GPU entry: `Common_3/OS/Windows/pc_gpu.data:306` (1.64) and `:293` (1.63), both `verylow`; 1.63 `Common_3/OS/Linux/steamdeck_gpu.data:18` (`medium`)
- Absence greps (main session): TAA/FSR/DLSS/XeSS/upscaler over `Common_3` and `Examples_3` excluding ThirdParty (only `Common_3/Application/Camera.cpp:990`); Tracy over the whole tree (no hits)
- flecs use: `Examples_3/Unit_Tests/src/19_CodeHotReload/19_CodeHotReload_Main.cpp:26,180-230`; flecs version `Common_3/Game/ThirdParty/OpenSource/flecs/flecs.h:88-90` (agent)
- Loader and streaming: `Common_3/Resources/ResourceLoader/ResourceLoader.cpp:470,2586-2599`; `.../Interfaces/IResourceLoader.h:207-242`; `Common_3/Resources/Streaming/Streaming.cpp`, used by `Examples_3/Unit_Tests/src/15a_VisibilityBufferOIT/15a_VisibilityBufferOIT.cpp` (agent)
- Animation: `Common_3/Resources/AnimationSystem/Animation/Animation.h:39-45`, `AnimatedObject.h:98,101` (agent)
- C API: `Common_3/Graphics/Interfaces/IGraphics.h:2704,2763-2766`
- Community issues (2026-11-06, REST APIs via curl): the issue numbers, what was fetched by the main session or an agent, and what was not checked are listed in section 8.
- 1.63 Vulkan and Linux (agent): `Common_3/Graphics/Vulkan/VulkanConfig.h:63` (Vulkan 1.1); `Vulkan.c` 9627 lines; `Common_3/OS/Linux/*` about 4,049 lines; `Examples_3/Unit_Tests/SteamOS_CodeLite/`; Deck workaround `09_LightShadowPlayground/GPUCfg/gpu.cfg:56`

### 14.7 URLs
- Jolt upstream: https://github.com/jrouwe/JoltPhysics (release notes: `Docs/ReleaseNotes.md`; compared via `git log` and `git show` in a blobless clone). ezEngine: https://github.com/ezEngine/ezEngine (`dev` branch)
- Godot Jolt docs: https://docs.godotengine.org/en/stable/tutorials/physics/using_jolt_physics.html
- The Forge: https://codeberg.org/The-Forge/The-Forge (current) ; https://github.com/ConfettiFX/The-Forge (frozen at 1.63); issues: https://github.com/ConfettiFX/The-Forge/issues , https://codeberg.org/The-Forge/The-Forge/issues
- Godot 4.4 release: https://godotengine.org/releases/4.4/ ; 4.5 release: https://godotengine.org/releases/4.5/
- Rendering Priorities Jan 2024: https://godotengine.org/article/rendering-priorities-january-2024/ ; Sep 2024: https://godotengine.org/article/rendering-priorities-september-2024/
- Priorities: https://godotengine.org/priorities/
- Rendering CPU optimisations (2026-09-15): https://godotengine.org/article/rendering-cpu-optimizations/
- 4.4 dev 4: https://godotengine.org/article/dev-snapshot-godot-4-4-dev-4/
- GodotCon US 2026 update: https://godotengine.org/article/godotcon-us-2026-update/
- Godot PR #102125; HDDAGI PRs #86267 and #119869; issue #90400
- Talks: https://media.ccc.de/v/godotcon2023-57791-the-future-of-rendering-in-godot ; https://talks.godotengine.org/godotcon-24/talk/YX78Q8/ ; https://godotfest.com/talks/from-alpha-to-z-test/
- Arm blog (2025-06-11): developer.arm.com, "Optimizing 3D scenes in Godot on Arm GPUs" (community blogs, mobile graphics and gaming)
- Pruden article: https://sampruden.github.io/posts/godot-is-not-the-new-unity/ ; Hacker News item 37598985
- Pantopie postmortem: https://pantopie.studio/releasing-a-3d-game-with-godot-engine/
- BRUTAL: https://forums.ahwoo.com/forums/brutal/news/brutal-goes-open-source-ahwoo-and-rocketwerkz-release-their-game-dev-framework.1391/ ; https://www.gamedeveloper.com/programming/rocketwerkz-ceo-predicts-frameworks-not-engines-will-be-future-of-game-development ; https://kittenspaceagency.wiki.gg/wiki/BRUTAL ; https://tech.yahoo.com/gaming/articles/dayz-creator-says-unity-accusing-141752673.html (snippet only); https://x.com/rocket2guns/status/1942222168085205338 (snippet only)
- Jenova: https://jenova-framework.github.io/docs/pages/Misc/Benchmark ; `.../Misc/Clarifications`
- Jolt: JoltPhysics discussions #292 and #327; multicore PDF `jrouwe.nl/jolt/JoltPhysicsMulticoreScaling.pdf` (not readable)
- Vulkan device reports (could not be read): https://vulkan.gpuinfo.org/vulkansupport.php ; a public Deck sysinfo gist (older Deck data: API 1.3.224, Mesa 22.2.0-devel, no features of interest)
- Mesa source read through the public GitLab file API: `src/amd/vulkan/radv_physical_device.c`, `radv_constants.h` (Mesa main)
- Valve Steam Deck spec page (steamdeck.com)

### 14.8 Video IDs

Every video ID is listed in the talk tables of sections 9.1 and 9.2 and in the headings of sections 9.3 and 9.4.
