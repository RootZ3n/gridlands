#!/usr/bin/env python3
"""Planted defects for P11 (ADR-0039): Building v1. Fine yaw through the structural lifecycle (2.5 degree steps, oriented
geometry, socket facings), PREVIEW == REALITY (placement colour and removal prediction from the commit's own rules),
FRAME -> FINISH (a finish never changes support; electrical registered, not built), player-built construction in the
canonical structural model (collapse, debris, persistence, streaming), salvage quality by recovery path, inventory
stacks and slots (no weight; nothing ever discarded), shared base storage (claims, order, all or nothing, containers
never lose their contents), ownership (world renewal never touches player construction), plans, the v2 -> v3 save
migration, and the asynchronous restore that closes the P8 debt.

A permanent gate, same method and the same strict classifier as p10_structural.py: a defect is CAUGHT only when a test
ASSERTION fails. A run that dies, does not build, or fails only through engine errors is not a catch.

Usage: Tools/planted-defects/p11_building.py [--evidence DIR] [NAME ...]
       Tools/planted-defects/p11_building.py [--evidence DIR] --summarize
"""
import difflib, glob, json, os, re, shutil, subprocess, sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
RULES = 'Source/GridlandsCore/Private/Building/GLStructureRules.cpp'
COLLAPSE = 'Source/GridlandsCore/Private/Building/GLCollapseRules.cpp'
CONSTRUCTION = 'Source/GridlandsCore/Private/Building/GLConstructionRules.cpp'
CLAIMS = 'Source/GridlandsCore/Private/Building/GLClaimRules.cpp'
PLANS = 'Source/GridlandsCore/Private/Building/GLPlanRules.cpp'
POOL = 'Source/GridlandsCore/Private/Inventory/GLMaterialPool.cpp'
INVENTORY = 'Source/GridlandsCore/Private/Inventory/GLInventory.cpp'
CODEC = 'Source/GridlandsCore/Private/Save/GLWorldSave.cpp'
BUILDING = 'Source/GridlandsGame/Private/Building/GLBuildingSubsystem.cpp'
STRUCT = 'Source/GridlandsGame/Private/Structure/GLStructureSubsystem.cpp'
SALVAGE = 'Source/GridlandsGame/Private/Salvage/GLSalvageableComponent.cpp'
INVCOMP = 'Source/GridlandsGame/Private/Inventory/GLInventoryComponent.cpp'
PHASE = 'Data/phase/construction/electrical.json'
VALIDATE = 'Tools/gldata/validate.py'
TESTS = ('Gridlands.Core.BuildingV1+Gridlands.Game.BuildingV1+Gridlands.Core.Building+Gridlands.Game.Building+Gridlands.Core.Inventory'
         '+Gridlands.Game.Inventory+Gridlands.Core.Save+Gridlands.Game.Grid.PlayerConstructionStreamsWholeAndAFallResumes')

POOL_CONSUME = ('\tTaken.Reset();\n\tTaken.SetNum(Sources.Num());\n\tif (!CanConsume(Cost))\n\t{\n\t\treturn false;\n\t}\n'
                '\tverify(ConsumeInto(Sources, Cost, &Taken));\n\treturn true;')
POOL_GREEDY = ('\tTaken.Reset();\n\tTaken.SetNum(Sources.Num());\n\tconst bool bAll = CanConsume(Cost);\n'
               '\tfor (const FGLItemStackDef& Stack : Cost) { int32 Left = Stack.Count; for (FGLInventory* S : Sources) { const int32 Here = S ? FMath::Min(Left, S->CountOf(Stack.Item)) : 0; '
               'if (Here) { S->Remove(Stack.Item, Here); Left -= Here; } } } // DEFECT: takes what there is\n\treturn bAll;')

