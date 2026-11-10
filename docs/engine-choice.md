# Engine choice for the Quest game (QuestMark study)

Investigation date: 2026-11-06. Status: investigation only. The Quest game has not started; this repository (QuestMark) holds the engine study and benchmark.

Sections 3 to 11 come from reading source code (study clones of each repo), reading talk captions, and web reports. Since 2026-11-08 the ranking in section 1 also uses the desktop benchmark (section 12.2; Godot + Jolt, the Godot PhysX fork, Stride + Bepu and ezEngine + Jolt, on one machine). **Nothing was run or measured on a Steam Deck.**

Labels used in this file:

- **Proved**: read in source or on a fetched page; the evidence is cited in section 14.
- **Inference / hypothesis / not verified**: reasoning or a source that was not checked. Marked in the text.
- "Snippet": the claim comes from a web-search result summary and the page was not opened.

---

## 1. Summary

**Recommendation (2026-11-08, after the desktop benchmark, order confirmed 2026-11-09 with a narrower lead; provisional until a Steam Deck run): ezEngine with Jolt.** Naming (user decision 2026-11-09): "ezEngine" means ezEngine through its own Jolt physics components (default usage, `--physics-path=components`); "ezEngine direct Jolt" means bodies made and stepped through the Jolt BodyInterface by the benchmark (alternative, `--physics-path=raw`). Godot's default is nodes, its alternative is RIDs; ezEngine's default is components, its alternative is direct Jolt. Every ezEngine number measured before 2026-11-09 evening is direct Jolt. The measured numbers and their limits are in section 12.2. The study-only ranking of 2026-11-06 (1 Godot + Jolt, 2 Stride, 3 ezEngine) is kept in the corrections log (section 13).

Ranking for a physics-heavy 3D game at 60 fps on a Steam Deck LCD, built largely by AI agents. Ranks 1 to 3 rest on the desktop benchmark plus the study; ranks 4 and 5 rest on the study only:

| Rank | Choice | Why (short) |
|---|---|---|
| 1 | **ezEngine + Jolt** (Windows build via Proton/DXVK) | Measured (default path = its own Jolt components, section 12.6): the only engine whose default path keeps the game-like mixed scene well inside the frame budget in render mode (p99 8.6 / 8.9 ms, stress / game, against 19.3 ms or more for every other default path: Stride 24 to 39, Godot PhysX nodes 19.3 to 89.5, Godot Jolt nodes 408 to 447). Per step the default path is level with Godot Jolt on RIDs, 1.2 to 1.6 times below node-based Godot Jolt (direct Jolt: 1.4 to 2.2 times, corrected 2026-11-09, section 13 items 25 and 26; the first figure was 1.6 to 2.4), above Godot PhysX nodes on pile and scale (PhysX nodes 14 to 28 % lower; 20 % higher in mixed) and 2.3 to 3.8 times above Stride's Bepu. The alternative path, ezEngine direct Jolt, is 10 to 40 % cheaper per step and has the best rendered frame measured (p99 5.1 / 5.2 ms) and the best stress-profile render-mode scale among the Jolt paths (3000 bodies at 20 ms; the default path reaches 2500 / 2500 game / stress, equal to Godot PhysX nodes and above Stride 2000 and Godot Jolt nodes 1500; debris with rendering 3.5 against direct Jolt 3; ladders measured 2026-11-10 on the default path, section 12.6). Study: richest Jolt components, agent tooling. Risks (not measured): DXVK under Proton, MSAA-only renderer (no TAA or upscaler), serial transform update and write-back, small community. |
| 2 | **Stride + Bepu** (via Proton) | Measured: by far the cheapest physics (Bepu about 1 ms per step at 2000 awake bodies, 8000 to 10000 bodies at 20 ms in physics mode), but an expensive whole frame in render mode (mixed scene about 54 fps on the desktop, cause not profiled). Study: most parallel engine. Risks: beta, no ragdoll builder, no restitution, D3D through DXVK. |
| 3a | **Godot + PhysX fork** | Measured (CPU-only fork build, Godot 4.7.3rc, section 12.2 and 12.3): with nodes pit-2000 2.24 ms per step (c6f3b659; 2.23 ms at 97bc4de), mixed scene 3.85 ms, mixed render 68 fps (p99 89.5 ms), 2500 bodies at 20 ms in both modes; with RIDs + MultiMesh pit-2000 2.18 ms, mixed 3.30 ms, mixed render 262 fps (p99 5.6 ms), 3000 bodies at 20 ms in both modes. Physics per step is the closest Godot variant to ezEngine (against ezEngine direct Jolt, after the 2026-11-09 accounting correction, section 13 item 25, the nodes variant is at or below it on the pile, at 2000 and at 4000 bodies; against ezEngine's default components path the nodes variant is 28 % lower at the pile, 18 % at 4000 bodies and 14 % at 1000 bodies, and 20 % higher in mixed, 2026-11-09) and faster than or equal to Jolt on RIDs in every cell (equal at 3000 bodies with rendering off). Study: single-maintainer fork, our build is CPU-only (the GPU features need NVIDIA CUDA, section 12.2), not recommended as the base. Ordered by the measured numbers: it is faster than Godot + Jolt in every physics cell (nodes and RIDs). The fork risks below are judgment, not measurements, and are why it is not recommended as the base. It is a Godot variant, not a separate engine.|
| 3b | **Godot + Jolt** | Measured with one RigidBody3D node per body (section 12.2): the slowest variant in every physics and whole-frame category (pit-2000 4.0 ms per step, collapse in the mixed render scene at 200 Hz, 1500 bodies at 20 ms). With bodies made directly on the physics server and MultiMesh drawing (section 12.3) the same Godot + Jolt costs 3.14 ms per step at pit-2000 and runs the mixed render scene at 217 fps (p99 5.6 ms); capacity 2000 bodies at 20 ms with rendering and 3000 without; the order is the user's decision (ezEngine's default path runs the mixed render scene at 222 fps with p99 8.6 ms against Godot Jolt RID's 217 fps and 5.6 to 9.1 ms, so on RIDs the two are level; RIDs are Godot's alternative path). Study: best renderer features for a Deck-class GPU (TAA, FSR2, ubershaders, auto-instancing, occlusion culling) and the most agent knowledge. The PhysX fork brings Godot's piles to about ezEngine level (below it on nodes), but it is a single-maintainer fork and not recommended as the base. |
| 4 | **Bevy + Avian** | Best threading design. But physics is third-party, no editor, f32 coordinates by default, API breaks about every 3 months, no ragdoll builder. |
| 5 | **O3DE** | Strongest renderer, but serial gameplay, serial write-back by default, heavy boilerplate, declining project funding. |
| Building blocks, not engines | The Forge, BRUTAL, SDL3, raylib | You would build the engine yourself. The Forge has the strongest renderer base of these (section 3.5), but no physics, scene, TAA/upscaler or editor. |

Why in one paragraph: all candidates share the main risk of this goal, which is main-thread CPU work per physics object on four Zen 2 cores. Godot has the weakest threading of the complete engines (serial dirty-instance loop, serial Jolt write-back to nodes, gameplay on the main thread), but each problem has a documented workaround: physics server bodies (RIDs) plus MultiMesh, the experimental separate render thread, and C++ in a GDExtension. In return it has the best renderer features for a Deck-class GPU and the largest body of material that AI agents know. The engines that thread better (Stride, Bevy) give up built-in physics, ragdolls or API stability, or (Stride) need a D3D-to-Vulkan translation layer under Proton. The only way to settle the order is a benchmark on a real Deck (section 12).

