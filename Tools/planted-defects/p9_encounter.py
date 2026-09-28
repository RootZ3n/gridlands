#!/usr/bin/env python3
"""Planted defects for P9 (ADR-0037, ADR-0029 as amended): durable creature facts, DEFEATED vs NEUTRALIZED, the
model-first mechanism's decision moment, ambient masking, and navigation that scales with authored regions.
A permanent gate. Same method as terrain_collision.py and p8_split_lod.py: each defect is written into a
backup-protected copy of the source, built and tested on its own, and restored (never with git checkout). Every
defect must be CAUGHT; a dead or unbuildable run is not a catch.

Usage: Tools/planted-defects/p9_encounter.py [--evidence DIR] [NAME ...]
"""
import difflib, glob, os, shutil, subprocess, sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CREATURE = 'Source/GridlandsGame/Private/Combat/GLCreature.cpp'
MODELS = 'Source/GridlandsGame/Private/World/GLCreatureModels.cpp'
SAVE = 'Source/GridlandsGame/Private/Save/GLSaveSubsystem.cpp'
MECH = 'Source/GridlandsGame/Private/Mechanism/GLMechanismSubsystem.cpp'
RULES = 'Source/GridlandsCore/Private/Combat/GLCreatureRules.cpp'
REGIONS = 'Source/GridlandsGame/Private/World/GLNavRegionSubsystem.cpp'
TESTS = 'Gridlands.Game.Encounter+Gridlands.Core.Combat+Gridlands.Game.Navigation+Gridlands.Game.Noise+Gridlands.Game.ActorPresentation+Gridlands.Game.Combat'

RESTORE_TAIL = '''	M.HeldYaw = Saved.HeldYaw;
	if (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))'''

