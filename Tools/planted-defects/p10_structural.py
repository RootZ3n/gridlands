#!/usr/bin/env python3
"""Planted defects for P10 (ADR-0038): structural environmental resolution. Support failure decides the fall, never
its victims; the impact decides from where everything is then (Neutralize.Pinned XOR damage); a fall in flight is a
durable fact rebuilt bit-identically (never re-planned), frozen with its dormant cell and held until its cell's
creatures can move; nothing replays.

A permanent gate. Same method as p9_encounter.py: each defect is written into a backup-protected copy of the source,
built and tested on its own, and restored (never with git checkout). Stricter than the earlier suites: a defect is
CAUGHT only when a test ASSERTION fails (an error entry located in a test source file). A run that dies, does not
build, or fails only through engine errors is reported as such and is not a catch.

Usage: Tools/planted-defects/p10_structural.py [--evidence DIR] [NAME ...]
"""
import difflib, glob, json, os, shutil, subprocess, sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
STRUCT = 'Source/GridlandsGame/Private/Structure/GLStructureSubsystem.cpp'
MODELS = 'Source/GridlandsGame/Private/World/GLCreatureModels.cpp'
CREATURE = 'Source/GridlandsGame/Private/Combat/GLCreature.cpp'
SAVE = 'Source/GridlandsGame/Private/Save/GLSaveSubsystem.cpp'
RULES = 'Source/GridlandsCore/Private/Building/GLCollapseRules.cpp'
PENDING = 'Source/GridlandsCore/Private/Building/GLPendingCollapse.cpp'
VALIDATE = 'Tools/gldata/validate.py'
TESTS = 'Gridlands.Game.Structural+Gridlands.Core.Structure+Gridlands.Game.Structure+Gridlands.Game.Encounter'

KEY = 'FString::Printf(TEXT("%s/%s"), *Placement.ToString(), *PartName.ToString())'
AT_FAILURE_GLOBAL = ('\tbool IsPresent(EGLStructurePartState State)', '\tTMap<FString, TArray<FName>> GAtFailure; // DEFECT: who was under it at failure\n\n\tbool IsPresent(EGLStructurePartState State)')
AT_FAILURE_RECORD = ('\t\tEntry.CreditActor = By;', '\t\tEntry.CreditActor = By;\n\t\tGAtFailure.Add(FString::Printf(TEXT("%s/%s"), *Entry.Placement.ToString(), *Entry.Part.ToString()), GetWorld()->GetSubsystem<UGLPlacementSubsystem>()->ActiveCreaturesTouching(Outcome.Impact)); // DEFECT')
CANDIDATES = 'if (Entry.Value.Kind != TEXT("spawn") || !Entry.Value.Creature.IsActiveHostile() || !CreatureLocation(Entry.Key, Feet))'
DAMAGE_GUARD = 'if (!Model || Model->Kind != TEXT("spawn") || !Model->Creature.IsActiveHostile() || Amount <= 0.0)'
RESUME_CREDIT = '\t\t\tResumed.Credit = Flight->Credit;'
RESTORE_TAIL = '''	M.HeldYaw = Saved.HeldYaw;
	if (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))'''

