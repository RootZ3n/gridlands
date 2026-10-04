# Summarizes the A/B runs in a folder: <run>.json (gl.Perf crossing) + <run>.timing.txt (the unload/GC log lines).
import json,re,sys,glob
def row(f):
    d=json.load(open(f+'.json'))
    log=open(f+'.timing.txt',errors='ignore').read()
    ret=[float(x) for x in re.findall(r'retired \d+ player parts in ([\d.]+) ms',log)]
    gc=[h for h in d['hitches'] if h['garbageCollected']]
    gco=d.get('gcObjects',[])
    rec=max([g['liveBefore']-g['liveAfter'] for g in gco] or [0])
    return dict(run=f.split('/')[-1],worst=round(d['worstFrameMs'],1),stream=round(d['streamingGameThreadMsWorst'],2),retire=ret,gcWorstGT=round(max([h['gameThreadMs'] for h in gc] or [0]),1),gcMaxReclaimed=rec,liveEnd=d.get('liveObjectsAtEnd'),mem=round(d['memPeakMb']),pres=round(d['presentationMsWorst'],2),gtMean=round(d['gameThreadMsMean'],2),p99=round(d['frameMsP99'],2))
for f in sorted(glob.glob(sys.argv[1]+'/*.json')):
    print(row(f[:-5]))