DEFECTS = [
    # --- the operator's list
    ('N1-containment-while-target-outside-the-volume', [(MECH,
        '&& Placements->CreatureLocation(Entry.Key, At) && Box.IsInsideOrOn(At))',
        '&& Placements->CreatureLocation(Entry.Key, At)) // DEFECT: the box is not checked')]),
    ('N2-entering-after-the-decision-is-retroactively-neutralized', [(CREATURE,
        '\t\tPlacements->SyncCreatureFromActor(*this);',
        '\t\tPlacements->SyncCreatureFromActor(*this);\n\t\tif (UGLMechanismSubsystem* Mechs = GetWorld()->GetSubsystem<UGLMechanismSubsystem>()) { for (const TPair<FName, FGLMechanismRecord>& R : Mechs->GetRecords()) { const FGLMechanismDef* D = UGLMechanismSubsystem::DefOf(R.Value); if (D && D->HasNeutralize() && R.Value.State == D->Neutralize.InState && UGLMechanismSubsystem::NeutralizeBox(R.Value).IsInsideOrOn(GetActorLocation() - FVector(0, 0, 60))) { Placements->TryNeutralize(PlacementId, D->Neutralize.Tag, R.Key, GetActorLocation(), 0.0); } } } // DEFECT: a standing trigger'),
        (CREATURE, '#include "World/GLNavRegionSubsystem.h"', '#include "World/GLNavRegionSubsystem.h"\n#include "Mechanism/GLMechanismSubsystem.h"')]),
    ('N3-reload-replays-the-neutralization-event', [(MODELS, RESTORE_TAIL,
        '\tM.HeldYaw = Saved.HeldYaw;\n\tif (M.Outcome == EGLCreatureOutcome::Neutralized) { Announce(this, TEXT("Event.Creature.Neutralized"), Model->Definition, nullptr, [](FGLGameplayEvent&) {}); } // DEFECT\n\tif (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))')]),
    ('N4-reload-grants-the-encounter-reward-again', [(MODELS, RESTORE_TAIL,
        '\tM.HeldYaw = Saved.HeldYaw;\n\tif (M.Outcome != EGLCreatureOutcome::None) { ResolveEncounter(*Model, M.Outcome == EGLCreatureOutcome::Neutralized); } // DEFECT\n\tif (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))')]),
    ('N5-neutralized-actor-resumes-hostile-behaviour-when-remade', [(CREATURE,
        '\tState = Model.State == EGLCreatureState::Attack ? EGLCreatureState::Chase : Model.State;',
        '\tState = Model.Outcome == EGLCreatureOutcome::Neutralized || Model.State == EGLCreatureState::Attack ? EGLCreatureState::Chase : Model.State; // DEFECT'),
        (CREATURE, '\t\tPresentNeutralized(Model.HeldAt, Model.HeldYaw);', '\t\t// DEFECT: presented as an active hostile')]),
    ('N6-saved-wounded-creature-restores-at-full-health', [(CREATURE,
        '\t\tHealth->Restore(Model.Health); // P9: a wounded creature stays wounded', '\t\t; // DEFECT')]),
    ('N7-save-reload-in-alerted-or-search-returns-to-calm', [(CREATURE,
        '\tState = Model.State == EGLCreatureState::Attack ? EGLCreatureState::Chase : Model.State;',
        '\tState = Model.Outcome == EGLCreatureOutcome::None ? EGLCreatureState::Idle : Model.State; // DEFECT: memory wiped')]),
    ('N8-pehlichi-lure-bypasses-ambient-masking', [(CREATURE,
        'const double Mask = NoiseWorld ? NoiseWorld->MaskAt(GetActorLocation()) : 0.0;',
        'const double Mask = Heard.bDistraction ? 0.0 : (NoiseWorld ? NoiseWorld->MaskAt(GetActorLocation()) : 0.0); // DEFECT')]),
    # --- the proposal's list
    ('N9-health-never-written-through', [(CREATURE,
        '\tWriteThrough(); // its health is a gameplay fact (P9: never restored to full by a reload)', '\t// DEFECT')]),
    ('N10-streaming-does-not-count-the-time-away', [(SAVE,
        'const double Elapsed = Record.CapturedWorldSeconds >= 0.0 ? FMath::Max(0.0, World->GetTimeSeconds() - Record.CapturedWorldSeconds) : 0.0;',
        'const double Elapsed = 0.0; // DEFECT')]),
    ('N11-position-not-restored-it-reappears-at-home', [(MODELS,
        '\tModel->Location = Saved.Location;', '\t// DEFECT: back home')]),
    ('N12-an-unpresented-model-cannot-hear', [(MODELS,
        '\t\t// Waiting for presentation, or far: its model hears by the same rule, at its own position.',
        '\t\tcontinue; // DEFECT')]),
    ('N13-neutralized-recorded-as-defeated', [(MODELS,
        '\tM.Outcome = EGLCreatureOutcome::Neutralized;', '\tM.Outcome = EGLCreatureOutcome::Defeated; // DEFECT')]),
    ('N14-neutralization-through-a-fake-damage-event', [(MODELS,
        '\tM.Outcome = EGLCreatureOutcome::Neutralized;',
        '\tif (AGLCreature* Hit = Cast<AGLCreature>(Model->Actor.Get())) { Hit->GetHealth()->ApplyDamage(1.0, Hit); } // DEFECT\n\tM.Outcome = EGLCreatureOutcome::Neutralized;')]),
    ('N15-susceptibility-ignored', [(MODELS,
        '!Model->Creature.IsActiveHostile() || !Def->NeutralizableBy.Contains(How))', '!Model->Creature.IsActiveHostile()) // DEFECT')]),
    ('N16-mechanism-state-not-saved', [(SAVE,
        '\t\tMechanisms->CaptureCell(Cell, Record.Mechanisms);', '\t\t; // DEFECT')]),
    ('N17-mask-judged-at-the-source-not-the-listener', [(CREATURE,
        'const double Mask = NoiseWorld ? NoiseWorld->MaskAt(GetActorLocation()) : 0.0;',
        'const double Mask = NoiseWorld ? NoiseWorld->MaskAt(Heard.Location) : 0.0; // DEFECT')]),
    ('N18-ambient-duty-cycle-ignored', [(MECH,
        '\treturn A.Period <= 0.0 || FMath::Fmod(Seconds + A.Phase, A.Period) < A.OnSeconds;', '\treturn true; // DEFECT')]),
    ('N19-idle-creature-keeps-its-own-navigation', [(RULES,
        '\treturn State != EGLCreatureState::Idle && IsActiveHostile(State);', '\treturn IsActiveHostile(State); // DEFECT')]),
    ('N20-creatures-in-a-region-keep-their-own-navigation', [(REGIONS,
        '\t\tif (Entry.Value.Bounds.IsInside(At))', '\t\tif (false && Entry.Value.Bounds.IsInside(At)) // DEFECT')]),
    ('N21-stale-tile-sweep-disabled', [(REGIONS, '\tSweepStaleTiles(Now);', '\t// DEFECT')]),
    ('N22-mechanism-restore-replays-its-effects', [(MECH,
        '\tRecord->SwitchedAt = -1e9; // at rest: nothing replays',
        '\tUGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Mechanism.Slam"), Record->Location, nullptr); // DEFECT\n\tRecord->SwitchedAt = -1e9;')]),
]


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects-P9')
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
                    assert mut.count(a) == 1, f'{name}: anchor not found exactly once in {f}'
                    mut = mut.replace(a, b, 1)
            work.append((f, p, orig, mut))
        with open(os.path.join(evidence, name + '.diff'), 'w') as out:
            for f, p, orig, mut in work:
                out.writelines(difflib.unified_diff(orig.splitlines(1), mut.splitlines(1), 'a/' + f, 'b/' + f))
        for f, p, orig, mut in work:
            shutil.copy(p, p + '.defectbak')
        try:
            for f, p, orig, mut in work:
                open(p, 'w').write(mut)
            built = subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True, text=True)
            if 'RESULT: PASS' not in built.stdout:
                verdict = 'BUILD FAILED'
            else:
                out = subprocess.run([os.path.join(R, 'Tools', 'test.sh'), '--no-build', TESTS], capture_output=True, text=True).stdout
                report = sorted(glob.glob(os.path.join(R, '.test-reports', '*/')), key=os.path.getmtime)[-1]
                fails = [l.strip() for l in out.splitlines() if l.strip().startswith(('Fail', 'Error'))]
                if not os.path.exists(report + 'index.json'):
                    verdict = 'RUN DIED (no test report): not counted as caught'  # a crash is not a detection
                else:
                    shutil.copy(report + 'index.json', os.path.join(evidence, name + '.index.json'))
                    verdict = f'CAUGHT ({len(fails)} tests)' if fails else 'SURVIVED'
        finally:
            for f, p, orig, mut in work:
                shutil.move(p + '.defectbak', p)
                os.utime(p)  # newer than the defect's object files, or UBT keeps the defective binary
        results.append((name, verdict))
        print(name, verdict, flush=True)
    subprocess.run([os.path.join(R, 'Tools', 'build.sh')], capture_output=True)  # the restored source
    caught = sum(v.startswith('CAUGHT') for _, v in results)
    summary = '\n'.join(f'{n}: {v}' for n, v in results) + f'\n{caught}/{len(results)} caught\n'
    open(os.path.join(evidence, 'summary.txt'), 'w').write(summary)
    print(f'RESULT: {"PASS" if caught == len(results) else "FAIL"} {caught}/{len(results)} planted defects caught')
    sys.exit(0 if caught == len(results) else 1)


if __name__ == '__main__':
    main()