DEFECTS = [
    # --- timing and authority: failure decides the fall, the impact decides the victims
    ('S1-victims-decided-at-support-failure', [(STRUCT, '\t\tEntry.Outcome = Outcome;',
        '\t\tEntry.Outcome = Outcome;\n\t\tEntry.Outcome.ImpactSeconds = 0.0; // DEFECT: resolved the moment support fails')]),
    ('S2-target-at-its-failure-position-hit-after-escaping', [(STRUCT,) + AT_FAILURE_GLOBAL, (STRUCT,) + AT_FAILURE_RECORD,
        (STRUCT, 'for (const FName& Creature : Placements->ActiveCreaturesTouching(Outcome.Impact))', f'for (const FName& Creature : GAtFailure.FindRef({KEY})) // DEFECT')]),
    ('S3-late-entrant-ignored-because-absent-at-failure', [(STRUCT,) + AT_FAILURE_GLOBAL, (STRUCT,) + AT_FAILURE_RECORD,
        (STRUCT, '\t\t\tFVector Feet;\n\t\t\tPlacements->CreatureLocation(Creature, Feet);',
         f'\t\t\tif (!GAtFailure.FindRef({KEY}).Contains(Creature)) {{ continue; }} // DEFECT\n\t\t\tFVector Feet;\n\t\t\tPlacements->CreatureLocation(Creature, Feet);')]),
    ('S4-impact-volume-where-the-part-was-not-where-it-lands', [(STRUCT, '\t\tEntry.Outcome = Outcome;',
        '\t\tEntry.Outcome = Outcome;\n\t\tEntry.Outcome.Impact.Centre.Z += Outcome.FallMetres * 100.0; // DEFECT: the start pose')]),
    # --- who is affected, and how
    ('S5-actorless-creature-immune-to-impacts', [(MODELS, CANDIDATES,
        'if (Entry.Value.Kind != TEXT("spawn") || !Entry.Value.Actor.IsValid() || !Entry.Value.Creature.IsActiveHostile() || !CreatureLocation(Entry.Key, Feet)) // DEFECT')]),
    ('S6-non-susceptible-target-neutralized', [(MODELS, '!Model->Creature.IsActiveHostile() || !Def->NeutralizableBy.Contains(How))',
        '!Model->Creature.IsActiveHostile() || !(Def->NeutralizableBy.Contains(How) || How == TEXT("Neutralize.Pinned"))) // DEFECT: Pinned works on anyone')]),
    ('S7-severity-threshold-ignored', [(RULES, '\treturn Tuning.PinMinSeverity > 0.0 && Outcome.Severity >= Tuning.PinMinSeverity;',
        '\treturn Outcome.Severity > 0.0; // DEFECT')]),
    ('S8-neutralizable-target-damaged-and-neutralized', [(STRUCT, '\t\t\tif (bPins && Placements->TryNeutralize(',
        '\t\t\tif (Placements->DamageCreature(Creature, Outcome.Damage, Credit), bPins && Placements->TryNeutralize( // DEFECT: damage first, then pin')]),
    ('S9-defeated-and-neutralized-still-considered', [(MODELS, CANDIDATES,
        'if (Entry.Value.Kind != TEXT("spawn") || !CreatureLocation(Entry.Key, Feet)) // DEFECT')]),
    ('S10-neutralized-target-takes-later-collapse-damage', [(MODELS, CANDIDATES,
        'if (Entry.Value.Kind != TEXT("spawn") || !CreatureLocation(Entry.Key, Feet)) // DEFECT'),
        (MODELS, DAMAGE_GUARD, 'if (!Model || Model->Kind != TEXT("spawn") || Amount <= 0.0) // DEFECT'),
        (CREATURE, '\tHealth->SetIgnoresDamage(true);', '\t// DEFECT: and the P9 immunity')]),
    ('S11-one-impact-hits-a-creature-twice', [(STRUCT, '\t\t\tcontinue; // decided on its model above', '\t\t\t; // DEFECT')]),
    ('S12-model-capsule-differs-from-the-actor', [(MODELS,
        '\t\tconst FVector Centre = Feet + FVector(0.0, 0.0, GLCreatureRules::CapsuleHalfHeightCm);',
        '\t\tconst FVector Centre = Feet + FVector(0.0, 0.0, GLCreatureRules::CapsuleHalfHeightCm - 20.0); // DEFECT: a second, different capsule')]),
    # --- persistence: in flight is a durable fact
    ('S13-in-flight-collapses-not-captured', [(SAVE, '\t\tStructures->CaptureCollapses(Cell, Record.Collapses); // P10: in flight, frozen while the record waits', '\t\t; // DEFECT')]),
    ('S14-file-save-drops-in-flight-collapses', [(SAVE,
        '\tconst double Passed = Record.CapturedWorldSeconds >= 0.0 ? FMath::Max(0.0, Now - Record.CapturedWorldSeconds) : 0.0;',
        '\tconst double Passed = Record.CapturedWorldSeconds >= 0.0 ? FMath::Max(0.0, Now - Record.CapturedWorldSeconds) : 0.0;\n\tRecord.Collapses.Reset(); // DEFECT')]),
    ('S15-resumed-fall-presented-solid', [(STRUCT,
        '\t\t// Mid-fall (decided while it was waiting): the plan\'s pose, not solid, until it lands.\n\t\tActor->SetSolid(false);',
        '\t\t// Mid-fall (decided while it was waiting): the plan\'s pose, not solid, until it lands.\n\t\tActor->SetSolid(true); // DEFECT')]),
    ('S16-restored-with-elapsed-reset', [(STRUCT, '\t\t\tResumed.Elapsed = Flight->ElapsedSeconds;', '\t\t\tResumed.Elapsed = 0.0; // DEFECT')]),
    ('S17-dormant-collapse-advances-while-its-target-is-frozen', [(SAVE,
        '\t\tStructures->RestoreCell(Record.Cell, Record.StructureParts, Record.Collapses, OutProblems);',
        '\t\tTArray<FGLSavedCollapse> Aged = Record.Collapses; for (FGLSavedCollapse& C : Aged) { C.ElapsedSeconds += Elapsed; } // DEFECT: the fall kept time\n\t\tStructures->RestoreCell(Record.Cell, Record.StructureParts, Aged, OutProblems);')]),
    ('S18-resumes-before-restored-creatures-can-move', [(STRUCT,
        '\t\tif (Placements && Placements->HasPendingCreatures(Collapse.Cell))', '\t\tif (false && Placements && Placements->HasPendingCreatures(Collapse.Cell)) // DEFECT')]),
    ('S19-restore-replans-in-the-current-world', [(STRUCT, '\t\t\tOutcome.Def = Part->Piece.Def;',
        '\t\t\tOutcome.Def = Part->Piece.Def;\n\t\t\t{ const TArray<FGLCollapseRequest> Again = { { Part->Piece, Part->Motion, Part->Direction, Part->DamageScale } }; '
        'const FGLCollapsePlan P = GLCollapseRules::Plan(GLContent::Get(), Again, {}, [this](const FVector2D& At) { return GroundAt(At); }, FVector::ZeroVector, GLContent::Tuning().Collapse); '
        'if (P.Outcomes.Num() == 1) { Outcome = P.Outcomes[0]; Outcome.PieceId = Part->Piece.Id; } } // DEFECT: re-planned')]),
    ('S20-impacted-collapse-still-saved-as-pending', [(STRUCT, '\t\tif (Collapse.Cell == Cell && !Collapse.bImpacted)', '\t\tif (Collapse.Cell == Cell) // DEFECT'),
        (STRUCT, '\tActive.RemoveAll([](const FGLActiveCollapse& Collapse) { return Collapse.bImpacted; });',
         '\tActive.RemoveAll([](const FGLActiveCollapse& Collapse) { return Collapse.bImpacted && Collapse.Elapsed > Collapse.Outcome.ImpactSeconds + 1.0; }); // DEFECT: kept a while')]),
    ('S21-credit-lost-after-reload', [(STRUCT, RESUME_CREDIT, '\t\t\tResumed.Credit = NAME_None; // DEFECT')]),
    # --- replay: a restore is silent
    ('S22-restore-replays-the-collapse-event', [(STRUCT, RESUME_CREDIT,
        RESUME_CREDIT + '\n\t\t\tEmitStructureEvent(this, TEXT("Event.Structure.Collapsed"), Structure->Def, nullptr, 1); // DEFECT')]),
    ('S23-restore-replays-the-impact-noise', [(STRUCT, RESUME_CREDIT,
        RESUME_CREDIT + '\n\t\t\tUGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Structure.Collapse"), Resumed.Outcome.Impact.Centre, nullptr, Resumed.Material); // DEFECT')]),
    ('S24-restore-replays-the-pinned-reward', [(MODELS, RESTORE_TAIL,
        '\tM.HeldYaw = Saved.HeldYaw;\n\tif (M.NeutralizedHow == FName(TEXT("Neutralize.Pinned"))) { ResolveEncounter(*Model, true); } // DEFECT\n\tif (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))')]),
    # --- reconstruction
    ('S25-topple-reintegration-drifts', [(PENDING,
        '\t\tconst double Seconds = GLCollapseRules::IntegrateTopple(S.ToppleHeightCm, S.ToppleGravityCmS2, S.ToppleStartRadians, Out.ToppleAngles);',
        '\t\tconst double Seconds = GLCollapseRules::IntegrateTopple(S.ToppleHeightCm, S.ToppleGravityCmS2, S.ToppleStartRadians, Out.ToppleAngles);\n\t\tfor (double& A : Out.ToppleAngles) { A += 1e-12; } // DEFECT')]),
    ('S26-rest-saved-through-a-rotator', [(PENDING, '\tOut.Rest = FTransform(S.RestRotation, S.RestLocation);',
        '\tOut.Rest = FTransform(S.RestRotation.Rotator(), S.RestLocation); // DEFECT: through a rotator')]),
    ('S27-severity-not-physical', [(RULES, '\t\tOut.Severity = Out.FallMetres * 100.0 < NoFallCm ? 0.0 : Out.FallMetres * Request.DamageScale;',
        '\t\tOut.Severity = Out.FallMetres * 100.0 < NoFallCm ? 0.0 : Out.FallMetres; // DEFECT: the material ignored')]),
    # --- the cross-cell rule (tooling)
    ('S28-structure-reach-lint-disabled', [(VALIDATE, '                check_structure_reach(ds, entity, transform, cells_by_short, margin_cm)',
        '                pass  # DEFECT')]),
]


def assertion_failures(index_path):
    """Failing tests and the assertion messages located in test sources (an engine error alone is not a catch)."""
    report = json.load(open(index_path, encoding='utf-8-sig'))
    found = []
    for test in report.get('tests', []):
        if test.get('state') == 'Success':
            continue
        asserts = [e['event']['message'] for e in test.get('entries', [])
                   if e['event']['type'] == 'Error' and '/Tests/' in (e.get('filename') or '') and (e.get('lineNumber') or 0) > 0]
        found.append((test['fullTestPath'], asserts))
    return found


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects-P10')
    if args[:1] == ['--evidence']:
        evidence, args = os.path.abspath(args[1]), args[2:]
    os.makedirs(evidence, exist_ok=True)
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
