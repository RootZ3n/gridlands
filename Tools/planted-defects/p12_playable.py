#!/usr/bin/env python3
"""Planted defects for P12 (ADR-0040): playable building. Each defect breaks one claim the build mode makes to the player:
the browser commits the piece chosen, the variant and finish chosen are the ones committed, the cost and its sources shown
are the consumption, the snap marker and yaw shown are what is committed, the structural word and reason are the canonical
check, the removal highlight is the canonical prediction and its hold follows collateral collapse, the salvage preview is
the recovery, the claim shown is the canonical claim, CAMERA AIM == GAMEPLAY AIM, favorites persist and recents are
deterministic, stairs and the window wall are ordinary components, modes never commit another mode's action, and the UI
is never authority.

The same strict classifier as p11_building.py: a defect is CAUGHT only when a test ASSERTION fails (or, for the defects
marked 'proof', a check of the real-game public-intent proof fails in a run that completed and wrote its result). A run
that dies, does not build, or fails only through engine errors is not a catch.

Usage: Tools/planted-defects/p12_playable.py [--evidence DIR] [NAME ...]
       Tools/planted-defects/p12_playable.py [--evidence DIR] --summarize
"""
import difflib, glob, json, os, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(__file__))
from p11_building import assertion_failures  # noqa: E402  (the shared strict classifier)

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MODE = 'Source/GridlandsGame/Private/Building/GLBuildModeComponent.cpp'
BUILDING = 'Source/GridlandsGame/Private/Building/GLBuildingSubsystem.cpp'
TEXT = 'Source/GridlandsGame/Private/Building/GLBuildText.cpp'
RULES = 'Source/GridlandsCore/Private/Building/GLStructureRules.cpp'
CATALOG = 'Source/GridlandsCore/Private/Building/GLBuildCatalog.cpp'
VALIDATE = 'Tools/gldata/validate.py'
STAIR = 'Data/buildpiece/modern/timber_stair.json'
WINDOW = 'Data/buildpiece/modern/window_wall.json'
WINDOW_BOARDS = 'Data/finish/modern/timber_board_window.json'
TESTS = ('Gridlands.Core.PlayableBuilding+Gridlands.Game.PlayableBuilding+Gridlands.Game.Building.StairsAreNavigable'
         '+Gridlands.Core.BuildingV1+Gridlands.Game.BuildingV1.WinchesterHouseFrameFinishCollapseAndPersistence')


def stair_without_treads(text):
    """The stair's twelve solid treads become one slab at its head: it still snaps and stands, but nobody can climb it."""
    data = json.loads(text)
    data['shapes'] = [{'size': [1.4, 4.0, 0.2], 'offset': [0, 0, 2.6]}]
    return json.dumps(data, indent=2) + '\n'


