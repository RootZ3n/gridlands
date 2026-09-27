#!/usr/bin/env python3
"""Planted defects for the one terrain surface, terrain collision and the chunk pool (ADR-0034, ADR-0035): a permanent gate.

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
CORE = 'Source/GridlandsCore/Private/Terrain/GLHeightfield.cpp'
COREH = 'Source/GridlandsCore/Public/Terrain/GLHeightfield.h'
BUILD = 'Source/GridlandsGame/Private/Building/GLBuildingSubsystem.cpp'
STRUCT = 'Source/GridlandsGame/Private/Structure/GLStructureSubsystem.cpp'
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
    ('H9-render-diagonal-differs-from-collision', [(CHUNK,
        '\t\t\tBuilt.AppendTriangle(T[0], T[1], T[2]);\n\t\t\tBuilt.AppendTriangle(T[3], T[4], T[5]);',
        '\t\t\tBuilt.AppendTriangle(T[0], T[1], T[5]); // DEFECT: the render mesh split B-C, the rest A-D\n\t\t\tBuilt.AppendTriangle(T[5], T[1], T[2]);')]),
    ('H10-collision-relevance-never-refreshed', [(COLL,
        '\tconst bool bWas = bNavigationRelevant;\n\tbNavigationRelevant = IsNavigationRelevant();',
        '\tconst bool bWas = bNavigationRelevant;\n\treturn; // DEFECT')]),
    # --- the one terrain surface (visible = collision = gameplay), 2026-09-27
    ('S1-heightat-restores-the-old-bilinear', [(CORE,
        '\treturn GLTerrainSurface::Height(VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0), VertexHeight(X0, Y0 + 1), VertexHeight(X0 + 1, Y0 + 1), FX - X0, FY - Y0);',
        '\tconst double TX = FX - X0, TY = FY - Y0; // DEFECT: the old bilinear interpolation\n\treturn FMath::Lerp(FMath::Lerp<double>(VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0), TX), FMath::Lerp<double>(VertexHeight(X0, Y0 + 1), VertexHeight(X0 + 1, Y0 + 1), TX), TY);')]),
    ('S2-wrong-triangle-inside-the-quad', [(COREH,
        '\t\treturn FY >= FX ? HA + FX * (HD - HC) + FY * (HC - HA)   // triangle (A, C, D)',
        '\t\treturn FY <= FX ? HA + FX * (HD - HC) + FY * (HC - HA)   // DEFECT: the other triangle\'s plane')]),
    ('S3-diagonal-reversed-everywhere', [(COREH,
        '\t\treturn FY >= FX ? HA + FX * (HD - HC) + FY * (HC - HA)   // triangle (A, C, D)\n\t\t                : HA + FX * (HB - HA) + FY * (HD - HB);  // triangle (A, D, B)',
        '\t\treturn FX + FY <= 1.0 ? HA + FX * (HB - HA) + FY * (HC - HA) : HD + (1.0 - FX) * (HC - HD) + (1.0 - FY) * (HB - HD); // DEFECT: B-C split'),
        (COREH, '\t\tOut[0] = A; Out[1] = C; Out[2] = D;\n\t\tOut[3] = A; Out[4] = D; Out[5] = B;',
        '\t\tOut[0] = A; Out[1] = C; Out[2] = B; // DEFECT: B-C split\n\t\tOut[3] = B; Out[4] = C; Out[5] = D;')]),
    ('S4-wrong-plane-in-a-triangle', [(COREH,
        '\t\t                : HA + FX * (HB - HA) + FY * (HD - HB);  // triangle (A, D, B)',
        '\t\t                : HA + FX * (HB - HA) + FY * (HD - HA);  // DEFECT: wrong barycentric term')]),
    ('S5-exact-vertices-wrong-interior', [(CORE,
        'VertexHeight(X0 + 1, Y0 + 1), FX - X0, FY - Y0);',
        'VertexHeight(X0 + 1, Y0 + 1), FMath::SmoothStep(0.0, 1.0, FX - X0), FMath::SmoothStep(0.0, 1.0, FY - Y0)); // DEFECT: exact at vertices only')]),
    ('S6-building-bypasses-the-terrain-query', [(BUILD,
        '\treturn Terrain && Terrain->HasGround() ? Terrain->HeightAt(At) : 0.0;',
        '\treturn Terrain && Terrain->HasGround() ? Terrain->HeightAt(FVector2D(FMath::RoundToDouble(At.X / 100.0) * 100.0, FMath::RoundToDouble(At.Y / 100.0) * 100.0)) : 0.0; // DEFECT: its own (nearest-vertex) ground')]),
    ('S6b-structures-bypass-the-terrain-query', [(STRUCT,
        '\treturn Terrain ? Terrain->HeightAt(At) : 0.0;',
        '\treturn Terrain ? Terrain->HeightAt(FVector2D(FMath::RoundToDouble(At.X / 100.0) * 100.0, FMath::RoundToDouble(At.Y / 100.0) * 100.0)) : 0.0; // DEFECT: its own ground')]),
    ('S7-seam-quads-use-the-other-diagonal', [(CORE,
        '\treturn GLTerrainSurface::Height(VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0), VertexHeight(X0, Y0 + 1), VertexHeight(X0 + 1, Y0 + 1), FX - X0, FY - Y0);',
        '\tif (X0 % 64 == 63) { return GLTerrainSurface::Height(VertexHeight(X0 + 1, Y0), VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0 + 1), VertexHeight(X0, Y0 + 1), 1.0 - (FX - X0), FY - Y0); } // DEFECT: seam quads split B-C\n\treturn GLTerrainSurface::Height(VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0), VertexHeight(X0, Y0 + 1), VertexHeight(X0 + 1, Y0 + 1), FX - X0, FY - Y0);')]),
    ('S8-stale-gameplay-ground-after-an-edit', [(TERR,
        '\treturn Ground ? Ground->Field.HeightAt(World) :',
        '\tthread_local TMap<FVector2D, double> Seen; if (const double* Old = Seen.Find(World)) { return *Old; } // DEFECT: remembers ground across edits\n\tif (Ground) { return Seen.Add(World, Ground->Field.HeightAt(World)); }\n\treturn Ground ? Ground->Field.HeightAt(World) :')]),
    ('S9-restore-leaves-gameplay-on-the-old-ground', [
        (TERR, '\tconst FGLHeightfield Old = MoveTemp(Ground->Field);\n\tGround->Field = MoveTemp(Fresh);', '\tconst FGLHeightfield Old = Ground->Field; // DEFECT: gameplay keeps the pre-restore ground'),
        (TERR, '\t\t\t\tbChanged = Old.VertexHeight(X, Y) != Ground->Field.VertexHeight(X, Y);', '\t\t\t\tbChanged = Old.VertexHeight(X, Y) != Fresh.VertexHeight(X, Y);'),
        (TERR, '\t\t\t\tSlot.Actor->Rebuild(Ground->Field, true);\n\t\t\t\tSlot.BuiltVersion = Slot.Version;\n\t\t\t}\n\t\t}\n\t}', '\t\t\t\tSlot.Actor->Rebuild(Fresh, true);\n\t\t\t\tSlot.BuiltVersion = Slot.Version;\n\t\t\t}\n\t\t}\n\t}')]),
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