DEFECTS = [
    # --- fine yaw and oriented geometry
    ('B1-yaw-quantized-to-quarter-turns', [(RULES, '\treturn NormalizeYawStep(FMath::RoundToInt(Wrap360(Degrees) / YawStepDegrees));',
        '\treturn NormalizeYawStep(FMath::RoundToInt(Wrap360(Degrees) / 90.0) * QuarterTurnSteps); // DEFECT')]),
    ('B2-oriented-overlap-falls-back-to-boxes', [(RULES, '\t\tif (OtherDef && Mine.Overlaps(Footprint(*OtherDef, Other), OverlapShrinkCm))',
        '\t\tif (OtherDef && Bounds(*Def, Candidate).ExpandBy(-OverlapShrinkCm).Intersect(Bounds(*OtherDef, Other).ExpandBy(-OverlapShrinkCm))) // DEFECT')]),
    ('B3-terrain-footprint-is-the-box-not-the-piece', [(RULES, '\t\tif (Def && Def->Grounded && Footprint(*Def, Piece).ContainsXY(World, MarginCm))',
        '\t\tif (Def && Def->Grounded && Bounds(*Def, Piece).ExpandBy(FVector(MarginCm, MarginCm, 0.0)).IsInsideOrOnXY(FVector(World, 0.0))) // DEFECT'),
        (STRUCT, '\t\t\tFGLFootprint Footprint = PartFootprint(Part);\n\t\t\tFootprint.Half += FVector2D(MarginCm, MarginCm);',
         '\t\t\tFGLFootprint Footprint = FGLFootprint::FromBox(PartFootprint(Part).Enclosing()); // DEFECT: the enclosing box\n\t\t\tFootprint.Half += FVector2D(MarginCm, MarginCm);')]),
    ('B4-socket-facing-ignored-when-linking', [(RULES, '\t\treturn !(A.bHasFacing && B.bHasFacing) || AngleBetween(A.Facing, B.Facing + 180.0) <= GLStructureRules::FacingToleranceDegrees;',
        '\t\treturn true; // DEFECT')]),
    ('B5-snap-ignores-socket-facing', [(RULES, '\t\t\t\tif (bSide && bOursFaces && Theirs.bHasFacing)', '\t\t\t\tif (false && bSide && bOursFaces && Theirs.bHasFacing) // DEFECT')]),
    ('B6-sockets-rotated-by-the-nearest-quarter-turn', [(RULES,
        '\tconst FVector2D XY = RotateXY(FVector2D(Component(LocalMetres, 0), Component(LocalMetres, 1)) * 100.0, Piece.YawStep);',
        '\tconst FVector2D XY = RotateXY(FVector2D(Component(LocalMetres, 0), Component(LocalMetres, 1)) * 100.0, (Piece.YawStep + 18) / 36 * 36); // DEFECT')]),
    ('B7-drop-impact-on-world-axes', [(COLLAPSE, '\t\t\tOut.Impact.Axis[0] = FVector(Print.AxisX, 0.0);\n\t\t\tOut.Impact.Axis[1] = FVector(Print.AxisY, 0.0);',
        '\t\t\tOut.Impact.Axis[0] = FVector::ForwardVector; // DEFECT\n\t\t\tOut.Impact.Axis[1] = FVector::RightVector;')]),
    # --- PREVIEW == REALITY
    ('B8-preview-is-a-parallel-two-state-estimate', [(BUILDING, '\tResult.Preview = GLStructureRules::PreviewOf(Result);\n\treturn Result;',
        '\tResult.Preview = Result.IsAllowed() ? EGLPreview::Green : EGLPreview::Red; // DEFECT\n\treturn Result;')]),
    ('B9-yellow-threshold-off-by-one-step', [(RULES, '\treturn Check.Support <= Check.VerticalStep + 1e-9 ? EGLPreview::Yellow : EGLPreview::Green;',
        '\treturn Check.Support < Check.VerticalStep - 1e-9 ? EGLPreview::Yellow : EGLPreview::Green; // DEFECT')]),
    ('B10-removal-preview-misses-transitive-dependents', [(BUILDING,
        '\treturn GLStructureRules::CollapsesAfterRemoving(GLContent::Get(), Structures->PlayerStructureOf(PieceId), PieceId, [this](const FVector2D& At) { return GroundAt(At); });',
        '\tTArray<int32> All = GLStructureRules::CollapsesAfterRemoving(GLContent::Get(), Structures->PlayerStructureOf(PieceId), PieceId, [this](const FVector2D& At) { return GroundAt(At); });\n'
        '\tif (All.Num() > 1) { All.SetNum(1); } // DEFECT: direct dependents only\n\treturn All;')]),
    # --- FRAME -> FINISH
    ('B11-finish-changes-support', [(RULES, '\t\tStrength.Add(bStructural ? Material->Support.Strength : 0.0);',
        '\t\tStrength.Add(bStructural ? Material->Support.Strength + Pieces[I].Layers.Num() : 0.0); // DEFECT')]),
    ('B12-finish-on-a-form-that-refuses-it', [(CONSTRUCTION, '\tif (!Form->AcceptsPhase(Finish->Phase))', '\tif (false && !Form->AcceptsPhase(Finish->Phase)) // DEFECT')]),
    ('B13-electrical-marked-implemented', [(PHASE, '"implemented": false', '"implemented": true')]),
    # --- player-built construction in the canonical model
    ('B14-player-removal-deletes-what-falls-v0-style', [(STRUCT,
        '\t// The same canonical collapse as an authored structure losing a part (P6/P10): whatever lost support falls.\n\treturn Collapse(*Structure, By, Where);',
        '\tconst TArray<int32> Fallen = Collapse(*Structure, By, Where);\n'
        '\tStructure->Parts.RemoveAll([&Fallen](const FGLStructurePartRuntime& P) { return Fallen.Contains(P.Piece.Id); }); // DEFECT: v0 deletion\n\treturn Fallen;')]),
    ('B15-player-fall-in-flight-not-saved', [(STRUCT, '\t\tif (Collapse.Cell == Cell && !Collapse.bImpacted)',
        '\t\tif (Collapse.Cell == Cell && !Collapse.bImpacted && !Collapse.Placement.ToString().StartsWith(TEXT("player:"))) // DEFECT')]),
    ('B16-player-origin-not-saved', [(STRUCT, '\t\tSaved.Origin = static_cast<uint8>(Part.Piece.Origin);', '\t\tSaved.Origin = 0; // DEFECT')]),
    ('B17-layers-not-saved', [(STRUCT, '\t\tSaved.Layers = Part.Piece.Layers;', '\t\t; // DEFECT')]),
    ('B18-restore-presents-synchronously', [(STRUCT, '\t\tAddPlayerPiece(Piece, true); // the pump presents it within the frame budget (P8 synchronous-restore debt)',
        '\t\tAddPlayerPiece(Piece, false); // DEFECT')]),
    # --- salvage quality
    ('B19-careful-dismantle-pays-the-collapse-path', [(BUILDING, '\tResult.Recovered = ScaledYield(Piece, Path);', '\tResult.Recovered = ScaledYield(Piece, EGLSalvagePath::Collapse); // DEFECT')]),
    ('B20-debris-salvages-by-the-careful-path', [(STRUCT, '\t\tActor->GetSalvageable()->Setup(Part.Salvage, nullptr, EGLSalvagePath::Collapse);',
        '\t\tActor->GetSalvageable()->Setup(Part.Salvage, nullptr, EGLSalvagePath::Careful); // DEFECT')]),
    ('B21-smash-equals-careful', [(CONSTRUCTION, '\tcase EGLSalvagePath::Destructive: return Salvage.YieldsByPath.Destructive.Num() ? Salvage.YieldsByPath.Destructive : Salvage.Yields;',
        '\tcase EGLSalvagePath::Destructive: return Salvage.YieldsByPath.Careful.Num() ? Salvage.YieldsByPath.Careful : Salvage.Yields; // DEFECT')]),
    ('B22-salvage-overflow-silently-lost', [(SALVAGE, '\t\tif (!Sources.Pool().CanDeliver(GLContent::Get(), CompletionYield()))',
        '\t\tif (false && !Sources.Pool().CanDeliver(GLContent::Get(), CompletionYield())) // DEFECT')]),
    ('B23-removal-return-overflow-silently-lost', [(BUILDING, '\tif (!Pool.CanDeliver(GLContent::Get(), Result.Recovered))', '\tif (false && !Pool.CanDeliver(GLContent::Get(), Result.Recovered)) // DEFECT')]),
    # --- inventory
    ('B24-stack-limit-bypassed', [(INVENTORY, '\t\tconst int32 Moved = FMath::Min(Remaining, Def->StackSize);\n\t\tStacks.Add({ Item, Moved });',
        '\t\tconst int32 Moved = Remaining; // DEFECT: one unlimited stack\n\t\tStacks.Add({ Item, Moved });')]),
    ('B25-restore-discards-overflow', [(INVCOMP, '\t\tInventory.ForceAdd(GLContent::Get(), Item.Key, Item.Value); // a save is never truncated (P11)',
        '\t\tInventory.Add(GLContent::Get(), Item.Key, Item.Value); // DEFECT')]),
    # --- shared base storage and claims
    ('B26-personal-inventory-consumed-before-storage', [(BUILDING, '\t\tSources.Inventories.Add(&Sources.Personal->GetMutableInventory());',
        '\t\tSources.Inventories.Insert(&Sources.Personal->GetMutableInventory(), 0); // DEFECT: Zenny first')]),
    ('B27-partial-consumption-on-a-refused-operation', [(POOL, POOL_CONSUME, POOL_GREEDY)]),
    ('B28-container-contents-lost-on-collapse', [(STRUCT, '\t\tPart->State = EGLStructurePartState::Debris;\n\t\tPart->Rest = Outcome.Rest;',
        '\t\tPart->State = EGLStructurePartState::Debris;\n\t\tPart->Contents = FGLInventory(0); // DEFECT\n\t\tPart->Rest = Outcome.Rest;')]),
    ('B29-container-contents-lost-on-restore', [(STRUCT, '\t\t\tPart->Contents.ForceAdd(Content, Held.Id, Held.Count); // a save is never truncated', '\t\t\t; // DEFECT')]),
    ('B30-storage-anywhere-is-connected', [(BUILDING, '\tif (Claim && Claim->Contains(FVector2D(At)))',
        '\tstatic FGLClaim Everywhere; Everywhere.Areas = { { FVector2D::ZeroVector, 1e9, 0 } }; Claim = &Everywhere; // DEFECT: every crate in the world\n'
        '\tif (Claim && Claim->Contains(FVector2D(At)))')]),
    ('B31-a-falling-container-still-supplies', [(BUILDING,
        '\t\tfor (const FGLPlacedPiece& Piece : GetPieces())\n\t\t{\n\t\t\tconst FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);\n\t\t\tif (Def && Def->Storage.Slots > 0',
        '\t\tfor (const FGLPlacedPiece& Piece : Structures->PlayerPieces(NAME_None, true)) // DEFECT: debris too\n\t\t{\n\t\t\tconst FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);\n\t\t\tif (Def && Def->Storage.Slots > 0')]),
    ('B32-overlapping-claims-allowed', [(BUILDING, '\t\t\tif (New.Num() && New[0].Overlaps(Existing))', '\t\t\tif (false && New.Num() && New[0].Overlaps(Existing)) // DEFECT')]),
    # --- ownership, plans, migration
    ('B33-renewal-ignores-player-ownership', [(CLAIMS, '\tif (Origin == EGLPieceOrigin::Player)\n\t{\n\t\treturn false;\n\t}', '\t// DEFECT: ownership ignored')]),
    ('B34-renewal-ignores-claims', [(CLAIMS, '\treturn ClaimAt(Claims, Location) == nullptr;', '\treturn true; // DEFECT')]),
    ('B35-plan-rebuilt-without-the-anchor-turn', [(PLANS, '\t\tPiece.YawStep = GLStructureRules::NormalizeYawStep(Entry.YawStep + AnchorYawStep);',
        '\t\tPiece.YawStep = GLStructureRules::NormalizeYawStep(Entry.YawStep); // DEFECT')]),
    ('B36-v0-pieces-migrate-without-their-look', [(CODEC, '\t\t\t\t\tfor (const FName& Layer : Form->LegacyLayers)', '\t\t\t\t\tfor (const FName& Layer : TArray<FName>()) // DEFECT')]),
    ('B37-v2-yaw-not-scaled', [(CODEC, '\t\t\tPiece->SetNumberField(TEXT("yawStep"), ((Quarter % 4 + 4) % 4) * 36);',
        '\t\t\tPiece->SetNumberField(TEXT("yawStep"), (Quarter % 4 + 4) % 4); // DEFECT')]),
    # --- the data rules (tooling)
    ('B38-salvage-quality-lint-disabled', [(VALIDATE, '            check_paths(entity, data.get("salvage"), ".salvage")\n    for entity in sorted',
        '            pass  # DEFECT\n    for entity in sorted')]),
    ('B39-unimplemented-phase-lint-disabled', [(VALIDATE,
        '                ds.problem("PH-2", rel, ".phase", f"{data.get(\'phase\')} is not implemented yet: no content for it")',
        '                pass  # DEFECT')]),
]