DEFECTS = [
    # --- the browser and the variant
    ('PB1-browser-commits-the-next-row', [(MODE, '\t\tSelectPiece(List[BrowseIndex]);\n\t\tSetState(EGLBuildState::Place); // predictable',
        '\t\tSelectPiece(List[(BrowseIndex + 1) % List.Num()]); // DEFECT\n\t\tSetState(EGLBuildState::Place); // predictable')]),
    ('PB2-browser-stays-open-after-a-choice', [(MODE, '\t\tSetState(EGLBuildState::Place); // predictable: a selection always returns to placing',
        '\t\t// DEFECT: the browser stays open')]),
    ('PB3-browser-click-also-places', [(MODE, '\tcase EGLBuildState::Browse:\n\t\tBrowseSelect();\n\t\treturn;',
        '\tcase EGLBuildState::Browse:\n\t\tBrowseSelect();\n\t\tRefreshView();\n\t\t[[fallthrough]]; // DEFECT')]),
    ('PB4-variant-cycles-to-the-category-head', [(MODE, '\t\tSelectPiece(List[((At + Delta) % List.Num() + List.Num()) % List.Num()]);',
        '\t\tSelectPiece(List[0]); // DEFECT')]),
    ('PB5-era-filter-locks-pieces', [(MODE, '\tif (CategoryIndexOf(InPiece) == INDEX_NONE)\n\t{\n\t\treturn;\n\t}\n\tPiece = InPiece;',
        '\tconst FGLBuildPieceDef* EraDef = GLContent::Get().Find<FGLBuildPieceDef>(InPiece);\n'
        '\tif (CategoryIndexOf(InPiece) == INDEX_NONE || (!EraFilter.IsNone() && EraDef && EraDef->Era != EraFilter)) // DEFECT: a filter locks\n\t{\n\t\treturn;\n\t}\n\tPiece = InPiece;')]),
    ('PB6-catalogue-ignores-explicit-category', [(CATALOG, '\tif (!Def->Category.IsNone() && Content.Find<FGLBuildCategoryDef>(Def->Category))',
        '\tif (false && !Def->Category.IsNone() && Content.Find<FGLBuildCategoryDef>(Def->Category)) // DEFECT')]),
    ('PB7-validator-lets-a-piece-have-no-category', [(VALIDATE,
        '            ds.problem("CAT-1", e.file, ".role", f"no category: give it a category, or add {e.data.get(\'role\')} to a category\'s fallbackRoles")',
        '            pass  # DEFECT')]),
    # --- frame / finish
    ('PB8-finish-installs-the-first-choice', [(MODE, '\t\t\tBuilding->InstallFinish(GetOwner(), View.FinishTarget, View.Finish);',
        '\t\t\tBuilding->InstallFinish(GetOwner(), View.FinishTarget, View.FinishChoices[0]); // DEFECT')]),
    ('PB9-finish-choice-not-remembered', [(MODE, '\tView.Finish = Remembered && View.FinishChoices.Contains(*Remembered) ? *Remembered : (View.FinishChoices.Num() ? View.FinishChoices[0] : NAME_None);',
        '\tView.Finish = View.FinishChoices.Num() ? View.FinishChoices[0] : NAME_None; (void)Remembered; // DEFECT')]),
    ('PB10-finishes-offered-without-the-install-rule', [(BUILDING,
        '\t\tif (Entry.Definition.GetPtr<FGLFinishDef>()\n\t\t\t&& GLConstructionRules::CanInstall(GLContent::Get(), Part->Piece, Entry.Id, Knowledge ? &Knowledge->GetKnowledge() : nullptr).IsAllowed())',
        '\t\tif (Entry.Definition.GetPtr<FGLFinishDef>() && (Knowledge || true)) // DEFECT: every finish, fitting or not')]),
    # --- cost and its sources
    ('PB11-cost-shows-personal-before-storage', [(BUILDING, '\t\t\t(I < Storage ? Line.FromStorage : Line.FromPersonal) += Take;',
        '\t\t\t(I >= Storage ? Line.FromStorage : Line.FromPersonal) += Take; // DEFECT')]),
    ('PB12-cost-ignores-base-storage', [(BUILDING, '\tconst int32 Storage = Sources.Containers.Num();\n\tTMap<FName, int32> Need;',
        '\tconst int32 Storage = 0; // DEFECT: a second, personal-only calculation\n\tTMap<FName, int32> Need;')]),
    # --- snap and yaw
    ('PB13-snap-marker-is-the-piece-centre', [(RULES, '\t\t\t\tInfo.TargetLocation = Theirs.Location;', '\t\t\t\tInfo.TargetLocation = Candidate.Location; // DEFECT')]),
    ('PB14-snap-reports-the-first-declared-socket', [(RULES, '\t\t\t\tInfo.OwnSocket = FName(*OursDef.Name);', '\t\t\t\tInfo.OwnSocket = FName(*Def->Sockets[0].Name); // DEFECT')]),
    ('PB15-resting-choice-by-data-order', [(RULES, '\t\t\t\tconst bool bBetter = Rests != BestRests ? Rests > BestRests : AwayScore != BestAway ? AwayScore > BestAway : Centre < BestCentre;',
        '\t\t\t\tconst bool bBetter = false && (Rests != BestRests || AwayScore != BestAway || Centre < BestCentre); // DEFECT: the first match')]),
    ('PB16-resting-choice-ignores-the-viewer', [(RULES, '\t\t\t\tconst bool bBetter = Rests != BestRests ? Rests > BestRests : AwayScore != BestAway ? AwayScore > BestAway : Centre < BestCentre;',
        '\t\t\t\tconst bool bBetter = Rests != BestRests ? Rests > BestRests : Centre < BestCentre; // DEFECT')]),
    ('PB17-yaw-shown-is-not-the-yaw-held', [(MODE, '\tView.YawStep = YawStep;', '\tView.YawStep = GLStructureRules::NormalizeYawStep(YawStep + 1); // DEFECT')]),
    ('PB18-shift-z-also-turns-plus-90', [(MODE, '\tYawStep = GLStructureRules::NormalizeYawStep(YawStep + (Direction >= 0 ? 1 : -1) * GLStructureRules::QuarterTurnSteps);',
        '\tYawStep = GLStructureRules::NormalizeYawStep(YawStep + GLStructureRules::QuarterTurnSteps); // DEFECT')]),
    ('PB19-fine-rotation-is-not-one-canonical-step', [(MODE, '\tYawStep = GLStructureRules::NormalizeYawStep(YawStep + (Direction >= 0 ? 1 : -1)); // one canonical 2.5 degree step',
        '\tYawStep = GLStructureRules::NormalizeYawStep(YawStep + (Direction >= 0 ? 2 : -2)); // DEFECT')]),
    # --- structural word and reason
    ('PB20-limit-shown-as-ok', [(TEXT, '\treturn Preview == EGLPreview::Green ? TEXT("OK") : Preview == EGLPreview::Yellow ? TEXT("LIMIT") : TEXT("NO");',
        '\treturn Preview != EGLPreview::Red ? TEXT("OK") : TEXT("NO"); // DEFECT')]),
    ('PB21-reason-text-not-from-the-refusal', [(TEXT, '\tcase EGLBuildRefusal::Unsupported: return TEXT("Nothing holds it up");',
        '\tcase EGLBuildRefusal::Unsupported: return TEXT("Something is in the way"); // DEFECT')]),
    ('PB22-overlap-loses-the-blocking-piece', [(RULES, "\t\tCheck.BlockingPieceId = Blocking;\n", "\t\t// DEFECT: which piece is not recorded\n")]),
    ('PB23-shortfall-reports-the-need-as-the-have', [(RULES, '\t\t\tCheck.MissingHave = Available(Cost.Item);', '\t\t\tCheck.MissingHave = Cost.Count; // DEFECT')]),
    # --- removal: prediction, confirmation
    ('PB24-removal-highlight-only-direct-dependents', [(MODE, '\t\t\tNow = Building->PreviewRemoval(View.RemoveTarget); // the commit\'s own function: what is shown is what will fall',
        '\t\t\tNow = Building->PreviewRemoval(View.RemoveTarget);\n\t\t\tif (Now.Num() > 1) { Now.SetNum(1); } // DEFECT: direct dependents only')]),
    ('PB25-confirmation-by-piece-importance', [(MODE, '\t\t\tView.bNeedsConfirm = Now.Num() > 0; // collateral collapse, not the piece\'s importance, decides',
        '\t\t\tView.bNeedsConfirm = Now.Num() > 0 || Part->Piece.Def == FName(TEXT("buildpiece.modern.base_core")); // DEFECT')]),
    ('PB26-safe-removal-needs-a-confirmation', [(MODE, '\t\tif (!View.bNeedsConfirm)\n\t\t{\n\t\t\tConfirmTarget = View.RemoveTarget;',
        '\t\tif (false && !View.bNeedsConfirm) // DEFECT\n\t\t{\n\t\t\tConfirmTarget = View.RemoveTarget;')]),
    ('PB27-a-collapse-removes-on-one-click', [(MODE, '\t\tif (!View.bNeedsConfirm)\n\t\t{\n\t\t\tConfirmTarget = View.RemoveTarget;',
        '\t\tif (true) // DEFECT: collapses need no hold\n\t\t{\n\t\t\tConfirmTarget = View.RemoveTarget;'),
        (MODE, '\tif (State != EGLBuildState::Remove || !View.RemoveTarget || View.RemoveTarget != ConfirmTarget\n\t\t|| (View.bNeedsConfirm && View.Predicted != ConfirmPrediction))',
         '\tif (State != EGLBuildState::Remove || !View.RemoveTarget || View.RemoveTarget != ConfirmTarget) // DEFECT')]),
    ('PB28-a-hold-transfers-to-another-piece', [(MODE, '\t\tconst bool bSame = SameConfirmation(ConfirmTarget, ConfirmPrediction, View.RemoveTarget, View.Predicted);',
        '\t\tconst bool bSame = true; // DEFECT'),
        (MODE, '\tif (State != EGLBuildState::Remove || !View.RemoveTarget || View.RemoveTarget != ConfirmTarget\n\t\t|| (View.bNeedsConfirm && View.Predicted != ConfirmPrediction))',
         '\tif (State != EGLBuildState::Remove || !View.RemoveTarget) // DEFECT: whatever is aimed at now')]),
    # --- salvage preview
    ('PB29-salvage-preview-is-always-careful', [(BUILDING, '\treturn Part && Part->State == EGLStructurePartState::Intact ? ScaledYield(Part->Piece, Path) : TMap<FName, int32>();',
        '\treturn Part && Part->State == EGLStructurePartState::Intact ? ScaledYield(Part->Piece, EGLSalvagePath::Careful) : TMap<FName, int32>(); // DEFECT')]),
    # --- claim
    ('PB30-claim-drawn-around-zenny', [(MODE, '\t\t\tView.ClaimAreas = Claim.Areas;\n',
        '\t\t\tView.ClaimAreas = Claim.Areas;\n\t\t\tfor (FGLClaimArea& Area : View.ClaimAreas) { Area.Centre = At; } // DEFECT\n')]),
    # --- camera aim == gameplay aim (only a controlled pawn with a real camera shows it: the real-game proof)
    ('PB31-aim-from-the-eyes-not-the-camera', [(MODE, '\t\tController->GetPlayerViewPoint(Eye, ViewRot);',
        '\t\tOwner->GetActorEyesViewPoint(Eye, ViewRot); (void)Controller; // DEFECT')], 'proof'),
    # --- favorites and recents
    ('PB32-favorites-not-saved', [(MODE, '\t\tFavs.Add(MakeShared<FJsonValueString>(Id.IsNone() ? FString() : Id.ToString()));',
        '\t\tFavs.Add(MakeShared<FJsonValueString>(FString())); (void)Id; // DEFECT')]),
    ('PB33-recents-appended-not-most-recent-first', [(MODE, '\t\t\tRecents.Insert(Piece, 0);', '\t\t\tRecents.Add(Piece); // DEFECT')]),
    ('PB34-recents-repeat', [(MODE, '\t\t\tRecents.Remove(Piece);\n', '\t\t\t// DEFECT: repeats kept\n')]),
    # --- modes and authority
    ('PB35-an-empty-click-in-remove-places', [(MODE, '\t\tif (!View.RemoveTarget)\n\t\t{\n\t\t\treturn;\n\t\t}',
        '\t\tif (!View.RemoveTarget)\n\t\t{\n\t\t\tFGLPlacedPiece Fallback; // DEFECT: an empty click in REMOVE places the selected piece\n'
        '\t\t\tif (Building->Snap(Piece, View.AimPoint, YawStep, Fallback)) { Building->Place(GetOwner(), Fallback); }\n\t\t\treturn;\n\t\t}')]),
    ('PB36-commit-trusts-the-last-view', [(MODE, '\tRefreshView(); // the commit re-aims and re-checks now: nothing shown earlier is trusted',
        '\t// DEFECT: the view shown last frame is committed')]),
    # --- stairs and the window wall: ordinary components
    ('PB37-stair-without-treads', [(STAIR, stair_without_treads)]),
    ('PB38-stair-head-misses-the-floor', [(STAIR, '      "name": "head",\n      "role": "side",\n      "offset": [\n        0,\n        2.0,\n        2.6\n      ]',
        '      "name": "head",\n      "role": "side",\n      "offset": [\n        0,\n        2.0,\n        2.3\n      ]')]),
    ('PB39-window-opening-closed', [(WINDOW, '      "size": [\n        0.9,\n        0.2,\n        0.9\n      ],\n      "offset": [\n        0,\n        0,\n        0.45\n      ]',
        '      "size": [\n        0.9,\n        0.2,\n        2.1\n      ],\n      "offset": [\n        0,\n        0,\n        1.05\n      ]')]),
    ('PB40-window-boards-fit-solid-walls', [(WINDOW_BOARDS, '    "window_wall"', '    "window_wall",\n    "wall"')]),
]