Update 2026-11-08: the paragraph above is the study-only reasoning. The desktop benchmark (section 12.2) measured the main risk directly, and node-based Godot + Jolt came out last (on server bodies with MultiMesh it is no longer last on the whole frame, section 12.3). The same Jolt costs 1.4 to 2.2 times more per step in node-based Godot than in ezEngine direct Jolt (1.2 to 1.6 times against ezEngine's default components path; corrected 2026-11-09 from 1.6 to 2.4, section 13 items 25 and 26), which outweighs Godot's renderer and agent-knowledge advantages for a physics-heavy game. The Deck run is still the deciding test.

Update 2026-11-09 (naming and verdict): the order is unchanged (1 ezEngine + Jolt, 2 Stride + Bepu, 3a Godot + PhysX fork, 3b Godot + Jolt), the lead is narrower. The ranking compares each engine's default usage first, and the deciding measure for a rendered 60 fps game is the frame with rendering; see the verdict update in section 12.2.

**Source freshness.** Rankings rest on pinned clones taken at the default-branch heads of 2026-10-31 to 2026-11-05. Head check on 2026-11-06 (section 3.6, Jolt in 3.7): every repo with a gap was diffed, nothing changed a claim or the ranking. Godot results are for 4.8-beta master, not stable 4.6.

---

## 2. Goal, constraints, method, limits

**Goal.** A high-performance, physics-heavy 3D game ("Quest"). Much of the code is written by AI agents. Target: 60 fps (16.7 ms frame) on a Steam Deck LCD. All candidates had to be open source.

**Platform (user, 2026-11-06).** Quest ships as a **Windows build only**. There is no native Linux build. The Steam Deck is a performance target, reached through Proton. The game is not Deck-first. Native Linux or Linux-Vulkan support therefore counts for nothing in this comparison. What counts is how each engine's Windows renderer runs under Proton on the Deck.

**Steam Deck LCD (Valve spec page).** Zen 2, 4 cores / 8 threads, 2.4-3.5 GHz. 8 RDNA2 compute units at 1.6 GHz. 16 GB LPDDR5 at 5500 MT/s. APU power 4-15 W. Display 1280x800 at 60 Hz. The OLED model differs; the two were not mixed.

**Proton.** Every candidate's Windows build reaches the Deck through Proton. The path depends on the graphics API the Windows build uses:

| Windows API | Proton layer | Candidates (Windows default) |
|---|---|---|
| Vulkan | winevulkan, a thin pass-through to Mesa RADV | Godot (the engine default of `driver.windows` is `vulkan`, `main.cpp:2388`, but the editor writes `d3d12` into NEW projects since 4.6 (`editor_node.cpp:8351`, `get_initial_settings`); the QuestMark Godot benchmark project has no driver setting, so it ran the engine default, Vulkan (per `benchmark/godot/NOTES.md` item 12; the env files do not record the driver)), Bevy/wgpu (backend choice not checked), BRUTAL, SDL_GPU (backend choice not checked) |
| D3D11 | DXVK | ezEngine (benchmarked build: DX11; it also contains Vulkan, which would take the Vulkan row, section 3.1a), Stride (default API not checked) |
| D3D12 | vkd3d-proton | O3DE (default RHI not checked), The Forge 1.64 (D3D12 is its only public runtime, section 3.5) |
| OpenGL | Wine's OpenGL path | raylib |

Translation overhead was not measured. That D3D translation costs more CPU than Vulkan pass-through is an assumption (not checked); the Deck benchmark should settle it. The Deck's system-level FSR1 is spatial only.

**Method.**
- Study clones of each repo were read at a pinned commit, the default-branch head at the time (section 3). Head check on 2026-11-06: section 3.6. Godot and ezEngine were full clones. O3DE, Bevy and Stride were blobless sparse clones read with `git show HEAD:<path>`. BRUTAL, SDL3, raylib, J.E.N.O.V.A and Avian were shallow clones.
- Sonnet-class subagents read code per question. Claims that decide the ranking were spot-checked by the main session against the files.
- Official Godot talks were read from YouTube auto-captions (names and version numbers in captions are unreliable).
- Community problems were collected from the web; many statuses are snippet-level.

**Limits.**
- The study itself built, ran and measured nothing; the later desktop benchmark is in section 12.2. No engine has published Deck CPU numbers that were found.
- Much web evidence is at search-snippet level.
- Not read or not obtainable: the Godot licence text in full; the FidelityFX licence file; the Jolt multicore-scaling PDF (unreadable); the Jolt GDC 2022 talk; the SteamOS Mesa version; DXVK and vkd3d-proton overhead on the Deck.
- Ranks are judgment. Where the text says "hypothesis" it was not tested.

---

## 3. Candidates studied

### 3.1 Versions, language, physics, renderer, editor, licence

| Candidate | Version / commit | Language | Physics | Renderer / graphics API | Platforms | Editor | Licence |
|---|---|---|---|---|---|---|---|
| **Godot** | 4.8-beta master, `95c0fa5` | GDScript, C#, C++ via GDExtension | Jolt 5.6.0 (one backport patch), default for projects created from 4.6; GodotPhysics3D still available | Vulkan, D3D12, Metal, GLES3 | Linux, Windows, macOS, others | Yes, all desktop OSes | MIT (full licence text not read) |
| **ezEngine** | 26.9, `fb0a247` | C++, AngelScript, visual scripts | Jolt 5.5.1-dev snapshot (about 2026-06-06, section 3.7) | DX11 and Vulkan (API 1.1), both in the build files; Vulkan is built by default, D3D11 is opt-in (section 3.1a). Vulkan maturity not read. No DX12 | Full engine Windows only per README; non-rendering code builds on Windows, Android, Linux, macOS | Yes (Qt) | MIT |
| **O3DE** | 2.8.0, `703bbae` (2026-10-29) | C++, Lua, Script Canvas | PhysX 5.1.1-rev4 | Atom: DX12, Vulkan, Metal | Windows, Linux, others | Yes (Qt) | Apache-2.0 OR MIT (`LICENSE.txt:1-5`) |
| **Stride** | 4.5.0-beta01, `27f25ec` | C# (.NET 10) | Bepu 2.5.0-beta.28 (Bullet kept alongside) | D3D11, D3D12, Vulkan | Windows, Linux (crashes reported) | Windows-only WPF | MIT |
| **Bevy** | 0.20-dev, `ae61521` | Rust | None built in | wgpu 30 (Vulkan, D3D12, Metal) | Desktop, others | None | MIT or Apache-2.0 |
| **Avian** (Bevy physics) | 0.8.0, `2c2d693` (2026-11-04) | Rust | Own solver, Parry for narrow phase | n/a | with Bevy 0.20 | None | not read |
| **BRUTAL** | `7a3b72d` (2026-10-27) | C# (.NET 10) | Full Jolt binding (same Jolt commit as Godot's 5.6.0, `e77f175`); Box2D | Vulkan 1.3 bindings only | README: Windows or Linux; samples README also lists macOS (conflict, section 10) | None | MIT plus required credit/logo; not OSI-approved |
| **SDL3** | 3.5.0, `c58854d` | C | None | SDL_GPU on Vulkan, D3D12, Metal | All major | None | zlib |
| **raylib** | 6.5-dev, `4bfb72d` | C | None | OpenGL 1.1-4.3 / ES, software renderer | All major | None | zlib |
| **The Forge** | 1.64, Codeberg `5abcdd9` (2026-08); GitHub frozen at 1.63 | C API, C++ apps, FSL shaders | None | D3D12 only in the public repo (Vulkan and Metal removed in 1.64) | Public repo: Windows only. README lists Deck, mobile and consoles (console code licensed separately) | None public (Gladiator editor named in README only) | Apache-2.0 (PC); consoles commercial |
| **J.E.N.O.V.A** | `4bd544d`; Jenova-Runtime `f9cb794` | C++ scripting for Godot | n/a (Godot's) | n/a | Windows x64, Linux x64 | Godot editor | Runtime MIT; other parts proprietary or custom (section 10) |

### 3.1a ezEngine renderer status: DX11 and Vulkan (2026-11-08)

Earlier text called Vulkan unfinished and DX11 the only complete renderer. That came from the README and the project site. The build files and PR history say otherwise.

- PROVED (build files of the ezEngine clone at `100564a`, read 2026-11-07: `Code/BuildSystem/CMake/CMakeUtils/ezUtilsDX11.cmake:11`, `ezUtilsVulkan.cmake:5`, `ezUtilsRenderer.cmake:27`):
  - `EZ_BUILD_VULKAN` defaults ON.
  - `EZ_BUILD_D3D11` defaults OFF, with the note "D3D11 is nearing end of life and may be removed in a future release".
  - With `EZ_DEFAULT_RENDERER` empty, the default renderer is DX11 if built, else Vulkan.
  - Our benchmark build turned D3D11 on explicitly (`-DEZ_BUILD_D3D11=ON`) and also contains Vulkan. The app takes `-renderer DX11|Vulkan`.
- PROVED (GitHub API search and PR pages: titles, states and dates; PR text was read through a summarising fetch, so details may be missing), `ezEngine/ezEngine`:
  - PR 1958 (merged): Vulkan built by default, D3D11 still the default renderer, `-renderer Vulkan` selects it.
  - PR 1948 (Render Graph) and PRs 1955, 1976, 1977, 1963 (merged by 2026-08-11).
  - PRs 2061 and 2080 ("Vulkan Fixes") and 2122 ("Vulkan: Fix stale shader across compute/graphics scopes"), merged September to October 2026.
  - PR 2062 "Disable D3D11 by default and use Vulkan instead" (merged). The author says Vulkan was already the default for developers and that CI now uses Vulkan. A maintainer objected "vulkan performance is still lacking"; the author replied that D3D will not be removed "for a while". The thread has no numbers.
  - PR 2038 (render pipeline rework, merged), PR 2153 (merged), PR 2150 ("Minor fixes", open).
  - No open Vulkan issue was found; the searches were truncated.
- PROVED (ezengine.net, read 2026-11-08, undated): "The renderer currently uses DX11"; the Vulkan port is "still in the early phase and not yet productively usable". The `README.md` of the clone says the same in other words. These statements conflict with the build defaults and PR history above. The site may be stale.
- HYPOTHESIS (not measured): under Proton on the Deck, Vulkan would be a winevulkan pass-through to RADV, while the D3D11 build goes through DXVK.
- Limits: Vulkan code maturity was not read. `RendererVulkan` has about 12.6k lines against about 6.7k in `RendererDX11`, with 28 TODO or not-implemented hits (counts by grep, not an assessment).
- No ezEngine Vulkan timing existed before this note. All ezEngine numbers in this document are D3D11 (Shipping). A DX11 against Vulkan A/B (stages A and D) was run on 2026-11-08 (result below). A/B result (2026-11-08, PROVED by `*.summary.txt` of 48 runs: stages A and D, game and stress profiles, physics and render modes, 3 runs per cell, same Shipping exe, `-renderer DX11` against `-renderer Vulkan`, `render_backend` in env.txt confirms the backend in 48 of 48 files, 0 failed cells):
  - Vulkan is slower in every cell by median of the three runs: unpaced fps in physics mode about 710 to 900 against 1120 to 1470 on DX11 (about 35 to 40 percent lower); in render mode 225 to 262 against 260 to 330 (about 0 to 28 percent lower; stress pit-2000 render is about equal at 257 to 263 against 261 to 331 with one DX11 outlier at 261).
  - Physics step cost (tick frames, median) is 0 to 9 percent higher on Vulkan (for example pit-2000 game 2.77 against 2.69 ms, mixed game 2.22 against 2.06 ms); stress mixed render is within run noise (1.97 to 2.17 against 2.00 to 2.10 ms). The same Jolt build runs in both, so this is probably the renderer sharing the cores (HYPOTHESIS, not profiled).
  - Frame-time p99 in render mode is higher or equal on Vulkan (pit-2000 game 5.9 to 8.4 against 5.0 ms; mixed 5.7 against 5.3 ms).
  - Limits: one desktop with a discrete GPU, native Windows (no Proton, so no Deck statement), unpaced render loop, vsync off, one exe built at `100564a`, the idle state was sampled only at the start of the run, run-to-run spread on DX11 stress pit-2000 render is large (261 to 331 fps). The ranking in section 1 is unchanged; no Vulkan number exists for stages B, C, E, F or G.

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
| **ezEngine** | C++, AngelScript | DX11, Vulkan (`-renderer DX11\|Vulkan`; Vulkan maturity not read, section 3.1a) | Clustered lighting, SSAO, baked probes; **MSAA only** | OpenDDL text | Good: `AGENTS.md`, MCP editor plugin | Weakest renderer of the engines; small community |
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

### 3.8 Looked at, not benchmarked: Lumix, Lumina, Wicked Engine (2026-11-08)

Shallow clones at head (`.tmp/engines-study/`), read only, nothing built or run, so no CPU timings exist for these three. Commit dates are the clone's head (one commit visible; cadence not readable). Spot-checked by me: Wicked Jolt version and `SetFrameRate`, Lumix PhysX 4.1 header and Linux-forces-Vulkan line, Lumina Box3D dependency line. Everything else is the reading agents' report (PROVED = read in source by the agent).

| | Lumix `9e5ae34` | Lumina `4e9e657` | Wicked Engine `675355f` |
|---|---|---|---|
| Language, licence | C++20, MIT | C++23 + C# (.NET 10), Apache-2.0 | C++17, MIT |
| Physics | PhysX 4.1.2 (prebuilt lib). Jolt only as an out-of-tree plugin (README: early prototype) | Box3D (vendored C source); no Jolt | Jolt 5.6.0 in-tree, only backend |
| Fixed step | none: one `simulate(dt)` per frame, dt clamped to 1/20 s | accumulator, default 60 Hz, max 8 steps, settable `PhysicsHz` | accumulator, 1/60 s, max 4 catch-up steps, setter `SetFrameRate` exists (200 Hz untested) |
| Step threading | PhysX tasks on the engine job system | Box3D solver tasks on the engine job system | Jolt's own thread pool, step blocks the main thread inside scene update |
| Sync to game | loop over ALL dynamic actors every frame (includes sleeping), main thread | per-step sync/interpolation passes, cost per active body not measured | loop over ALL rigid bodies every frame, skips inactive |
| Renderer | DX12 default; Vulkan backend is a clear-only stub; no working Linux renderer in head | Vulkan 1.4 required; mesh shaders optional in code (README says mandatory, stale); VRAM gate 2816 MiB | DX12 (Windows) and Vulkan (Windows, Linux, SDL); many heavy optional features |
| Scripting, editor | experimental in-house Evox, C++ plugins; Studio editor | C# on CoreCLR, ImGui editor | Lua 5.4, C++; in-tree editor, Windows and Linux templates |
| Platform notes | Windows only in practice; Proton needs VKD3D-Proton (untested) | Windows needs VS 2026; Linux needs GCC 16; Proton untested | native Linux Vulkan build exists; Proton untested |
| Maturity | about 156k lines C++, unit tests, shipped games listed in README | about 719k lines, tests + benchmarks, README: "educational project" | about 165k lines C++ + 42k HLSL, sample app only, no unit-test suite |

HYPOTHESES (not checked): any Deck frame cost, Proton behaviour of all three, whether Wicked's heavy GI features can be turned off cheaply, whether Box3D matches Jolt's solver quality and sleeping, Lumix's `lumixengine_jolt` completeness.

Verdict (my reading, labelled judgement): none of the three replaces the current ranking. Lumix has no fixed step, a DX12-only renderer and PhysX 4. Lumina's only attraction is Box3D, which is new, and its toolchain is bleeding edge. Wicked is the closest (Jolt 5.6.0, same as Godot, native Linux Vulkan), but its sync loop is O(all bodies) like several others and the step is main-thread-blocking; it would need its own benchmark port (about the effort of ezEngine's) to say more. Not planned unless you ask.

## 4. Full comparison tables (latest "remade" version)

### 4.1 Features

Moved: type, language, licence, physics and renderer per candidate are in sections 3.1, 3.2 and 3.3 (no separate table, to avoid keeping three copies).

### 4.2 Deck and agent fit

CPU design per engine is in section 5; reported problems per engine are in section 8.

| | How it runs on the Deck | Fit for AI agents |
|---|---|---|
| **Godot** | Windows Vulkan build under Proton (winevulkan pass-through) | **Best**: largest knowledge base, text scenes |
| **ezEngine** | Windows D3D11 build under Proton (DXVK, the variant benchmarked); the same build also contains a Vulkan renderer (winevulkan under Proton, not measured, section 3.1a) | Good: `AGENTS.md`, MCP plugin, text scenes |
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
| **ezEngine** | Full engine Windows/D3D11 only; Vulkan port ongoing; Linux editor not usable (`README.md:15`). Superseded for the renderer: build files and PR history show Vulkan built by default (section 3.1a) | README | checked locally |
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

What exists (PROVED by the files in `benchmark/`; see `benchmark/README.md`): a shared spec (`benchmark/SPEC.md`), a Godot implementation (C++ GDExtension, Jolt by default; PhysX fork selectable, see `benchmark/godot/PHYSX.md`) and a Stride implementation (Bepu, JIT and Native AOT), scenarios A to G, run and merge scripts, and a render check. Only Godot and Stride are implemented; ezEngine and Bevy + Avian are not in the benchmark.

Proved:
- Both engines run all scenarios in short smoke runs at the current settings (200 Hz physics, unlocked render loop, shared camera rule, game and stress profiles). Smoke runs show the code works. They are not results (godot and stride NOTES).
- Earlier full runs (5 s warm-up, 20 s measured, 3 repeats, scenarios A to D) exist only at 60 Hz physics, for Godot (GDScript and C++) and Stride JIT. They are not comparable with the 200 Hz setup and are not used for any claim here.

Open:
- The full protocol ran once at 200 Hz on 2026-11-07 for all four variants (one desktop machine, results not committed yet). No timing verdict is drawn yet: the results are not reviewed, and stages near a ladder limit flip between runs (the one-run ladder decision and the 3-run median disagree by one stage in some ladders).
- No run on a Steam Deck under Proton, and no Deck is available (2026-11-07); desktop numbers do not transfer to it, so the Deck target stays unmeasured.
- Scenario G and the PhysX fork ran in that one full run only; the scale and debris ladders were completed once.
- Some settings are not equal between the engines (solver, damping formulas, restitution, light units; list in `benchmark/SPEC.md`, "Settings parity"), so cross-engine ratios will carry those caveats.

Superseded on 2026-11-08 by section 12.2: the reduced second full run (Godot Jolt re-measured, PhysX fork rebuilt at 97bc4de for stages A and C, ezEngine all stages) was reviewed together with the first run, and the ranking in section 1 now uses the measured numbers. The Deck items above stay open.

### 12.2 Benchmark results and verdict (2026-11-08)

**Setup.** One desktop machine (Windows, idle, discrete GPU), fixed 200 Hz physics, render loop unlocked, profiles `game` (engine-default sleeping) and `stress` (sleeping off). Variants:
- Godot 4.7.2-stable + Jolt. The benchmark code is a C++ GDExtension and every body is a `RigidBody3D` node with a `MeshInstance3D` (`quest_bench.cpp:476,705`), so the RID/MultiMesh workaround from section 11 is NOT tested in the rows of this section (it was tested afterwards, see section 12.3). Re-measured on 2026-11-08 with `physics_ms` definition 2.
- Godot PhysX fork, CPU-only build, same node path. Stages A and C come from 97bc4de (2026-11-08); stages E, F and G come from c6f3b659 (2026-11-07), where jointed bodies ran with 1/1 iterations (see `benchmark/godot/PHYSX.md`). Its D and E mixed numbers are therefore optimistic.
- Stride 4.4.1 + Bepu, JIT and Native AOT, measured on 2026-11-07.
- ezEngine 100564a + Jolt, Shipping D3D11, measured on 2026-11-08 (raw path = "ezEngine direct Jolt") and, for the cells named in section 12.6, on 2026-11-09 through its own Jolt components ("ezEngine", the default usage). NAMING (user decision 2026-11-09): "ezEngine" = components path (`--physics-path=components`); "ezEngine direct Jolt" = bodies made and stepped through the Jolt BodyInterface (`--physics-path=raw`, alternative; the exe default is components since 2026-11-09). All ezEngine runs before 2026-11-09 evening (stage A to G r2 run, ladders F and G, debris, raycast, renderer A/B, sleeping experiment) used direct Jolt, so those numbers are labelled "ezEngine direct Jolt" below. The tables below keep the 2026-11-08 text where history; the ezEngine (components) column holds the 2026-11-09 values.

Each A to E cell has 3 runs (median shown). Ladder stages are single runs. Results live in the gitignored `.tmp/results-200hz*/` folders and are not committed; the tables come from `benchmark/tools/merge-results.pl`. Parity check, mixed render env: Godot against ezEngine has 17 keys equal and 0 mismatches; Stride against ezEngine has 1 mismatch, `shadow_map_size` (Stride uses 4 shadow cascades).

**Table A: headline, unpaced wall ms per physics step** (1000 / steps per s, physics mode, lower is better). It includes the per-step code and the engine loop, so it does not depend on how each engine's `physics_ms` stopwatch is defined. The "ezEngine" column is the default (components) path, measured 2026-11-09 (PROVED, `.tmp/comp-chart-1`, 3 runs per cell, median, 1000 / `steps_per_s_window`; scale 2000 not measured). CORRECTION 2026-11-09 (PROVED, section 12.6 and 13 item 25): the ezEngine direct Jolt (raw path) column first counted only the step work and left out the app frame between its 10 ms step slices, so it was 7 to 9 % too low; the values below are the like-for-like ones (measured seconds plus the overhead logged on the summary `pacing:` line, divided by steps; the build prints it as `steps_per_s_window`). The other columns were not affected: Godot's measured seconds cover its whole loop window and Stride's run from the first to the last row. RE-MEASURED 2026-11-09 (PROVED, `.tmp/comp-chart-1`, 3 runs per cell, median, 1000 / `steps_per_s_window`): the ezEngine direct Jolt cells of pit-2000 (stress 2.82, game 2.78), mixed (stress 2.47, game 2.33), scale 1000 stress 1.158 and 4000 stress 5.49 below are the re-measured values; the scale 2000 stress value (2.68) is not re-measured and stays the corrected 2026-11-08 one. Section 12.6 and item 26 of section 13.

| Scene | Stride Bepu JIT | Stride Bepu AOT | Godot PhysX fork | ezEngine | ezEngine direct Jolt | Godot Jolt | Godot Jolt RID | Godot PhysX RID |
|---|---|---|---|---|---|---|---|---|
| pit-2000, stress (2000 awake) | 1.02 | 1.04 | 2.24 (c6f3b659) | 3.10 | 2.82 | 3.99 | 3.14 | 2.18 |
| mixed, stress (3000 bodies, 100 chains, 1000 rays per step) | 1.25 | 1.33 | 3.85 (c6f3b659, joints 1/1) | 3.22 | 2.47 | 5.03 | 3.50 | 3.30 |
| scale 1000 / 2000 / 4000, stress | 0.59 / 1.02 / 1.69 | 0.62 / 1.04 / 1.71 | 1.17 / 2.36 / 5.30 | 1.366 / not measured / 6.45 | 1.158 / 2.68 / 5.49 | 1.85 / 4.07 / 9.37 | 1.51 / 3.15 / 6.26 | 1.145 / 2.26 / 4.73 |
| pit-2000, game (default sleeping) | 0.09 (pile sleeps) | 0.09 (pile sleeps) | 0.23 (pile sleeps) | 3.20 (about 1860 awake) | 2.78 (about 1900 awake) | 3.97 (about 1900 awake) | 3.09 (about 1900 awake) | 0.21 (pile sleeps) |
| mixed, game | not listed here | not listed here | not listed here | 3.27 | 2.33 | 5.06 | not listed here | not listed here |

**Table B: whole frame, render mode** (physics at 200 Hz plus rendering in one loop; the frame budget is 20 ms, 33.3 ms is the limit2 rule).

| Measure | Stride Bepu JIT | Stride Bepu AOT | Godot PhysX fork | ezEngine | ezEngine direct Jolt | Godot Jolt | Godot Jolt RID | Godot PhysX RID |
|---|---|---|---|---|---|---|---|---|
| D mixed render: p99 wall ms, game / stress | 39.0 / 25.0 | 24.0 / 25.0 | 19.3 / 89.5 (c6f3b659) | 8.9 / 8.6 | **5.2 / 5.1** | 447 / 408 | 9.2 / 5.6 to 9.1 | 5.5 / 5.6 |
| E unlock-render-mixed: fps, game / stress | 54 / 54 | 53 / 51 | 291 / 68 (c6f3b659) | 216 / 222 | **297 / 299** | 2.7 / 2.8 | 206 / 217 | 412 / 262 |
| F render: most bodies passing 20 ms, game / stress | 2000 / 2000 | 2000 / 2000 | 2500 / 2500 (c6f3b659) | 2500 / 2500 | 2000 / **3000** | 1500 / 1500 | 2500 / 2500 | 4000 / 3000 |
| F physics: most bodies passing 20 ms, game / stress | **8000 / 8000** | **8000 / 10000** | 2500 / 2500 (c6f3b659) | 2000 / 3000 | 3000 / 3500 | 2000 / 2000 | 3000 / 3000 | 3500 / 3500 |
| G debris: highest R passing 20 ms, render / physics | 3 / 8 | 3 / **10** | 3 / 5 (c6f3b659) | 3.5 / 3.5 | 3 / 3 | 2.5 / 2.5 | 3 / 3 | **7** / 8 |
| B raycast-10000: wall ms per frame (all fail real time) | 394 | 498 | 653 (c6f3b659) | 353 | **345** | 503 | 500 | 644 |

The ezEngine column (components path) is from the 2026-11-09 session (PROVED, `.tmp/comp-chart-1`, 3 runs per cell, median; D is the paced stage D, E the unlocked loop, raycast the game-profile median; section 12.6). The ezEngine F and G cells are from the ladder run of 2026-11-10 (PROVED, `.tmp/comp-ladders-1`, `run-all.sh ezengine --stages=FG`, components path in every cell, Shipping D3D11 exe, 3 runs at the last pass and first fail, other ladder stages 1 run, sun 2.0, idle machine, 0 failed cells; section 12.6). The earlier `comp-all5` components ladders used the old accounting and are history in section 12.6. The ladder values of ALL columns follow the `merge-results.pl` rule (see the note below and section 13 item 29). The 2026-11-08 and 2026-11-09 direct Jolt rows are one engine path; the 2026-11-09 components cells are not the same session as the other engines' cells.

Ladder rule (all F and G rows, corrected 2026-11-10): the value is the highest tested stage whose median passes the 20 ms p99 rule and the real-time rule, midpoints included (the ladder table at the end of the `merge-results.pl` summary). It is not the `LADDER ... last pass 20ms=` line of the run-all logs, which can drop a passing stage and uses run 1 only (section 13 item 29). CORRECTION 2026-11-10: the 2026-11-08 "PROVED correction" that stood here (G row moved to the log-line values 3 / 6, 3 / 8, 3 / 4, 2 / 2 for Stride JIT, Stride AOT, the PhysX fork and Godot Jolt) was itself wrong; the merge rule gives 3 / 8, 3 / 10, 3 / 5 and 2.5 / 2.5 for the four (so the values printed before 2026-11-08 were right for Stride JIT, Stride AOT and the PhysX fork physics cells; Stride JIT: 8 passed as a median of 3 and no higher rate was tested, so its capacity may be higher). RID: Jolt RID 3 / 3 (R = 3.5 and 4 fail), PhysX RID 7 / 8 (was 6 / 8). Limits: ladder steps coarse (1 2 4 8 then bisection), so a value is uncertain by about one stage; the node and Stride runs are from 2026-11-07 and 2026-11-08, the RID runs from 2026-11-08, the ezEngine runs from 2026-11-10; one desktop.

RID columns: game-profile and raycast cells from a second set of 3 runs per cell (2026-11-08, same exes, idle desktop; stress medians reproduced within 3 %). The Jolt RID stress mixed-render p99 was 5.6 ms in the first set and 9.1 ms in the second (medians equal at 4.8 ms). PROVED from `steps_per_frame` in the CSVs: the first set had 24 to 32 two-step frames of about 4350 (0.6 to 0.7 %), the second set 68 to 96 of about 4200 (1.6 to 2.3 %); the p99 is the 2-step frame time (about 9 ms) once more than 1 % of the frames take two steps. It is a threshold effect, not a slower engine. Why the second session had more two-step frames (frame rate 210 against 219 fps) was not checked, so both are given. PhysX RID in the game profile sleeps (pit 0.21 ms per step, mixed render 412 fps), so read it next to `awake_bodies`.

**Analysis.** Each point is labelled PROVED (measured; the data is named) or HYPOTHESIS (not tested).

1. PROVED (tables A and B): **the same Jolt costs 1.4 to 2.2 times more per step in node-based Godot than in ezEngine direct Jolt, and 1.2 to 1.6 times more than in ezEngine's default components path.** Against ezEngine direct Jolt: at pit-2000 it is 3.97 against 2.78 ms (game, 1.43; stress 3.99 against 2.82, 1.41); in mixed, 5.06 against 2.33 (game, 2.17; stress 5.03 against 2.47, 2.04); along the scale ladder from 1000 to 4000 bodies (stress; direct Jolt 1000 and 4000 medians of 3, 2000 a single run) the factor is 1.5 to 1.7 (1.60, 1.52, 1.71). Against ezEngine (components, computed from Table A): pit stress 1.29, mixed stress 1.56, 4000 bodies 1.45, 1000 bodies 1.35, pit game 1.24, mixed game 1.55. CORRECTION 2026-11-09: this was 1.6 to 2.4 before the ezEngine raw values were corrected for the app-frame time that the old `steps_per_s` left out (section 13 item 25); the ezEngine values were then re-measured the same evening (section 12.6, item 26), which moved the lower end from 1.45 to 1.4 (PROVED, arithmetic on the values in Table A). The correction narrows ezEngine's per-step lead over Godot Jolt; it does not change the order in section 1, and the final order is the user's decision. On the F physics ladder Godot Jolt nodes fail at 2500 bodies and ezEngine direct Jolt passes 3500 (stress), the default path 3000; on the G ladder Godot Jolt passes 2.5 events per s, ezEngine direct Jolt 3 and the default path 3.5 (ladders of the default path: 2026-11-10, section 12.6). Limits:
   - The Jolt versions differ (5.6.0 against 5.5.1-dev).
   - Thread counts were not forced equal.
   - ezEngine runs in a window, because it has no headless mode.
   - Its `physics_ms` stopwatch is narrower; the headline metric avoids that stopwatch.
2. PROVED: **the PhysX fork, running through the same Godot node path, reaches ezEngine direct Jolt's per-step cost on piles and is below it at pit-2000; it is below ezEngine's default components path on piles and scale and above it in mixed.** Against ezEngine direct Jolt: 2.24 against 2.82 ms at pit-2000 (20.6 % lower), 2.36 against 2.68 at 2000 bodies (not re-measured) and 5.30 against 5.49 ms at 4000 bodies (3.5 % lower, PhysX single runs, inside drift); at 1000 bodies direct Jolt 1.158 against PhysX nodes 1.17 and PhysX RID 1.145 is a near tie (within about 1 %). Against ezEngine (components, 2026-11-09): PhysX nodes are 28 % lower at pit-2000 (2.24 against 3.10), 18 % lower at 4000 bodies (5.30 against 6.45), 14 % lower at 1000 bodies (1.17 against 1.366) and 20 % higher in mixed (3.85 against 3.22); PhysX RID is 30 % lower at pit-2000 (2.18), 27 % lower at 4000 bodies (4.73) and about equal in mixed (3.30 against 3.22). Jolt RID is about equal to ezEngine (pit 3.14 against 3.10, 4000 bodies 6.26 against 6.45, mixed 3.50 against 3.22, 1000 bodies 1.51 against 1.366). The components path costs 10 to 40 % more per step than direct Jolt in the same session (section 12.6). The PhysX numbers are from the 2026-11-07 run; the ezEngine values are the corrected ones re-measured on 2026-11-09 (section 13 items 25 and 26; the corrected-only values were 2.72 at pit-2000, 5.47 at 4000 and 1.136 at 1000, which gave about 18 % and 3 %; before the correction this item said 2.24 against 2.52 and 5.30 against 5.05). In mixed stress ezEngine direct Jolt (2.47) is ahead of every Godot variant; ezEngine's default path (3.22) is ahead of Godot Jolt and PhysX nodes (5.03, 3.85) and about equal to PhysX RID (3.30) and Jolt RID (3.50 is 9 % above). Limits: stress profile, desktop, one machine; direct Jolt scale cells at 1000 and 4000 bodies are medians of 3, at 2000 a single run. Within the 2026-11-08 run (both `physics_ms` definition 2, stress pit-2000), the Godot tick costs 2.85 ms with PhysX and 3.97 ms with Jolt.
   - HYPOTHESIS: Godot's extra Jolt cost is in its Jolt module (per-body state access and sync), not in the `RigidBody3D` node layer that both backends share. Nothing was profiled.
   - HYPOTHESIS (written before section 12.3, now tested): replacing the nodes with RIDs would therefore not close most of the gap. CORRECTION 2026-11-08: this was a hypothesis, not a result. Section 12.3 measured it. It is REFUTED for the whole frame in render mode (mixed scene: Godot Jolt 2.8 fps on nodes, 217 fps on RIDs) and PARTLY refuted for physics (Jolt saves 18 to 32 % per step on RIDs, but the per-step gap to ezEngine and Stride remains). The first sub-hypothesis (the extra Jolt cost sits in the Jolt module, not in the node layer) is wrong in part: the node layer was a large part of it.
3. PROVED: **Bepu is the cheapest physics by far.** It is about 2.8 times below ezEngine direct Jolt (2.82 against 1.02 ms, re-measured 2026-11-09; corrected from 2.0 to 2.5, which was 2.7 with the corrected-only 2.72), 3.0 times below ezEngine's default components path (3.10 against 1.02; 2.3 to 3.8 times over pile, mixed, 1000 and 4000 bodies) and 3.9 times below Godot Jolt (pit-2000, stress), and its F physics capacity is 8000 to 10000 bodies against 2000 to 3500 for the Jolt variants. JIT and AOT are equal within noise. Limits:
   - Bepu has no restitution.
   - Solver iterations and substeps are not the same quantity in Bepu and Jolt, so the two engines do not do identical work.
   - Measured in the first run only.
4. PROVED: **Stride loses most of that lead in render mode.** Its physics costs about 1 ms per step, yet mixed render runs at about 54 fps unlocked, with an 18 to 20 ms median frame. The cost therefore sits outside the physics step. Render-mode capacity (2000 bodies at 20 ms, both profiles) is below ezEngine's default path (2500 / 2500, game / stress, 2026-11-10) and equal to or below ezEngine direct Jolt's (2000 / 3000), within the one-stage ladder uncertainty.
   - HYPOTHESIS: the cause is per-entity render submission (no automatic instancing, section 4.4) plus the 4-cascade shadow setting (the parity mismatch above). Not profiled.
5. PROVED for the node-based Godot rows (CORRECTED by section 12.3: the collapse is the node + `MeshInstance3D` path, not Jolt itself; on RIDs the same scene runs at 217 fps): **Godot Jolt's render-mode collapse comes from its per-step cost, not from a separate render problem.** At about 5 ms per step, 200 steps per s need about one full second of main-thread time per second. In mixed render the loop therefore falls behind (real-time factor 0.87, 2.7 fps) and hits the per-frame step cap. Per tick the cost is the same as in physics mode (pit-2000: 13.0 ms of physics per frame at about 3.2 ticks per frame, about 4 ms per tick).
   - HYPOTHESIS: at 60 Hz physics every variant would cost about 3.3 times less per second and Godot Jolt would no longer collapse, with the order unchanged. Not measured in the 200 Hz protocol.
6. PROVED: **sleeping differs by engine design** (user decision 2026-11-08: reported, not forced equal). With default settings the Bepu and PhysX piles fall asleep (0.09 and 0.23 ms per step in the game profile). The Jolt pile stays awake in both hosts.
   - A Jolt sleep threshold of 0.1 m/s puts the pile to sleep at 27 s (diagnostic run, `benchmark/SPEC.md` sleep row).
   - What a higher threshold does to how the game feels was not tested.
   - PROVED (ezEngine direct Jolt sleeping experiment, 2026-11-08; pit-2000, game profile, physics mode, 5 s warm-up + 20 s window, 3 runs per cell with identical awake counts; new options `--sleep-vel` and `--sleep-time`): at the default 0.03 m/s 1920 of 2000 bodies are awake at the end; 0.05 gives 1883, 0.1 gives 1783, and 0.3 and 1.0 put all 2000 to sleep (0.3 at about 19.5 s after start, 1.0 at about 10.7 s). The step cost then falls from about 2.6 ms to 0.003 ms (median of the last 5 s). The awake count drops from about 1780 to 0 within one step, so the pile sleeps as one island (read from the data, no source check). A shorter sleep time at the default velocity does not help (0.1 s: 1902 awake; 0.05 s: 1913).
   - Limits: one scene, one machine, 25 s horizon (0.1 may sleep later; the Godot Jolt diagnostic above slept at 27 s). The threshold is a point velocity; 0.3 is 10x the default and a body creeping slower than that freezes, which was not judged in play. HYPOTHESIS: the PhysX and Bepu sleep gap is mostly a threshold choice, not solver quality.
7. PROVED (G): **creating bodies at run time is about 6 to 10 times dearer in Godot.** One 64-piece event costs 0.42 to 0.66 ms in Godot Jolt against 0.06 to 0.07 ms in ezEngine direct Jolt (the spawn cost of the default components path was not re-measured after the accounting change; its debris ladder is in section 12.6; in the `comp-all5` run created pieces cost 0.19 to 0.28 ms per event, section 12.6, old accounting). Pooling brings Godot down to 0.05 ms (physics mode).
8. PROVED (B): **10000 rays per step at 200 Hz fail real time in every engine.** ezEngine (353 ms per frame, direct Jolt 345) is the least slow and the PhysX fork the slowest, so this scene does not separate the candidates.

**Verdict (2026-11-08, provisional until a Deck run).**
1. **ezEngine + Jolt.** As written 2026-11-08 (all numbers here are ezEngine direct Jolt): it is the only variant that keeps the game-like mixed scene well inside the frame budget in render mode (p99 5.2 ms; every other variant is at 19 ms or more). It has the best render-mode scale (3000 bodies at 20 ms, stress) and the lowest Jolt cost per step. See the verdict update 2026-11-09 below for the default path. The study's Jolt component set (ragdolls, joints) and its agent tooling (`AGENTS.md`, MCP plugin) still apply. Risks from the study, none measured here:
   - D3D11 through DXVK under Proton.
   - MSAA-only renderer (no TAA, no upscaler).
   - Serial transform update and Jolt write-back.
   - A small community.
   - No headless mode.
2. **Stride + Bepu.** It has the strongest physics in every physics-only measure, but its whole frame in render mode is the most expensive after Godot Jolt. It also has no ragdoll builder and no restitution, and it is a beta.
3. **Godot, with the PhysX fork (3a) ahead of Jolt (3b) on the measured numbers.** Godot + Jolt is the slowest variant in every measured physics and whole-frame category (node-based Godot only; see the note after this list). Its renderer features and the largest body of agent knowledge (section 4.2) are unchanged, but they do not offset a 1.4 to 2.2 times physics cost (versus ezEngine direct Jolt, section 12.2; 1.2 to 1.6 times versus ezEngine's default path) for a physics-heavy game.
   - Ordered by the numbers, the Godot + PhysX fork (3a) comes before Godot + Jolt (3b): faster per step in every physics cell and, on nodes, 68 against 2.8 fps in the rendered mixed scene. It lifts Godot's piles to about ezEngine direct Jolt level. It is still not recommended as the base: it is a single-maintainer fork, our build is CPU-only, its jointed solver-iteration default had to be overridden, and its mixed numbers are not yet re-measured at 97bc4de.

**Verdict update 2026-11-09 (main-session decision; provisional until a Steam Deck run).** Order UNCHANGED: 1 ezEngine + Jolt, 2 Stride + Bepu, 3a Godot + PhysX fork, 3b Godot + Jolt. Reason: the ranking compares each engine's DEFAULT usage first (ezEngine: its own Jolt components; Godot: nodes; Stride: Bepu), and the deciding measure for a rendered 60 fps game is the frame with rendering. PROVED (stage D, `.tmp/comp-chart-1` and the 2026-11-08 runs): ezEngine is the only engine whose default path keeps the game-like mixed scene well inside the 60 fps budget with rendering (p99 8.6 / 8.9 ms, stress / game, against 19.3 ms or more for every other default path: Stride 24 to 39, Godot PhysX nodes 19.3 to 89.5, Godot Jolt nodes 408 to 447).
- What changed: ezEngine's lead is narrower. Per physics step its default path is no longer ahead of Godot PhysX (PhysX nodes are 14 to 28 % lower on pile and scale, 20 % higher in mixed), it is level with Godot Jolt on RIDs, still 1.2 to 1.6 times below node-based Godot Jolt, and 2.3 to 3.8 times above Stride's Bepu. On per-step physics alone Stride would be first and Godot PhysX ahead of ezEngine.
- The alternative paths: ezEngine direct Jolt is 10 to 40 % cheaper per step than its components and has the best rendered frame measured (p99 5.1 / 5.2 ms), so it is a measured fallback for physics hot spots. Godot on RIDs reaches 5.5 to 9.2 ms p99 with rendering, which would rank Godot above Stride on the rendered frame; this comparison uses Godot's alternative path, so it does not change the default-usage order.
- Update 2026-11-10 (section 12.6, section 13 item 30): the capacity (F) and debris (G) ladders were measured on the default path. Order UNCHANGED, provisional. Render-on capacity ezEngine 2500 / 2500 equals Godot PhysX nodes 2500 / 2500 and is above Stride 2000 / 2000 and Godot Jolt nodes 1500 / 1500; debris with rendering ezEngine 3.5 is the best of the default paths (Stride 3, PhysX nodes 3, Godot Jolt nodes 2.5); with rendering off Stride leads by far (8000 to 10000 bodies, debris 8 to 10) and ezEngine 3000 / 2000 against PhysX nodes 2500 / 2500, debris 3.5 against 5. The rendered mixed frame (p99 8.6 / 8.9 ms against 19.3 ms or more) stays the deciding criterion. PhysX nodes are one ladder stage above Stride with rendering (within the one-stage uncertainty) and Stride is far ahead without rendering, so this is no reason to swap places 2 and 3a. The default path is one step below direct Jolt on render-on stress (2500 against 3000) and physics (3000 against 3500 stress, 2000 against 3000 game), above it on render-on game (2500 against 2000) and on debris (3.5 against 3; direct Jolt never tested 3.5, so equal within ladder resolution).
- Limits: ladder values are uncertain by about one stage (earlier text: capacity and debris ladders on direct Jolt only, now superseded); one desktop; no Steam Deck; the components and direct Jolt re-measure are one session, 3 runs each. The rank 1 risks are unchanged (DXVK, MSAA-only renderer, serial transform update, small community). The final order is the user's decision.

What could change this verdict:
- (a) A Steam Deck run under Proton. It is the main open item: DXVK (ezEngine, Stride) against winevulkan (Godot), 4 Zen 2 cores, and an integrated GPU. Desktop ratios may not transfer.
- (b) A Godot variant with RID bodies and MultiMesh. Now tested (section 12.3). The earlier statement here, that point 2 suggested it would not close the gap, was a hypothesis; it was refuted for the rendered whole frame and partly refuted for physics. Still unmeasured: a function-level Jolt profile (needs a symbolised Godot build), `chain_check` of the RID chains against the node chains, and any Steam Deck run of the RID variant.
- (c) A profile of Stride's render-mode cost. If it is fixable, Stride's physics lead would count fully.
- (d) The real physics rate of Quest. At 60 Hz every variant has 3.3 times more headroom (HYPOTHESIS, above).

**Note on the Godot rows (added 2026-11-08, after section 12.3).** The Godot rows above, and the ranking built on them, are node-based Godot (`RigidBody3D` + `MeshInstance3D`). They depend on the body API. On RIDs + MultiMesh the rendered whole-frame result of Godot beats Stride in the mixed scene (217 fps with Jolt and 262 fps with the PhysX fork against 54 and 51 for Stride; ezEngine direct Jolt stays ahead at 299, ezEngine's default path runs 222), while Stride keeps the cheapest physics per step (1.02 ms against 3.14 ms for Godot Jolt on RIDs, pit-2000). The ranking above is NOT changed here: the final order, and whether Quest would accept a design that drives the physics server directly, is the user's decision.

### 12.3 Body API test: nodes vs PhysicsServer RIDs + MultiMesh (2026-11-08)

**Method.** New option `--body-api=nodes|rid` in the Godot benchmark (`benchmark/godot/src/quest_bench.cpp`; `BODY_API=rid` in `run-all.sh`). `rid` creates dynamic bodies and chain pin joints directly on `PhysicsServer3D` (no `RigidBody3D` / `PinJoint3D` nodes, no state-sync callback). Render mode draws them with one MultiMesh per shape kind (box, sphere, capsule), filled once per frame from `body_get_state` + `multimesh_set_buffer`. Statics (pit walls, ceiling) stay nodes. Scenario G (debris) is not supported in rid mode. Pin joint parameters are the `PinJoint3D` defaults (bias 0.3, damping 1.0, impulse clamp 0, solver priority 1, collisions between the bodies off).

**Setup.** Stress profile (sleeping off), 3 runs per cell, nodes and rid alternated per cell, idle desktop, one machine, Windows, discrete GPU. Cells: `unlock-physics-pit-2000` and `unlock-physics-mixed` (5 s warm-up + 20 s), scale 1000 / 2000 / 4000 with `--unpaced=1` (5 + 10 s), `mixed` render and `unlock-render-mixed` render (5 + 20 s). Ms per step = 1000 / steps per s (unpaced, rendering off). In every run all bodies were awake (`awake_bodies` = N) and 0 bodies were outside the pit. Spread of a cell between runs: 3 % or less, except PhysX nodes `unlock-render-mixed` run 1 (14.1 ms median, 100 ms p99; runs 2 and 3: 10.5 and 65). Builds: Jolt rows = stock Godot 4.7.2 release export; PhysX rows = Godot PhysX fork 4.7.3rc at fork head 97bc4de, CPU-only, jointed solver iterations 8/2 set in `override.cfg`. Engine in use proved by the class of the direct space state in env.txt (`JoltPhysicsDirectSpaceState3D`, `GodotPhysXDirectSpaceState3D`). Data: local gitignored folders `.tmp/results-rid`, `.tmp/results-rid-physx` (first set, stress), `.tmp/results-rid-abde` (second set: stages A, B, D, E, both profiles) and `.tmp/results-rid-cap` (stage F ladders, both profiles), `.tmp/results-rid-g` and `.tmp/results-rid-g-physx` (stage G, game profile, one run per rate), `.tmp/results-rid-c` and `.tmp/results-rid-c-physx` (stage C chains), not committed.

**Table C: Godot body API, stress profile** (PROVED, medians of 3 runs).

| Cell | Jolt nodes | Jolt RID | PhysX nodes | PhysX RID | ezEngine direct Jolt | Stride JIT | Stride AOT |
|---|---|---|---|---|---|---|---|
| Pit 2000: ms per step | 3.87 | 3.14 | 2.23 | 2.18 | 2.82 | 1.02 | 1.04 |
| Mixed: ms per step | 4.92 | 3.50 | 3.72 | 3.30 | 2.47 | 1.25 | 1.33 |
| 1000 bodies: ms per step | 1.84 | 1.51 | 1.155 | 1.145 | 1.158 | 0.59 | 0.62 |
| 2000 bodies: ms per step | 4.0 | 3.15 | 2.33 | 2.26 | 2.68 | 1.02 | 1.04 |
| 4000 bodies: ms per step | 9.2 | 6.26 | 6.12 | 4.73 | 5.49 | 1.69 | 1.71 |
| Mixed with rendering, render loop unlocked: fps | 2.8 | 217 | about 73 | about 262 | 299.1 | 53.5 | 50.6 |
| Mixed with rendering: p99 frame ms | about 405 | 5.6 | 62 to 74 | 5.6 | 5.1 | 25.0 | 25.0 |
| Most bodies at 20 ms p99, rendering on (stage F ladder, 2026-11-08; Jolt RID corrected 2026-11-10 from 2000, merge rule) | 1500 | 2500 | 2500 | 3000 | 3000 | 2000 | 2000 |
| Most bodies at 20 ms p99, rendering off (stage F ladder, 2026-11-08; PhysX RID corrected 2026-11-10 from 3000, merge rule) | 2000 | 3000 | 2500 | 3500 | 3500 | 8000 | 10000 |

Reference rows from the other engines (2026-11-07/08 runs, stress; section 12.2): ezEngine direct Jolt (raw path, re-measured 2026-11-09, section 13 items 25 and 26) pit 2.82, mixed 2.47, 4000 bodies 5.49, mixed render 299 fps with p99 5.1 ms (ezEngine default components path, 2026-11-09: pit 3.10, mixed 3.22, 4000 bodies 6.45, mixed render 222 fps with p99 8.6 ms; section 12.6); Stride JIT pit 1.02, mixed 1.25, 4000 bodies 1.69, mixed render 53.5 fps with p99 25 ms; Stride AOT 1.04 / 1.33 / 1.71 and 50.6 fps.

**Findings.**
1. PROVED (Table C): **the Godot node layer was a large part of Godot Jolt's cost.** RIDs save 18 to 32 % per step for Jolt (pit 19 %, mixed 29 %, 1000 bodies 18 %, 2000 bodies 21 %, 4000 bodies 32 %). For PhysX they save 1 to 3 % in the piles (pit, 1000, 2000), 11 % in mixed and 23 % at 4000 bodies.
2. PROVED: **the 12.2 hypothesis "RIDs would not close most of the gap" is REFUTED for rendering and PARTLY refuted for physics.** The whole-frame collapse of Godot Jolt on the mixed scene (2.8 fps) is the node + `MeshInstance3D` path, not Jolt: Godot Jolt on RIDs runs 217 fps and the PhysX fork 262 fps (ezEngine direct Jolt 299, ezEngine default 222, Stride 54). The p99 frame falls from about 405 ms to 5.6 ms for Jolt.
3. PROVED: **per step, the physics gap remains.** Jolt on RIDs 3.14 ms (pit) against ezEngine direct Jolt 2.82 ms (ezEngine default 3.10 ms, so Jolt RID is about equal to it) and Stride 1.02 ms (ezEngine corrected 2026-11-09 from 2.52 to 2.72, re-measured 2.82). PhysX on RIDs is faster than Jolt on RIDs in every physics cell, by 1.3 to 1.5 times.
4. HYPOTHESIS (not profiled): the 2.8 fps on nodes is specific to the Jolt module's node path, because the PhysX fork uses the same node layer and gets about 73 fps. No function-level profile exists (the export has no PDBs; it needs a symbolised Godot build).
5. PROVED (2026-11-08, stage C, chains-physics run 1, `.tmp/results-rid-c*` against the node runs in `.tmp/results-200hz-r2/merged`): the `chain_check` lines of the RID chains equal those of the node chains. Jolt: min_y 7.251, end_links_max_move 0.107, mean_dx -0.107 in both; PhysX fork: 7.255, 0.132, -0.068 in both. The first smoke that showed a different end_links_max_move was therefore not a lasting difference (its cause was not found). Median step in that run, nodes to RID: Jolt 0.90 to 0.54 ms, PhysX 0.60 to 0.48 ms (one run each, paced, so only a hint). Limits: one run per variant, only the chain_check summary was compared, not the per-link trajectories.
6. Note: PhysX nodes at 4000 bodies is 6.12 ms here against 5.30 ms in section 12.2. The builds differ (97bc4de here, c6f3b659 there); the cause was not checked. The Godot and ezEngine Jolt versions differ, and the PhysX fork is another Godot version than the stock export.

**Limits.** One desktop, stress profile, 3 runs per cell, desktop only (no Steam Deck, no Proton). RID mode means the game talks to the physics server directly; this is a game design choice, not a settings switch, and scenario G (runtime-created debris) was run in rid mode later, with one run per ladder cell (see the G row of Table B). Capacity ladders (most bodies at 20 ms, scale stage F, 1 run per level, 3 runs at the last pass and first fail, 2026-11-08) were run for rid in both profiles; the ladder steps differ from the node runs (coarser for Godot nodes), so capacities are only comparable to within one step. The RID F and G values were re-read with the `merge-results.pl` ladder rule on 2026-11-10 (section 13 item 29): the RID debris values are Jolt RID 3 / 3 and PhysX RID 7 / 8 (were 6 / 8), and the changed F cells are marked in Table C and Table B. Pit, mixed, unlock-render-mixed, raycast-10000 and the capacity ladders were run for rid in both profiles (game and stress); Table C and the first-set cells are stress only.

**Consequence for the verdict (not decided here).** The Godot rows depend on the body API; see the note after the verdict list in section 12.2. The final order is the user's decision.

### 12.4 Godot rendering driver: Vulkan against Direct3D 12 (2026-11-08)

Why: the Godot developers made D3D12 the default driver for NEW projects in 4.6 because Vulkan drivers on Windows are less reliable (PR GH-113213 and the 4.6 dev 5 announcement, read through a web search summary, not the pages; the talk the user remembered was not found in these docs). The engine default is still `vulkan` (`main/main.cpp:2389`); the editor only writes `d3d12` into new projects (`editor/editor_node.cpp:8351`). The QuestMark Godot project has no driver setting, so every Godot result before this section ran Vulkan (inferred from `benchmark/godot/NOTES.md` item 12; the older env files do not record the driver). Stride and ezEngine ran Direct3D 11 (section 3.1a). The new env.txt lines `rendering_driver_setting` and `rendering_driver_in_use` and the run-all variable `GODOT_RD_DRIVER` (with `BENCH_FLAVOUR`) exist since this test.

- PROVED (env.txt of 48 of 48 files: `rendering_driver_setting` and `rendering_driver_in_use` equal `vulkan` in the 24 Vulkan files and `d3d12` in the 24 D3D12 files; `rendering_method` forward_plus and `JoltPhysicsDirectSpaceState3D` in all 48; 0 failed cells): stages A and D, game and stress profiles, physics and render modes, 3 runs per cell, stock Godot with Jolt, one export, node body API, 200 Hz.
- PROVED (summary files): **the two drivers give the same result within the run spread in this scene set.**
  - The physics step (median of `physics_step_ms`, per run) is 1 to 2.5 percent lower on D3D12 in the pit scenes (pit-2000 game physics 3.75 against 3.81 ms; pit-2000 render 4.40 against 4.51 ms; stress pit-2000 render 4.43 against 4.52 ms). In the mixed scenes the steps are equal (3.83 to 3.87 against 3.81 to 3.85 ms in physics mode, 4.49 to 4.55 against 4.38 to 4.56 ms in render mode).
  - The mixed render scene collapses to the same 2.7 to 2.85 fps on both (tick 283 to 296 ms, p99 about 400 to 430 ms). **So the 2.8 fps of Godot on nodes is not a Vulkan driver effect:** it stays on D3D12. Section 12.2 item 4 (hypothesis: the node plus `MeshInstance3D` path) is consistent with this; no profile exists yet.
  - Frame rates in render mode are noisy: pit-2000 render fps is 55.6 to 65.3 (Vulkan) against 54.1 to 62.5 (D3D12) across three runs, and p99 frame is 60 to 93 ms against 65 to 94 ms. Mixed physics mode has a spread of 44 to 74 fps (D3D12) and 66 to 74 (Vulkan) with p99 185 to 283 ms against 196 to 217 ms, and stress mixed physics 52 to 84 (Vulkan) against 74 to 99 (D3D12). These spreads are larger than any difference between the drivers, so no driver ranking follows from the fps values.
  - Physics-mode pit scenes run at about 144.6 to 144.9 fps on both (identical; why was not checked, probably the window pacing).
- PROVED (same runs as section 3.1a and this section; medians of 3 runs per cell, stages A and D, one desktop, native Windows): the two renderer changes are NOT the same.

  | Cell (median fps) | ezEngine DX11 | ezEngine Vulkan | Godot Vulkan | Godot D3D12 |
  |---|---|---|---|---|
  | pit-2000 physics, stress | 1223.6 | 702.1 (-43%) | 144.9 | 144.9 (0%) |
  | pit-2000 physics, game | 1178.8 | 728.9 (-38%) | 144.7 | 144.9 (0%) |
  | mixed render, stress | 288.3 | 259.3 (-10%) | 2.8 | 2.8 (0%) |
  | mixed render, game | 277.8 | 231.3 (-17%) | 2.8 | 2.8 (0%) |

  ezEngine lost frame rate in every cell on Vulkan; Godot did not change beyond its run spread. Charts: README, section "Renderer A/B". HYPOTHESIS (not profiled): ezEngine's Vulkan renderer is less mature (a maintainer wrote "vulkan performance is still lacking" in PR 2062), while Godot's two backends cost about the same at these loads. The Godot physics-mode fps of about 145 looks pacing-bound (identical on both drivers), so it says little about the driver. The ezEngine and Godot columns are not one ranking: compare each engine only with its own other renderer.
- HYPOTHESIS (not tested): the Steam Deck under Proton may behave differently: D3D12 there goes through vkd3d-proton, while Vulkan is a pass-through (section 3.2 table). Windows driver quality is a statement about desktop Windows GPU drivers.
- Limits: one desktop with a discrete GPU, native Windows, stock Godot with Jolt only (the PhysX fork was not run on D3D12), stages A and D only (no ray, chain, scale or debris scenes), nodes only (no RID), unpaced render loop. A benchmark runner executed read-only listing commands for a few seconds at the start of the second run (reported by the runner). Conclusion scope: for these scenes, the choice of Vulkan against D3D12 does not move the Godot numbers measurably, so the ranking in section 1 is unchanged and the earlier Godot results ran on a driver that gives the same numbers. It says nothing about stability, shader compile hitches or crash rate of either driver, which were not measured.

### 12.5 Background and light parity: effect on performance (2026-11-09)

Why: Godot sets an explicit cornflower blue background; ezEngine and Stride ran their stock pipeline clear colour (ezEngine near-black blue), and the sun intensities were not fitted equally. The options `--background=default|cornflower` (ezEngine and Stride) and the fitted sun values were added (`benchmark/SPEC.md`, parity tables) and one scene was timed with the options off and on.

- PROVED (window captures, one shot per value, mean Rec.709 luminance of content pixels; Godot reference 184.90): ezEngine sun 5.8 with cornflower gives 184.88 (default 2.0 gives 138.90); Stride sun 0.375 gives 184.03 (default 0.55 gives about 197, so the earlier Stride fit to Godot no longer reproduces; HYPOTHESIS: the pit layout changed after that fit). Corner pixel of the background: Godot (100,149,237), Stride (100,149,237) both off and on (its default was already that blue), ezEngine (100,150,238) with the option and (2,14,25) without.
- PROVED (env.txt `background` and light lines in every file of the four sets): off = `default` with sun 2.0 (ezEngine) or 0.55 (Stride); on = `cornflower` with sun 5.8 (ezEngine) or 0.375 (Stride). 48 of 48 runs, 0 failed cells. Scene: pit-2000 (stage A), physics and render mode, game and stress profile, 3 runs per cell, same exe per engine (ezEngine Shipping D3D11, Stride JIT), medians below.

| Cell (median fps, runs low/mid/high) | ezEngine off | ezEngine on | Stride off | Stride on |
|---|---|---|---|---|
| stress, render | 352.3 (352/352/369) | 365.2 (365/365/369) | 114.4 (114/114/115) | 114.4 (114/114/116) |
| game, render | 364.8 (362/365/365) | 348.1 (347/348/367) | 126.5 (116/126/128) | 125.9 (126/126/126) |
| stress, physics | 1358.2 (1357/1358/1379) | 1344.5 (1273/1344/1369) | 1801.1 (1758/1801/1845) | 1951.2 (1909/1951/2227) |
| game, physics | 1314.5 (1228/1314/1327) | 1307.7 (1242/1308/1352) | 2490.8 (2468/2491/3000) | 2632.2 (2372/2632/2687) |

- Result: **no performance change that stands out from the run spread.** In the render cells the ezEngine difference changes sign (+3.7 percent stress, -4.6 percent game) and lies inside the spread of the runs; Stride render is equal (114.4 against 114.4, 126.5 against 125.9). Physics step medians in the stress profile are equal (ezEngine 2.54 against 2.54 ms, Stride 1.30 against 1.29 ms) and p99 frame times are equal within about 0.1 ms except Stride game render (12.4 against 19.2 ms, one run; the game-profile pile sleeps and its numbers move with the sleep time, not compared).
- Limits: one scene, one desktop, native Windows, off before on within each engine (drift would show as a difference), 3 runs per cell, fps is the unpaced loop. Stride game-profile step medians (1.17 against 0.48 ms) differ because the pile is asleep (awake 0 or 1 of 2000 at the end); they are not a background effect and were not investigated. Bloom of the stock ezEngine pipeline is still on and the vignette is off in cornflower mode, so the ezEngine look is close, not identical. In ezEngine captures the right interior pit wall is not visible while Godot shows it (HYPOTHESIS: face culling or winding of the wall boxes; not investigated). The ranking in section 1 is unchanged; the existing results were taken with the defaults and stay valid.

### 12.6 ezEngine through its own physics components, all stages (2026-11-09)

Naming (user decision 2026-11-09): "raw" = "ezEngine direct Jolt" (alternative path), "components" = "ezEngine" (default usage). Older text in this section keeps "raw" and "components".

Why: the raw ezEngine path drives Jolt's `BodyInterface` itself and so skips the engine's physics integration (actor, shape and constraint components, `ezJoltWorldModule`). The components path (`--physics-path=components`, run with `EZ_PHYSICS_PATH=components`, the default since 2026-11-09; the 2026-11-09 runs used `BENCH_FLAVOUR=components`) uses the engine's own components for every body, constraint and debris piece. It was made as fair as the other tests without changing them or the raw path; the design is in `benchmark/ezengine/NOTES.md` (component section).

- PROVED (code read plus smoke and full runs): per owed 200 Hz step the benchmark work runs first (kicks, rays, debris spawn), then one `ezWorld::Update` makes exactly one fixed step; the step cap is 64 like the other engines. The module merges steps into one `PhysicsSystem::Update(N*dt, N)`; Jolt's job pool holds 2048 jobs and a full pool makes `ezJoltJobSystem::CreateJob` spin forever, which was the stall (six timed-out cells in the first full run). One step per update removes it. The component `AddAngularImpulse` is a torque applied over all merged steps, so the spin is set with `SetAngularVelocity`.
- PROVED (impulse probe, debris rate 3, one run each, same seed): linear speed after the next step 1.7544 m/s (components, created), 1.7581 (components, pooled), 1.7544 (raw); spin 2.7721 rad/s set and read back. Limit: one piece per run.
- Full run, `.tmp/comp-all5`, old per-step app-frame accounting (SUPERSEDED for the unpaced E cells by the slice-accounting re-measure below; kept as history) (126 cells, 0 failed, 54 min, Shipping D3D11, idle machine, one machine, 3 runs per cell in A to E, mostly 1 run in F and G, batch drift about 5 %):

  | Cell (game profile unless noted) | components | raw reference (Table A and F/G tables, 2026-11-07/08) |
  |---|---|---|
  | A to D paced real-time factor | 1.0 (raycast-10000: 0.909) | 1.0 |
  | pit-2000 per step on tick frames | 3.07 ms | not same-session |
  | mixed per step on tick frames | 2.74 ms | not same-session |
  | E pit-2000 unpaced wall ms per step | 3.65 (stress 3.32) | 2.74 (stress 2.72) |
  | E mixed unpaced wall ms per step | 3.67 (stress 3.60) | 2.30 (stress 2.41) |
  | F physics last pass at 20 ms | 2000 (stress 3000) | 3000 (stress 3500) |
  | F render last pass | 2500 | 3000 (stress) |
  | G debris highest passing rate | 3.5 | 3 |

  Pooled debris spawn costs about 0.04 ms per event against 0.19 to 0.28 ms for created pieces. The G ladder here also tested 3.5; the raw ladder did not.
- Result (SUPERSEDED, old accounting; see the re-measure in the next bullet): through its own components ezEngine is about 30 to 40 percent slower than raw per unpaced step on pit-2000 and about 55 to 60 percent on mixed (corrected 2026-11-09 from 60 to 70 percent; against the corrected raw values 3.65 / 2.74 = +33 % on pit-2000 game and 3.67 / 2.30 = +60 % on mixed game, with batch drift about 5 percent; the interleaved like-for-like figures below are the firmer ones), and the scale ladder passes one step lower.
- PROVED (re-measure 2026-11-09 evening, 23:01 to 23:24, idle machine, same Shipping D3D11 exe, ezEngine commit 100564a, benchmark code = the uncommitted working tree with `--comp-unpaced=slice` as default; output `.tmp/comp-chart-1`: components stage E, raw stage E and the direct scale, mixed-render and raycast cells; 3 runs per cell, median, ms per step = 1000 / `steps_per_s_window`; 61 cells, all exit code 0; smoke run `.tmp/smoke-slice-1` passed every expected check: `comp_unpaced: slice`, pacing overhead above 0, `steps_per_s_window` below `steps_per_s`, equal `jolt_module_steps`, `physics_delta` 0.005, phase sums equal `frame_total`, `chain_peak` off, correct env lines). **With the slice accounting (one app frame per 10 ms slice, its overhead counted as in the raw path) the components path costs +10 to 15 % on pit-2000 and +31 to 40 % on mixed over raw**, raw and components from the same session:

  | Cell | components | raw (same run) | components against raw |
  |---|---|---|---|
  | pit-2000 stress, ms per step | 3.10 | 2.82 | +10.0 % |
  | pit-2000 game | 3.20 | 2.78 | +15.0 % |
  | mixed stress | 3.22 | 2.47 | +30.7 % |
  | mixed game | 3.27 | 2.33 | +40.3 % |
  | scale 1000 stress | 1.366 | 1.158 | +18.0 % |
  | scale 4000 stress | 6.45 | 5.49 | +17.5 % |
  | unlock-render-mixed fps, stress / game | 222.4 / 216.2 | 297.6 / 286 | -25 % / -24 % |
  | unlock-render-mixed p99 wall ms, stress / game | 8.82 / 9.08 | 5.19 / 5.37 | |
  | mixed render mode (paced, stage D) p99 ms, stress / game | 8.55 / 8.90 | 5.1 / 5.2 (2026-11-08 r2, not the same session) | |
  | raycast-10000 render, game, median wall ms per frame | 353.4 | 345 (r2, not the same session) | |

  Awake bodies at the end, pit game and mixed game: components 1861 and 2923, raw 1896 and 1902. The raw ezEngine values in this table replace the corrected r2 values in Table A (see section 13 item 26). Attribution: the old 30 to 60 % gap was larger than this one. That the old per-step app-frame accounting made up part of it is PROVED earlier in this section (pit-2000 game +41 % became +31 % on the same runs, and the phase timing found about 0.2 ms per step from the whole app frame per step). That it explains the whole drop from 30 to 60 % to 10 to 40 % is a HYPOTHESIS: the old numbers are from another session (`.tmp/comp-all5`) and another build, and the old mixed cells were never recomputed on the same session. Limits: 3 runs per cell, one desktop, one session, no Steam Deck. Components mixed stress run 2 was 11 to 13 % slower than runs 1 and 3. The benchmark code that built the exe is uncommitted. The old `comp-all5` rows above stay as history.
- HYPOTHESIS (cause of the gap, not measured): per-frame `ezWorld::Update` work beyond the Jolt step (transform sync, debug-info rendering), one step per frame in the unpaced loop, and the compound shape per actor. The earlier interleaved physics-only A/B on pit-2000 found only +5.1 percent (of which +1.7 from group filter and compound shape), so the whole-loop gap above is larger than that A/B explains. An interleaved raw-against-components run of pit-2000 and mixed is open.
- PROVED (interleaved A/B 2026-11-09, `.tmp/ab-r1-raw` then `.tmp/ab-r1-comp`, same session and exe, Shipping D3D11, stages A, C, D, E, 1 run per cell, 0 failed cells, one machine; with 1 run per cell differences under about 5 percent mean nothing): unpaced wall ms per step, raw against components: pit-2000 game 2.692 / 3.801 (+41 %), stress 2.617 / 3.861 (+48 %); mixed game 2.215 / 3.878 (+75 %), stress 2.333 / 3.902 (+67 %). So the gap is real and not batch drift. Unpaced render mixed: 271.3 against 209.1 fps (game), 268.3 against 218.1 (stress). Paced render mode wall median: pit-2000 game 4.00 / 4.29 ms (+7 %), stress 3.92 / 4.32 (+10 %), mixed game 4.30 / 4.93 (+15 %), stress 4.08 / 4.76 (+17 %). Chains physics p99 wall 0.80 / 1.01 ms.
- PROVED (same runs, unlock-physics-pit-2000 game): the physics column (tick-frame `physics_ms`) is 2.629 ms raw against 3.061 components (+16 %), and the median frame is 2.629 ms raw (all of it physics) against 3.604 components, so about 0.54 ms per frame in the components loop lies outside the timed `ezWorld::Update`. The cause of that outside time is a HYPOTHESIS (not measured; candidates: work around the update in the unpaced loop, present, flush). The physics part alone (+16 %) is larger than the +5 % of the earlier render-mode A/B, which measured a different mode.
- PROVED chains kick response (same runs, chains physics, `chain_check`): end_links_max_move 0.102 m raw, 0.038 m components. The gap of the earlier run persists on the new code. SUPERSEDED by the "chains response is the same in both paths" item below (same swing, end snapshot taken 56 steps earlier on the component path; checked again 2026-11-10 by a code read of both paths). Old text: cause UNPROVEN (HYPOTHESES: no ceiling body in the component path, compound wrapper, constraint solve differences); limit: 1 run per path, deterministic kick so run spread is probably small but was not measured.
- CORRECTION (2026-11-09, deep-analyst read of the code, arithmetic checked on pit-2000 game against the run summaries): the "+41 %" style unpaced figures above are not like for like. The raw unpaced row time (`steps_per_s`, `wall ms per step`) counts only the benchmark's own work and the Jolt step; the app frame (message pump, extraction, render of the empty pipeline, present) is paid once per 10 ms slice and is kept outside the rows (`m_fUnpacedOverheadMs`, QbLoop.cpp:182-196). In the components unpaced loop every step is a full app frame, so its rows include that cost. PROVED for pit-2000 game: raw 6883 steps in a 20 s window = 2.905 ms per step (measured_s 18.53 plus about 1.46 s overhead, the overhead is logged in the summary `pacing:` line), components 5263 steps in 20.002 s = 3.801 ms, so +31 %, not +41 %. The other cells (mixed, stress) were NOT recomputed and probably shrink the same way; the earlier raw-versus-components statements in this section and the raw reference numbers (2.54, 2.13 ms) share this accounting difference. The about 0.54 ms per frame outside the timed update is this app-frame cost (PROVED by code read: `physics_ms` covers `ezWorld::Update` plus `RenderWorldDebugInfos` only). The +16 % inside the update has a PROVED mechanism candidate: in components the Jolt step runs inside the Simulate task on an ez worker and the main thread only waits (it takes no Jolt jobs), while in raw the main thread runs jobs at the barrier, so one thread fewer works on Jolt (HYPOTHESIS for its size: up to 12.5 % on parallel phases; test: run the raw `Update` inside a task and wait). Group filter and compound wrapper add +1.7 % (PROVED earlier). Chains: the kick, damping, mass and constraints are equivalent in code (PROVED by read); `chain_check` is one snapshot at the end of the run, not a peak; components skip about 3 app frames before the kick, and raw gives exactly 0.102 m in physics and render mode while components gives 0.038 and 0.034, so the 0.102 against 0.038 gap is most likely a phase artefact of a damped swing (HYPOTHESIS, not tested; test: raw chains with warm-ups of 5.0 to 5.3 s, and a peak displacement metric).
- PROVED like-for-like unpaced cost per step (`.tmp/ab-r1-raw` and `.tmp/ab-r1-comp`, replicate 1, unlock-physics cells; raw = (measured_s + overhead logged in the summary `pacing:` line) / steps, window 20 s checked: raw measured_s plus overhead = 19.99 s in all four cells; components = measured_s / steps): pit-2000 game raw 2.905 ms against components 3.801 (+31 %); mixed game 2.376 / 3.878 (+63 %); pit-2000 stress 2.833 / 3.861 (+36 %); mixed stress 2.528 / 3.902 (+54 %). The raw overhead is 1.35 to 1.54 s of the window (about 7 %). Unlock-render-mixed fps are already whole-frame: 271.3 / 209.1 (game), 268.3 / 218.1 (stress). Limits: 1 run per cell, one machine. These replace the "+41 / +48 / +75 / +67 %" figures above; the raw reference numbers in the older tables (2.54, 2.13 ms) used the uncorrected accounting. They are corrected now (next bullet).
- PROVED (2026-11-09, recomputed by the main session from `.tmp/results-200hz-r2/ezengine/*.summary.txt`, confirmed by an independent review): **the published ezEngine raw-path unpaced ms per step were 7 to 9 % too low.** ezEngine's `measured_s` counted only the step work (TickWork plus the Jolt step, `m_fCumMs`); the app frame between its 10 ms step slices (window pump, world update) is logged on the summary `pacing:` line ("N ms in the window") and was left out of `steps_per_s`. Godot's measured seconds cover its whole loop window and Stride's run from the first to the last row, so only ezEngine was low. Like for like = (measured_s + overhead_s) / steps; the ezEngine build prints it as `steps_per_s_window` (new summary line, code not yet committed at the time of writing). The raw bars in Tables A and C, section 12.3 and this section were corrected: pit-2000 stress 2.52 to 2.72 (median of 3: 2.691 / 2.837 / 2.721), mixed stress 2.23 to 2.41, scale 1000 / 2000 / 4000 stress 1.06 / 2.50 / 5.05 to 1.136 / 2.68 / 5.47, pit-2000 game 2.54 to 2.74, mixed game 2.13 to 2.30 (medians of 3 where not noted), scale game 1000 / 2000 / 4000 0.98 / 2.56 / 5.35 to 1.06 / 2.76 / 5.82 (single runs). Not affected: other engines' values and all render-mode values (fps, p99, raycast, capacity, debris). Scope: ezEngine raw path (Jolt), `results-200hz-r2`, desktop, one machine. The correction narrows ezEngine's per-step lead (Godot Jolt over ezEngine now 1.45 to 2.2 times, Godot PhysX nodes below ezEngine on the pile); the ranking in section 1 and 12.2 is unchanged and the final order is the user's decision. UPDATE 2026-11-09 evening: the re-measurement was run (see the re-measure bullet after the Result bullet above and section 13 item 26); it reproduced the corrected values within 0.4 to 3.7 % (PROVED), so this correction stands. The values quoted in this bullet are the corrected-only ones and are kept as history.
- PROVED chains response is the same in both paths (interleaved A/B `.tmp/ab2`, 2026-11-09, Shipping D3D11, chains physics game, 3 rounds, raw / raw-update-in-task / components, one machine; new metrics `chain_peak` and `chain_fixed_step`, sampled once per physics step before the step in both paths; since then opt-in with `--chain-sample=1` (default off), because the sampling runs inside the timed step work of both paths): the peak end-link move is 0.535 m at step 561 after the kick (2.805 s) in all 9 runs, and the move 400 steps (2.000 s) after the kick is 0.093 m in all 9 runs. The old end-of-run snapshot differs only because the runs stop at different step counts: raw reports at step 4999 or 5000 (0.102 or 0.103 m), components at step 4944 (0.038 m), 56 steps (0.28 s) earlier, at another phase of the damped swing. A raw-only warm-up test (warm-up 5.0 / 5.1 / 5.2 / 5.3 s, `.tmp/ab-chains-warm`) gave 0.069 / 0.120 / 0.132 / 0.139 m, so the snapshot moves by that much within raw alone. The earlier "0.03 m against 0.10 m" was a measurement artefact, not a physical difference. Limits: 3 runs per path, the chains are deterministic here (identical values to 3 decimals), game profile only.
- PROVED thread-count test (same A/B, unlock-physics-pit-2000 game, 3 rounds, rotating order; steps in the 20 s window: raw 7000 / 7262 / 6970, raw with `--raw-update-in-task=1` 6826 / 6615 / 6856, components 5793 / 5702 / 5974; mean ms per step like for like, window / steps: raw 2.83, raw in task 2.96 (+4.6 %), components 3.44 (+21.6 % over raw); median timed physics step: raw 2.58, raw in task 2.72 (+5.7 %), components 2.93 (+13.7 %)). So the main thread not taking Jolt jobs explains about 5 of the 14 points inside the update (HYPOTHESIS for the mechanism, PROVED for the size of the effect of running the step in a task), and the rest of the timed update (+7.6 % over raw in task) and about 0.5 ms per step outside it (raw 0.25 ms per step of app-frame overhead) are still unexplained. The components ms per step was 3.80 in the earlier A/B and 3.44 here: batch drift or the rebuilt exe, UNPROVEN, so only compare inside one A/B. Limits: pit-2000 game only, 3 runs per path.
- PROVED phase timing (`--phase-timing=1`, `.tmp/ab3`, 2026-11-09, idle machine confirmed by the user, Shipping D3D11, unlock-physics-pit-2000 game, 3 rounds, raw and components each with the option off and on, rotating order, one machine): the 25 chained segments add up to `frame_total` and the marker hits equal the frame count in all 6 on-runs checked (r1 of both paths read in full: raw 1693 frames / 7254 steps, components 5972 / 5972). Stopwatch cost is not detectable: steps in 20 s, raw off 7182 / 6968 / 7323 against on 7257 / 7320 / 7341 (+2 %), components off 5750 / 5842 / 5985 against on 5973 / 5819 / 5618 (-1 %), both inside the 5 % batch drift. Per step, round 1 (the other rounds differ by 1 to 4 %, one outlier: `finish_frame` 0.36 ms in comp-on r3): raw 2.756 ms = Jolt step 2.559 + app frame 0.197 (finish_frame 0.133, render 0.036, extract 0.005, rest small); components 3.348 ms = wait for the Jolt task in `upd_jolt_fetch` 2.908 + `upd_transform` 0.038 + app frame about 0.40 (finish_frame 0.138, render 0.153, beginframe 0.052, extract 0.021, pump 0.019, rest small). Gap of 0.59 ms per step: about 0.35 ms (+14 %) inside Jolt wait against raw step, about 0.04 ms transform pass, about 0.2 ms because components pays the whole app frame each step while raw pays it once per 10 ms slice (render 0.153 against 0.036 per step is that slice amortisation). The `UpdateDynamicActors` / `UpdateGlobalTransforms` hypothesis (0.2 ms) is NOT supported: the transform segment is only 0.04 ms. HYPOTHESIS, not measured: the 0.35 ms inside the fetch wait is the Jolt step running with one thread fewer (see thread-count test, +5 % of it) plus the actor sync inside the task; the fetch segment does not separate them. Limits: pit-2000 game only, 3 runs per cell, only round 1 read per segment.
- PROVED (ladders on the components path, 2026-11-10, 09:29 to 09:54, `.tmp/comp-ladders-1`, `run-all.sh ezengine --stages=FG`; env.txt `physics_path: components` in every cell; Shipping D3D11 exe, sun 2.0, background default, idle machine, 0 failed cells; RUNS=3 at the last pass and first fail, other ladder stages 1 run; values by the `merge-results.pl` ladder rule: highest tested stage whose median passes 20 ms p99 and the real-time rule, midpoints included):

  | Ladder (ezEngine, components) | game | stress | evidence |
  |---|---|---|---|
  | F render (rendering on), most bodies at 20 ms | 2500 | 2500 | 2500 is a single run, p99 11.5 / 10.3 ms; 3000 fails, p99 42 to 267 ms |
  | F physics (rendering off) | 2000 | 3000 | game: 3000 fails (3 runs p99 20.1 / 35.5 / 23.6 ms, median 23.6); stress: 3 runs p99 17.1 to 18.4 ms at 3000, 3500 fails |
  | G debris, highest rate (events per s) | 3.5 render, 3.5 physics | n/a (G is a game-profile scenario) | 3.5 single run, p99 14.4 / 6.1 ms; 4 fails in 3 runs |

  So the capacity and debris ladders of "ezEngine" are now measured; they were "not measured" in earlier text. They match the old `comp-all5` ladder rows above (2000 / 3000, 2500, 3.5), which are no longer the only source. Against ezEngine direct Jolt (2026-11-08, Table B): render-on stress 2500 against 3000 and physics 3000 against 3500 (stress), 2000 against 3000 (game), one ladder step below; render-on game 2500 against 2000, above; debris 3.5 against 3, equal within ladder resolution because direct Jolt never tested 3.5. Limits: ladder steps coarse, so a value is uncertain by about one stage; the other stages ran once; one desktop, one session, no Steam Deck; the direct Jolt and other-engine ladders are from earlier sessions.
- PROVED (pit-drift settle, `.tmp/pitdrift-1`, 2026-11-10, same exe, unlock-physics-pit-2000 game, 3 rounds interleaved in rotating order, 5 s warm-up + 20 s window, ms per step = 1000 / `steps_per_s_window`): medians raw 2.87 ms (range 2.76 to 3.01), components slice accounting 3.17 (3.15 to 3.25), components frame accounting 3.71 (3.60 to 3.74). The slice value reproduces the published 3.20 within 1 %. The frame-to-slice gap is about 0.54 ms (15 %): that is the accounting change, so the drift of the components pit value from 3.80 (frame accounting, older exe, `.tmp/ab-r1-comp`) to 3.20 (slice accounting) is mostly accounting. Awake at the end: raw about 1894, components about 1860 to 1889. STILL UNEXPLAINED: the 3.44 ms of the `.tmp/ab2` frame-mode run is about 7 % below today's frame median 3.71 (one run per variant there; batch drift or the rebuilt exe, UNPROVEN). Limits: pit-2000 game only, 3 rounds, one desktop. The raw-against-components gap in the same run: 3.17 against 2.87 ms is +10 % with slice accounting.
- Limits: the raw numbers are from earlier batches (batch drift about 5 percent), ladder cells of the `comp-all5` run are single runs, the awake counts differ between paths, `physics_ms` of this path includes the whole world update, and the chain kick response (0.03 m here against 0.10 m raw in one earlier run) was not re-measured. The "+10 percent" first reported for pit-2000 was batch drift (section 13).
- The first idle full run of this code lost its CSVs; the documented set is the second complete run (kept under `.tmp/`, not committed). Its numbers agreed with the first within the drift.

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
21. **Ranking changed by the desktop benchmark (2026-11-08).** The study-only ranking of 2026-11-06 was 1 Godot + Jolt, 2 Stride, 3 ezEngine. It assumed that Godot's CPU problems have workarounds and that its renderer and agent knowledge outweigh them. The benchmark (section 12.2) measured Godot + Jolt last in every physics and whole-frame category, with the same Jolt 1.6 to 2.4 times dearer per step than in ezEngine. New ranking: 1 ezEngine + Jolt, 2 Stride + Bepu, 3 Godot + Jolt. It is provisional until a Steam Deck run. The RID/MultiMesh workaround for Godot was not tested.
22. **ezEngine Vulkan (2026-11-08).** The doc said Vulkan was unfinished and DX11 the only complete renderer (README and project site). Build files and PR history show Vulkan built by default and D3D11 opt-in; Vulkan maturity was not read and no Vulkan timing existed. Section 3.1a holds the evidence and the pending A/B; the rows in sections 2, 3.1, 3.3, 4.2 and 8 were updated. The benchmarked variant remains D3D11.
23. **Godot rendering driver (2026-11-08).** The doc said Godot `driver.windows` defaults to Vulkan, which hid that new projects since 4.6 get D3D12 and that our benchmark ran Vulkan. Fix: the Vulkan row of the Proton API table (near the top) and section 12.4, which measures both drivers (no measurable difference in stages A and D).

24. **ezEngine component path (2026-11-09).** A first "+10 percent" for the component path on pit-2000 was batch drift (the raw baseline moved about 5 percent between batches); the interleaved A/B gave +5.1 percent. A claim that the component path hit a right-wall limit was withdrawn: the stall was Jolt's job pool (2048 jobs) running dry when the module merges many steps into one update, and one step per world update removes it. The component `AddAngularImpulse` is a torque over merged steps, not an impulse, so the spin is set directly. See section 12.6.
25. **ezEngine raw unpaced ms per step were 7 to 9 % too low (2026-11-09).** The old `steps_per_s` of the ezEngine raw path left out the app frame between its 10 ms step slices (logged on the summary `pacing:` line), while Godot and Stride were counted over their whole loop window or row span. Like-for-like values: pit-2000 stress 2.52 to 2.72, mixed stress 2.23 to 2.41, scale 1000 / 2000 / 4000 1.06 / 2.50 / 5.05 to 1.136 / 2.68 / 5.47, pit-2000 game 2.54 to 2.74, mixed game 2.13 to 2.30. Statements that changed: Godot Jolt against ezEngine 1.6 to 2.4 times is now 1.45 to 2.2; "PhysX fork reaches about ezEngine's cost" is now "at or below it on the pile, at 2000 and at 4000 bodies"; Bepu against ezEngine 2.0 to 2.5 is now about 2.7. Not changed: the ranking (the final order is the user's decision), other engines, render-mode values. Item 21 above quotes the old 1.6 to 2.4 as it was written then. Section 12.2 table A, 12.3 table C and 12.6.
26. **ezEngine raw cells re-measured, components path measured with slice accounting (2026-11-09 evening).** PROVED (`.tmp/comp-chart-1`, idle machine, same Shipping D3D11 exe, 3 runs per cell, median, 1000 / `steps_per_s_window`, 61 cells all exit code 0): the raw values of item 25 were confirmed within 0.4 to 3.7 % (inside run spread), so the 7 to 9 % correction stands and the old uncorrected values stay wrong. New values: pit-2000 stress 2.72 to 2.82, mixed stress 2.41 to 2.47, scale 1000 stress 1.136 to 1.158, scale 4000 stress 5.47 to 5.49, pit-2000 game 2.74 to 2.78, mixed game 2.30 to 2.33. Not re-measured, item 25 values kept: scale 2000 stress 2.68, scale game 1.06 / 2.76 / 5.82. Statements that changed: Godot Jolt against ezEngine 1.45 to 2.2 is now 1.4 to 2.2; PhysX fork nodes against ezEngine on the pile "about 18 % lower" is now 20.6 % lower (2.24 against 2.82), 4000 bodies 3.5 % lower (5.30 against 5.49, the old README wording "slower" had the wrong direction), 1000 bodies still a near tie; Bepu against ezEngine 2.7 is now 2.8 times. The components path with slice accounting costs +10 to 15 % on pit-2000 and +31 to 40 % on mixed over raw in the same session (section 12.6); this replaces the "30 to 40 percent and 55 to 60 percent" result of `.tmp/comp-all5` (old per-step app-frame accounting), whose rows stay as history. Items 21 and 25 quote the older figures as written then. Limits: one desktop, one session, no Steam Deck; raw pit-2000 stress had one outlier run (window overhead 3.6 s against about 1.5 s, spread 21.7 %, median used); components mixed stress run 2 was 11 to 13 % slower than runs 1 and 3; the benchmark code that built the exe is uncommitted. Not changed: the ranking (the final order is the user's decision), other engines, render-mode raw values (fps, p99).
27. **Naming change and verdict update (2026-11-09, user decision and main-session decision).** Using ezEngine through its own Jolt physics components is the default way to use the engine and is now called "ezEngine" (`--physics-path=components`); creating and stepping bodies through the Jolt BodyInterface directly is the alternative and is called "ezEngine direct Jolt" (`--physics-path=raw`; the exe default was still raw when this item was written, it changed to components later on 2026-11-09, see item 28). Every ezEngine number measured before 2026-11-09 evening came from the direct Jolt path (stage A to G r2 run, ladders F and G, debris, raycast, renderer A/B, sleeping experiment), so those are now labelled "ezEngine direct Jolt" in sections 1, 12.2, 12.3 and the README; items 1 to 26 above keep the labels as written then (their "ezEngine" and "raw" mean direct Jolt). Statements that changed: "p99 5.2 ms against 19 ms or more for every other variant" is now the default-path figure (p99 8.6 / 8.9 ms against 19.3 ms or more for every other default path) plus the direct Jolt alternative (5.1 / 5.2); Godot Jolt against ezEngine "1.4 to 2.2 times" is now 1.2 to 1.6 times against the default path (1.4 to 2.2 against direct Jolt); Bepu against ezEngine "2.8 times" is now 2.3 to 3.8 times against the default path; PhysX nodes against ezEngine are 14 to 28 % lower on pile and scale and 20 % higher in mixed on the default path. Verdict: order unchanged (1 ezEngine + Jolt, 2 Stride + Bepu, 3a Godot + PhysX fork, 3b Godot + Jolt), lead narrower; ranked by default usage and the rendered frame (section 12.2, verdict update 2026-11-09). Limits: capacity and debris ladders on direct Jolt only; one desktop; no Steam Deck; components and direct Jolt re-measure are one session, 3 runs each. The final order is the user's decision.
28. **ezEngine exe default changed to components (2026-11-09, tooling).** `QbOptions.h` now defaults to `--physics-path=components`; items 26 and 27 and section 12.6 said the raw path was the exe default, which was true when they were written. `tools/run-all.sh` takes `EZ_PHYSICS_PATH=components|raw` (default components) and passes `--physics-path` to every cell; raw without `BENCH_FLAVOUR` writes `ezengine-jolt-direct-...`. File names: unflavoured `ezengine-jolt-...` files from 2026-11-09 on are components, older unflavoured files are raw, `ezengine-jolt-components-...` is components; env.txt `physics_path` proves which. No result changed.
29. **Ladder rule corrected for all series (2026-11-10).** PROVED (`benchmark/tools/run-all.sh` read, audit of all ladder logs, merge outputs `.tmp/ladder-audit/*.SUMMARY.md`): the `LADDER ... last pass 20ms=` line of the run-all logs drops a stage that passes 20 ms when that stage ran as the limit2 (looser-limit) midpoint, because the second midpoint block never updated `L20`; it also uses run 1 only, while `merge-results.pl` uses the median of 3. The audit showed the drop in many ladders of several engines. The published F values of the node and Stride series already followed the merge rule; the G row (whose 2026-11-08 "PROVED correction", in the text after Table B, moved to the log-line values) and the RID F and G values followed the log line. Rule now for ALL series: the `merge-results.pl` "last pass 20 ms" (highest tested stage whose median passes, midpoints included). The 2026-11-08 G correction was itself wrong: the earlier values were right for Stride JIT, Stride AOT and the PhysX fork physics cells. Values that changed (old to new, render / physics): G debris Stride JIT 3 / 6 to 3 / 8 (8 passed as a median of 3 and no higher rate was tested, so capacity may be higher), Stride AOT 3 / 8 to 3 / 10, Godot PhysX fork nodes 3 / 4 to 3 / 5 (`results-200hz`, build c6f3b659), Godot Jolt nodes 2 / 2 to 2.5 / 2.5 (`results-200hz-r2`), Godot PhysX RID 6 / 8 to 7 / 8; Jolt RID 3 / 3 and ezEngine direct Jolt 3 / 3 unchanged. F: Godot Jolt RID render-on game / stress 2000 / 2000 to 2500 / 2500, Jolt RID physics game 2000 to 3000 (stress 3000 unchanged), Godot PhysX RID physics game / stress 3000 / 3000 to 3500 / 3500; all other F values unchanged. Sources: node F and G from `.tmp/results-200hz` (PhysX fork, Stride) and `.tmp/results-200hz-r2` (Godot Jolt, ezEngine direct Jolt); RID from `.tmp/results-rid-cap`, `.tmp/results-rid-g`, `.tmp/results-rid-g-physx`. `run-all.sh` was fixed on 2026-11-10 (the limit2 midpoint now updates `L20` / `F20` when it lies between them); this only affects which stage gets 3 runs and the log line, and the fix was checked by dry runs only (not in a real run): with simulated capacities `SIM_PASS20=SIM_PASS33=3600` the F ladder now reports last pass 3500 (before: 3000), with 3200 first fail 3500, and with 2500 / G 3.5 the values 2500 / 3.5. The same tests found a second, older bug: `sim_pass` used integer arithmetic on decimal G sim limits (e.g. `SIM_G_PASS20=3.5`), the shell error skipped the dry-run block's `exit` and the estimate line then called `run-all.sh` again without end (hundreds of nested processes); fixed in `sim_pass`, and a guard now ends an aborted dry run with exit 1 instead of falling through to the real-run code. Statements that changed: Table B and Table C ladder cells, the G-row note, the README table rows and the PhysX RID debris text. Not changed: the ranking.
30. **ezEngine ladders on the components path, pit-drift settle, verdict confirmed (2026-11-10).** PROVED (section 12.6: `.tmp/comp-ladders-1`, `.tmp/pitdrift-1`): the capacity and debris ladders of "ezEngine" are measured (F render 2500 / 2500, F physics 2000 / 3000, G debris 3.5 / 3.5; game / stress for F), and the components pit-2000 drift 3.80, 3.44, 3.20 ms is mostly accounting (frame against slice, about 0.54 ms or 15 % in one interleaved run); the 3.44 ms `ab2` frame value stays unexplained. Statements that changed: "ladders measured on direct Jolt only (ezEngine default: not measured)" in section 1, section 12.2 (analysis items 1, 4, 7 and the verdict limits), item 27 above (quoted as written then) and the README. Verdict (main-session decision, 2026-11-10): ORDER UNCHANGED, 1 ezEngine + Jolt, 2 Stride + Bepu, 3a Godot + PhysX fork, 3b Godot + Jolt; still provisional (desktop only, no Steam Deck). Reason from the default paths: render-on capacity ezEngine 2500 / 2500 equals Godot PhysX nodes 2500 / 2500, above Stride 2000 / 2000 and Godot Jolt nodes 1500 / 1500; debris with rendering ezEngine 3.5 is the best of the default paths (Stride 3, PhysX nodes 3, Godot Jolt nodes 2.5); with rendering off Stride leads by far (8000 to 10000 bodies, debris 8 to 10), ezEngine 3000 / 2000 (stress / game) against PhysX nodes 2500 / 2500, debris 3.5 against 5. The deciding criterion stays the rendered mixed frame (p99 8.6 / 8.9 ms against 19.3 ms or more). Stride against the PhysX fork: PhysX nodes are one ladder stage above Stride with rendering (within the one-stage uncertainty), Stride is far ahead without rendering; that is no reason to swap places 2 and 3a. Limits: ladder values are uncertain by about one stage; one desktop; no Steam Deck. The final order is the user's decision.

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