ASSERTION_TEXT = re.compile(r"^Expected '|The two values are not equal|to be (true|false|null|not null|valid)\b|^Expected .* to be ")


def is_assertion(entry):
    """A test's own check failing, as opposed to an engine error, a crash or an unexpected log line. UE records the
    location of templated checks (TestEqual on enums, TestTrue) inside its own headers, so the message decides; an
    entry located in a test source file also counts. Engine log errors arrive as 'LogXxx: ...' and never match."""
    message = entry['event'].get('message', '')
    if message.startswith('Log') or 'Unexpected' in message:
        return False
    return bool(ASSERTION_TEXT.search(message)) or ('/Tests/' in (entry.get('filename') or '') and (entry.get('lineNumber') or 0) > 0)


def assertion_failures(index_path):
    """Failing tests and the assertion messages located in test sources (an engine error alone is not a catch)."""
    report = json.load(open(index_path, encoding='utf-8-sig'))
    found = []
    for test in report.get('tests', []):
        if test.get('state') == 'Success':
            continue
        asserts = [e['event']['message'] for e in test.get('entries', []) if e['event']['type'] == 'Error' and is_assertion(e)]
        found.append((test['fullTestPath'], asserts))
    return found


def summarize(evidence):
    """Re-derives every verdict from the reports already in DIR (no build, no run): the gate's state after reruns."""
    lines, caught = [], 0
    for name, edits in DEFECTS:
        index, tooling = os.path.join(evidence, name + '.index.json'), os.path.join(evidence, name + '.selftest.txt')
        if all(f.endswith('.py') for f, *_ in edits) and os.path.exists(tooling):
            failed = [l.strip() for l in open(tooling).read().splitlines() if l.strip().startswith('FAIL:')]
            verdict, detail = ('CAUGHT (tooling assertion)', failed) if failed else ('SURVIVED', [])
        elif os.path.exists(index):
            failures = assertion_failures(index)
            asserting = [(t, a) for t, a in failures if a]
            detail = [f'{t}: {a[0]}' for t, a in asserting]
            verdict = f'CAUGHT ({len(asserting)} test(s) by assertion)' if asserting else ('FAILED WITHOUT AN ASSERTION: not a catch' if failures else 'SURVIVED')
        else:
            verdict, detail = 'NO REPORT (did not build, or the run died): not a catch', []
        caught += verdict.startswith('CAUGHT')
        lines.append(f'{name}: {verdict}')
        lines.extend(f'    {x[:300]}'.replace('\n', ' ') for x in detail[:3])
    summary = '\n'.join(lines) + f'\n{caught}/{len(DEFECTS)} caught by assertion\n'
    open(os.path.join(evidence, 'summary.txt'), 'w').write(summary)
    print(summary)
    print(f'RESULT: {"PASS" if caught == len(DEFECTS) else "FAIL"} {caught}/{len(DEFECTS)} planted defects caught by assertion')
    sys.exit(0 if caught == len(DEFECTS) else 1)


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects-P11')
    if args[:1] == ['--evidence']:
        evidence, args = os.path.abspath(args[1]), args[2:]
    os.makedirs(evidence, exist_ok=True)
    if args[:1] == ['--summarize']:
        return summarize(evidence)
    chosen = [d for d in DEFECTS if not args or d[0] in args]
    results = []
    for name, edits in chosen:
        work = []
        for f in dict.fromkeys(e[0] for e in edits):  # each file once, its edits applied in order
            p = os.path.join(R, f)
            orig = mut = open(p).read()
            for ef, a, b in edits:
                if ef == f:
                    assert mut.count(a) == 1, f'{name}: anchor not found exactly once in {f}: {a[:60]!r}'
                    mut = mut.replace(a, b, 1)
            work.append((f, p, orig, mut))
        with open(os.path.join(evidence, name + '.diff'), 'w') as out:
            for f, p, orig, mut in work:
                out.writelines(difflib.unified_diff(orig.splitlines(1), mut.splitlines(1), 'a/' + f, 'b/' + f))
        for f, p, orig, mut in work:
            shutil.copy(p, p + '.defectbak')
        detail = []
        try:
            for f, p, orig, mut in work:
                open(p, 'w').write(mut)
            if all(f.endswith('.py') for f, *_ in work):
                run = subprocess.run([os.path.join(R, 'Tools', 'selftest.sh')], capture_output=True, text=True, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
                text = run.stdout + run.stderr
                failed = [l.strip() for l in text.splitlines() if l.strip().startswith(('FAIL:', 'ERROR:'))]
                open(os.path.join(evidence, name + '.selftest.txt'), 'w').write(text)
                detail = failed
                verdict = f'CAUGHT (tooling assertion: {failed[0]})' if failed and any('FAIL:' in l for l in failed) else ('SURVIVED' if run.returncode == 0 else 'RUN DIED (selftest errored without an assertion)')
            else:
                built = subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True, text=True)
                if 'RESULT: PASS' not in built.stdout:
                    verdict = 'BUILD FAILED (not a catch)'
                else:
                    before = set(glob.glob(os.path.join(R, '.test-reports', '*/')))
                    subprocess.run([os.path.join(R, 'Tools', 'test.sh'), '--no-build', TESTS], capture_output=True, text=True)
                    fresh = [d for d in glob.glob(os.path.join(R, '.test-reports', '*/')) if d not in before and os.path.exists(d + 'index.json')]
                    if not fresh:
                        verdict = 'RUN DIED (no new test report): not a catch'
                    else:
                        index = max(fresh, key=os.path.getmtime) + 'index.json'
                        shutil.copy(index, os.path.join(evidence, name + '.index.json'))
                        failures = assertion_failures(index)
                        asserting = [(t, a) for t, a in failures if a]
                        detail = [f'{t}: {a[0]}' for t, a in asserting] + [f'{t}: (no assertion; engine error/crash only)' for t, a in failures if not a]
                        if asserting:
                            verdict = f'CAUGHT ({len(asserting)} test(s) by assertion)'
                        elif failures:
                            verdict = 'FAILED WITHOUT AN ASSERTION (dead/engine-error run): not a catch'
                        else:
                            verdict = 'SURVIVED'
        finally:
            for f, p, orig, mut in work:
                shutil.move(p + '.defectbak', p)
                os.utime(p)  # newer than the defect's object files, or UBT keeps the defective binary
        results.append((name, verdict, detail))
        print(name, verdict, flush=True)
        for line in detail[:4]:
            print('    ', line[:220], flush=True)
    subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True)  # the restored source
    caught = sum(v.startswith('CAUGHT') for _, v, _ in results)
    lines = []
    for n, v, d in results:
        lines.append(f'{n}: {v}')
        lines.extend(f'    {x[:300]}' for x in d[:6])
    summary = '\n'.join(lines) + f'\n{caught}/{len(results)} caught by assertion\n'
    open(os.path.join(evidence, 'summary.txt'), 'w').write(summary)
    print(f'RESULT: {"PASS" if caught == len(results) else "FAIL"} {caught}/{len(results)} planted defects caught by assertion')
    sys.exit(0 if caught == len(results) else 1)


if __name__ == '__main__':
    main()