def apply(name, edits):
    work = []
    for f in dict.fromkeys(e[0] for e in edits):  # each file once, its edits applied in order
        p = os.path.join(R, f)
        orig = mut = open(p).read()
        for edit in edits:
            if edit[0] != f:
                continue
            if callable(edit[1]):
                mut = edit[1](mut)
            else:
                a, b = edit[1], edit[2]
                assert mut.count(a) == 1, f'{name}: anchor not found exactly once in {f}: {a[:70]!r}'
                mut = mut.replace(a, b, 1)
        assert mut != orig, f'{name}: no change in {f}'
        work.append((f, p, orig, mut))
    return work


def proof_verdict(evidence, name):
    """The real-game public-intent proof (build run): CAUGHT only if it completed, wrote its result, and a check failed."""
    out = os.path.join(R, 'Saved', 'P12')
    for f in glob.glob(os.path.join(out, 'intent-*.json')):
        os.remove(f)
    subprocess.run([os.path.join(R, 'Tools', 'p12-intent-proof.sh')], capture_output=True, text=True)
    result = os.path.join(out, 'intent-build.json')
    if not os.path.exists(result):
        return 'RUN DIED (no proof result): not a catch', []
    shutil.copy(result, os.path.join(evidence, name + '.intent-build.json'))
    data = json.load(open(result))
    failed = [f"{c['check']}: {c['detail']}" for c in data.get('checks', []) if not c.get('pass')]
    if any(c.startswith('finished in time') for c in failed):
        return 'PROOF STALLED (did not complete): not a catch', failed
    return (f'CAUGHT ({len(failed)} proof check(s))', failed) if failed else ('SURVIVED', [])


