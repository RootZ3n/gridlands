#!/usr/bin/env python3
"""Planted defects for P8: gameplay models before gameplay actors (glitches, salvage nodes, creatures), the
production-density town block, and the LOD pipeline's runtime and data contract (ADR-0036). A permanent gate.

Same method as terrain_collision.py: each defect is written into a backup-protected copy of the source (or
data), built and tested on its own, and restored after every defect (never with git checkout: the tree may
hold uncommitted work). Every defect must be CAUGHT (a required test fails); a dead run is not a catch.
The importer's own LOD-budget check is proven by a real catch recorded in the P8 evidence (it rejected
SM_GrassTuft and SM_K50_Roof at 64 > 60 triangles); the validator rules VIS-3/4/5 by Tools/selftest.sh.

Usage: Tools/planted-defects/p8_split_lod.py [--evidence DIR] [NAME ...]
"""
import difflib, glob, os, shutil, subprocess, sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
PLACE = 'Source/GridlandsGame/Private/World/GLPlacementSubsystem.cpp'
GLITCHS = 'Source/GridlandsGame/Private/Glitch/GLGlitchSubsystem.cpp'
GLITCHC = 'Source/GridlandsGame/Private/Glitch/GLGlitchComponent.cpp'
CREATURE = 'Source/GridlandsGame/Private/Combat/GLCreature.cpp'
SALV = 'Source/GridlandsGame/Private/Salvage/GLSalvageableComponent.cpp'
SAVE = 'Source/GridlandsGame/Private/Save/GLSaveSubsystem.cpp'
VIS = 'Source/GridlandsGame/Private/Presentation/GLVisuals.cpp'
SCATTER = 'Source/GridlandsGame/Private/Presentation/GLScatterPatch.cpp'
PINE = 'Data/visual/nature/pine.json'
TESTS = 'Gridlands.Game.ActorPresentation+Gridlands.Game.TownBlock+Gridlands.Game.Lod+Gridlands.Game.Presentation+Gridlands.Game.Streaming+Gridlands.Game.Grid'

PINE_TRIS = '      5000,\n      2000,\n      750\n    ]'
PINE_SCREENS = '      1.0,\n      0.35,\n      0.12\n    ]'

# (name, [(file, exact text, replacement)])
DEFECTS = [
    # --- model / presentation separation
    ('M1-salvaged-node-is-presented-anyway', [(PLACE,
        'if (!Model || Model->Actor.IsValid() || Model->bSalvaged || Model->bDefeated)',
        'if (!Model || Model->Actor.IsValid() || Model->bDefeated) // DEFECT: a salvaged node comes back')]),
    ('M2-defeated-creature-is-remade', [(PLACE,
        'if (!Model || Model->Actor.IsValid() || Model->bSalvaged || Model->bDefeated)',
        'if (!Model || Model->Actor.IsValid() || Model->bSalvaged) // DEFECT: a defeated creature comes back')]),
    ('M3-glitch-actor-starts-latent-not-from-its-model', [(GLITCHS,
        '\tGlitch->GetGlitch()->PresentFromModel(Record->State, Record->ProgressSeconds, Record->bItemsDelivered, FGLPresentAuthority());',
        '\t// DEFECT: presented in its default state, not the model\'s')]),
    ('M4-glitch-changes-never-reach-the-model', [(GLITCHC,
        '\t\tGlitches->SyncFromComponent(*this);', '\t\t// DEFECT: the actor keeps its state to itself')]),
    ('M5-creature-death-never-reaches-the-model', [(CREATURE,
        '\t\tPlacements->MarkDefeated(PlacementId);', '\t\t// DEFECT: only the actor knows it died')]),
    ('M6-salvage-never-reaches-the-model', [(PLACE,
        '\t\t\tSalvaged->bSalvaged = true;', '\t\t\t// DEFECT: only the actor knows it was salvaged')]),
    ('M7-restore-skips-the-unpresented-model', [(PLACE,
        '\tModel->bSalvaged = true;\n\tif (AGLSalvageNode* Node = Cast<AGLSalvageNode>(Model->Actor.Get()))',
        '\tif (!Model->Actor.IsValid()) { return true; } // DEFECT: saved state waits for the actor\n\tModel->bSalvaged = true;\n\tif (AGLSalvageNode* Node = Cast<AGLSalvageNode>(Model->Actor.Get()))')]),
    ('M8-save-reads-only-presented-glitches', [(SAVE,
        '\t\tif (UGLPlacementSubsystem::IsPlacementOfCell(Glitch.Placement, Cell))',
        '\t\tif (UGLPlacementSubsystem::IsPlacementOfCell(Glitch.Placement, Cell) && Glitch.Actor.IsValid()) // DEFECT: from actors')]),
    ('M9-unload-keeps-waiting-actors', [(PLACE,
        '\tPendingActors.RemoveAll([CellId](const FGLPendingActor& P) { return P.Cell == CellId; }); // cancelled presentation',
        '\t// DEFECT: presentation of an unloaded cell is not cancelled')]),
    ('M10-retired-node-stays-live', [(SALV,
        '\t\tOwner->SetActorHiddenInGame(true);\n\t\tOwner->SetActorEnableCollision(false);\n\t}\n}\n\nvoid UGLSalvageableComponent::Complete',
        '\t}\n}\n\nvoid UGLSalvageableComponent::Complete')]),
    ('M11-retired-actors-never-destroyed', [(PLACE,
        '\twhile (RetiringActors.Num() > 0 && HasBudget())', '\twhile (false && RetiringActors.Num() > 0) // DEFECT')]),
    ('M12-actor-presentation-ignores-the-budget', [(PLACE,
        '\t\tif (!bNear && !HasBudget())\n\t\t{\n\t\t\tbreak;\n\t\t}',
        '\t\t// DEFECT: everything at once, paused or not')]),
    # --- LOD pipeline (runtime and the data/asset contract)
    ('L1-instanced-grass-never-culled', [(SCATTER,
        '\tInstances->SetCullDistances(FMath::RoundToInt(CullCm * 0.75), CullCm);', '\t// DEFECT: grass draws at any distance')]),
    ('L2-visual-cull-distance-ignored', [(VIS,
        '\t\tMain->SetCullDistance(Def->CullDistance * 100.0);', '\t\t// DEFECT')]),
    ('L3-cull-distance-in-the-wrong-unit', [(SCATTER,
        'const int32 CullCm = FMath::RoundToInt(Def->CullDistance * 100.0);', 'const int32 CullCm = FMath::RoundToInt(Def->CullDistance); // DEFECT: metres as cm')]),
    ('L4-visual-mesh-carries-collision', [(VIS,
        '\t\tComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);', '\t\tComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // DEFECT: gameplay would see the mesh')]),
    ('L5-budget-tightened-below-the-built-mesh', [(PINE, PINE_TRIS, '      5000,\n      1000,\n      750\n    ]')]),
    ('L6-screen-size-drifts-from-the-built-mesh', [(PINE, PINE_SCREENS, '      1.0,\n      0.3,\n      0.12\n    ]')]),
    ('L7-budget-declares-a-lod-the-mesh-lacks', [(PINE, PINE_TRIS, '      5000,\n      2000,\n      750,\n      300\n    ]'),
        (PINE, PINE_SCREENS, '      1.0,\n      0.35,\n      0.12,\n      0.05\n    ]')]),
]


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects-P8')
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
