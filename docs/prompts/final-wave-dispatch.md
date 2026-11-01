# Despacho da onda de correção final (texto entregue ao subagente implementador)

Texto integral, sem alteração, da mensagem com que o coordenador despachou o subagente que
implementou a onda de correção final (commits `Revisão final, item N`). A lista de itens está em
`final-fixlist.md` (nesta pasta) e as instruções de ambiente em `env.md`.

---

You are implementing the final fix wave for "Rail Rush", a C++11 / OpenGL 3.3 endless-runner game for a university CG course (INF01047/UFRGS), after the whole-branch final review. The game is feature-complete; these fixes address grade risks found by the final reviewer.

Repository: /computacao-grafica-2026-02 (branch main).

## Read first
1. Your fix list (requirements — every item already decided by the controller; items 1–10, "Parqueados" are out of scope):
   /computacao-grafica-2026-02/.superpowers/sdd/PLANO-IMPLEMENTACAO/final-fixlist.md
2. Build/test/E2E/commit instructions for this machine:
   /computacao-grafica-2026-02/.superpowers/sdd/PLANO-IMPLEMENTACAO/env.md
3. CLAUDE.md (rules + full architecture/module map), SPEC.md (binding; read-only), images/spec/image1-3.png (visual reference), data/FONTES.txt, PROMPTS.md.
4. For item 2, the source material lives in .superpowers/sdd/PLANO-IMPLEMENTACAO/ (briefs, fixlists, reports with fix sections, progress.md ledger).

## Hard rules
- NEVER edit README.md or SPEC.md. Don't edit src/correcao.cpp or vendored code (include/glm, glad, GLFW, KHR, stb_image.h, tiny_obj_loader.h, dejavufont.h, third_party/imgui).
- All geometric tests in src/collisions.cpp. Never glm::lookAt/perspective/ortho/rotate/scale/translate. C++↔GLSL contract (uniforms declared + g_*_uniform + glGetUniformLocation; duplicated GLSL blocks textually identical in both shaders). Assets: CC0 with direct link, recorded in data/FONTES.txt; procedural meshes via tools/gen_models.py.
- TDD where testable. Don't loosen existing tests.
- For every visual change, LOOK at before/after screenshots (Read renders PNGs); keep the scene coherent and not darker/uglier.
- Small logical commits, each `IA: ...` with body citing "Revisão final, item N" and the trailer from env.md. Do not push.
- At the end: Windows full build (Debug; also check Release builds), logic_tests OK, `bash tests/run_e2e.sh` all PASS (~10 min, run once at the end; if Windows App Control blocks a fresh exe with "Permission denied", touch a .cpp, rebuild, rerun), and update CLAUDE.md where facts changed.

You never dispatch subagents. If unclear or stuck, report NEEDS_CONTEXT / BLOCKED.

## Report
Write the full report (per item: change, tests, evidence, screenshots + what they show, fps before/after for item 4) to:
/computacao-grafica-2026-02/.superpowers/sdd/PLANO-IMPLEMENTACAO/final-fix-report.md
Then reply with ONLY (under 15 lines): Status, commits, one-line test summary, concerns, report path.