def tests_verdict(evidence, name):
    before = set(glob.glob(os.path.join(R, '.test-reports', '*/')))
    ran = subprocess.run([os.path.join(R, 'Tools', 'test.sh'), '--no-build', TESTS], capture_output=True, text=True)
    fresh = [d for d in glob.glob(os.path.join(R, '.test-reports', '*/')) if d not in before and os.path.exists(d + 'index.json')]
    selftest_log = os.path.join(R, '.test-reports', 'selftest.log')
    if not fresh and 'tooling self-tests failed' in ran.stdout + ran.stderr and os.path.exists(selftest_log):
        text = open(selftest_log).read()
        open(os.path.join(evidence, name + '.selftest.txt'), 'w').write(text)
        detail = [l.strip() for l in text.splitlines() if l.strip().startswith('FAIL:')]
        return (f'CAUGHT (tooling assertion: {detail[0]})' if detail else 'RUN DIED (self-tests errored without an assertion): not a catch'), detail
    if not fresh:
        return 'RUN DIED (no new test report): not a catch', []
    index = max(fresh, key=os.path.getmtime) + 'index.json'
    shutil.copy(index, os.path.join(evidence, name + '.index.json'))
    failures = assertion_failures(index)
    asserting = [(t, a) for t, a in failures if a]
    detail = [f'{t}: {a[0]}' for t, a in asserting] + [f'{t}: (no assertion; engine error/crash only)' for t, a in failures if not a]
    if asserting:
        return f'CAUGHT ({len(asserting)} test(s) by assertion)', detail
    return ('FAILED WITHOUT AN ASSERTION (dead/engine-error run): not a catch' if failures else 'SURVIVED'), detail


