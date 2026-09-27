#!/usr/bin/env python3
"""Planted defects for terrain collision and the chunk pool (ADR-0034, ADR-0035): a permanent gate.

Each defect is written into a backup-protected copy of the source, built and tested on its own; the source
is restored after every defect (never with git checkout: the tree may hold uncommitted work). Every defect
must be CAUGHT (a required test fails). Reports: <evidence dir>/<name>.diff and .index.json, summary.txt.

Usage: Tools/planted-defects/terrain_collision.py [--evidence DIR] [NAME ...]
"""
import difflib, glob, os, shutil, subprocess, sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CHUNK = 'Source/GridlandsGame/Private/Terrain/GLTerrainChunk.cpp'
COLL = 'Source/GridlandsGame/Private/Terrain/GLTerrainCollision.cpp'
TERR = 'Source/GridlandsGame/Private/Terrain/GLTerrainSubsystem.cpp'
TESTS = 'Gridlands.Game.TerrainCollision+Gridlands.Game.TerrainPool+Gridlands.Game.Terrain+Gridlands.Game.Navigation'

# (name, [(file, exact text, replacement)], what a regression would look like)
DEFECTS = [
    ('H1-edit-updates-the-visible-mesh-not-collision', [(CHUNK,
        '\t\tCollision->SetGeometry(MoveTemp(Geometry)); // the body is recreated now: no window without collision',
        '\t\tif (!bBuilt) { Collision->SetGeometry(MoveTemp(Geometry)); } // DEFECT: rebuilds keep the old body')]),
    ('H2-edit-updates-collision-not-the-visible-mesh', [(CHUNK,
        '\t\tEnsureSeparateCollision();\n\t\tMesh->SetMesh(MoveTemp(Built));',
        '\t\tEnsureSeparateCollision();\n\t\tif (!bBuilt) { Mesh->SetMesh(MoveTemp(Built)); } // DEFECT')]),
    ('H3-seam-column-of-collision-stale', [(COLL,
        '\t\t\t\tHeights[Row * V + Col] = H(Col, Row);',
        '\t\t\t\tHeights[Row * V + Col] = H(FMath::Min(Col, V - 2), Row); // DEFECT: last column copies its neighbour')]),
    ('H4-collision-built-without-saved-deformation', [(COLL,
        'return static_cast<double>(Snap.Heights[(Y + 1) * Side + (X + 1)]); };',
        'return static_cast<double>(Snap.Base[(Y + 1) * Side + (X + 1)]); }; // DEFECT: base, not edited, heights')]),
    ('H5-pooled-chunk-keeps-its-collision-body', [(CHUNK,
        '\t\tCollision->SetGeometry(nullptr); // the body goes now\n', '')]),
    ('H6-navigation-sees-old-terrain', [(CHUNK,
        '\t\tif (bNotifyNavigation)\n\t\t{\n\t\t\tUNavigationSystemV1::UpdateComponentInNavOctree(*Collision);',
        '\t\tif (false)\n\t\t{\n\t\t\tUNavigationSystemV1::UpdateComponentInNavOctree(*Collision); // DEFECT')]),
    ('H7-older-generation-collision-wins', [(TERR,
        '!Ground || Ground->Generation != Job->Generation ||', '!Ground ||')]),
    ('H8-single-default-material-makes-holes', [(COLL,
        '\t\tMaterials.SetNumZeroed((V - 1) * (V - 1));', '\t\tMaterials.SetNumZeroed(1); // DEFECT: the "default material" form')]),
    ('H9-render-diagonal-differs-from-collision', [(COLL,
        '\treturn GetMode() == EGLTerrainCollisionMode::Heightfield;', '\treturn false; // DEFECT: the other render split over a heightfield')]),
    ('H10-collision-relevance-never-refreshed', [(COLL,
        '\tconst bool bWas = bNavigationRelevant;\n\tbNavigationRelevant = IsNavigationRelevant();',
        '\tconst bool bWas = bNavigationRelevant;\n\treturn; // DEFECT')]),
    ('T1-pooled-chunk-keeps-its-mesh', [(CHUNK,
        '\tMesh->SetMesh(UE::Geometry::FDynamicMesh3());\n', '')]),
    ('T1b-cleared-chunk-still-marked-built', [(CHUNK,
        '\tbBuilt = false;\n\tOwnerCell = NAME_None;', '\tOwnerCell = NAME_None;')]),
    ('T2-reused-chunk-keeps-previous-cell-identity', [(TERR, '\tChunk->OwnerCell = Ground.Cell;', '')]),
    ('T4-reused-chunk-keeps-its-previous-place', [(TERR,
        '\t\tChunk->SetActorLocation(FVector(At.X, At.Y, 0.0));\n', '')]),
    ('T6-pool-ignores-its-bound', [(TERR, '\tif (Pool.Num() < PoolLimit)', '\tif (true)')]),
    ('T7b-live-chunk-left-in-the-pool', [(TERR,
        '\tChunk->OwnerCell = Ground.Cell;', '\tChunk->OwnerCell = Ground.Cell;\n\tPool.Add(Chunk); // DEFECT: still handed out')]),
    ('T8-cancellation-pools-a-half-reset-chunk', [(TERR,
        '\tif (Pool.Num() < PoolLimit)\n\t{\n\t\tChunk->ClearForPool();', '\tif (Pool.Num() < PoolLimit)\n\t{')]),
]


def main():
    args = sys.argv[1:]
    evidence = os.path.join(R, 'Saved', 'PlantedDefects')
    if args[:1] == ['--evidence']:
        evidence, args = os.path.abspath(args[1]), args[2:]
    os.makedirs(evidence, exist_ok=True)
    chosen = [d for d in DEFECTS if not args or d[0] in args]
    results = []
    for name, edits in chosen:
        work = []
        for f, a, b in edits:
            p = os.path.join(R, f)
            orig = open(p).read()
            assert orig.count(a) == 1, f'{name}: anchor not found exactly once in {f}'
            work.append((f, p, orig, orig.replace(a, b, 1)))
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
                if os.path.exists(report + 'index.json'):
                    shutil.copy(report + 'index.json', os.path.join(evidence, name + '.index.json'))
                fails = [l.strip() for l in out.splitlines() if l.strip().startswith(('Fail', 'Error'))]
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