def run_one(evidence, name, edits, how):
    work = apply(name, edits)
    with open(os.path.join(evidence, name + '.diff'), 'w') as out:
        for f, p, orig, mut in work:
            out.writelines(difflib.unified_diff(orig.splitlines(1), mut.splitlines(1), 'a/' + f, 'b/' + f))
    for f, p, orig, mut in work:
        shutil.copy(p, p + '.defectbak')
    try:
        for f, p, orig, mut in work:
            open(p, 'w').write(mut)
        if all(f.endswith('.py') for f, *_ in work):
            run = subprocess.run([os.path.join(R, 'Tools', 'selftest.sh')], capture_output=True, text=True, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
            text = run.stdout + run.stderr
            open(os.path.join(evidence, name + '.selftest.txt'), 'w').write(text)
            detail = [l.strip() for l in text.splitlines() if l.strip().startswith('FAIL:')]
            return (f'CAUGHT (tooling assertion: {detail[0]})' if detail else ('SURVIVED' if run.returncode == 0 else 'RUN DIED (selftest errored without an assertion)')), detail
        built = subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True, text=True)
        if 'RESULT: PASS' not in built.stdout:
            return 'BUILD FAILED (not a catch)', [l for l in built.stdout.splitlines() if 'error' in l][:3]
        return proof_verdict(evidence, name) if how == 'proof' else tests_verdict(evidence, name)
    finally:
        for f, p, orig, mut in work:
            shutil.move(p + '.defectbak', p)
            os.utime(p)  # newer than the defect's object files, or UBT keeps the defective binary


def write_summary(evidence, results):
    caught = sum(v.startswith('CAUGHT') for _, v, _ in results)
    lines = []
    for n, v, d in results:
        lines.append(f'{n}: {v}')
        lines.extend(f'    {x[:300]}'.replace('\n', ' ') for x in d[:4])
    summary = '\n'.join(lines) + f'\n{caught}/{len(results)} caught by assertion\n'
    open(os.path.join(evidence, 'summary.txt'), 'w').write(summary)
    print(summary)
    print(f'RESULT: {"PASS" if caught == len(results) else "FAIL"} {caught}/{len(results)} planted defects caught by assertion')
    return caught == len(results)


def summarize(evidence):
    """Re-derives every verdict from the reports already in DIR (no build, no run)."""
    results = []
    for d in DEFECTS:
        name = d[0]
        index, tooling, proof = (os.path.join(evidence, name + s) for s in ('.index.json', '.selftest.txt', '.intent-build.json'))
        if os.path.exists(proof):
            data = json.load(open(proof))
            failed = [f"{c['check']}: {c['detail']}" for c in data.get('checks', []) if not c.get('pass')]
            verdict = ('PROOF STALLED: not a catch' if any(c.startswith('finished in time') for c in failed)
                       else f'CAUGHT ({len(failed)} proof check(s))' if failed else 'SURVIVED')
            results.append((name, verdict, failed))
        elif os.path.exists(index):
            failures = assertion_failures(index)
            asserting = [(t, a) for t, a in failures if a]
            verdict = f'CAUGHT ({len(asserting)} test(s) by assertion)' if asserting else ('FAILED WITHOUT AN ASSERTION: not a catch' if failures else 'SURVIVED')
            results.append((name, verdict, [f'{t}: {a[0]}' for t, a in asserting]))
        elif os.path.exists(tooling):
            failed = [l.strip() for l in open(tooling).read().splitlines() if l.strip().startswith('FAIL:')]
            results.append((name, 'CAUGHT (tooling assertion)' if failed else 'SURVIVED', failed))
        else:
            results.append((name, 'NO REPORT (did not build, or the run died): not a catch', []))
    sys.exit(0 if write_summary(evidence, results) else 1)


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects-P12')
    if args[:1] == ['--evidence']:
        evidence, args = os.path.abspath(args[1]), args[2:]
    os.makedirs(evidence, exist_ok=True)
    if args[:1] == ['--summarize']:
        return summarize(evidence)
    if args[:1] == ['--check-anchors']:  # every defect applies cleanly (no build, nothing written)
        for d in DEFECTS:
            apply(d[0], d[1])
        print(f'{len(DEFECTS)} defects apply cleanly')
        return
    chosen = [d for d in DEFECTS if not args or d[0] in args]
    results = []
    for d in chosen:
        name, edits, how = d[0], d[1], (d[2] if len(d) > 2 else 'tests')
        verdict, detail = run_one(evidence, name, edits, how)
        results.append((name, verdict, detail))
        print(name, verdict, flush=True)
        for line in detail[:4]:
            print('    ', line[:220], flush=True)
    subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True)  # the restored source
    sys.exit(0 if write_summary(evidence, results) else 1)


if __name__ == '__main__':
    main()
