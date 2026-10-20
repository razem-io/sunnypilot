"""Checks the running preview without publishing anything to Replay."""
import json,time,urllib.request,urllib.error
BASE='http://127.0.0.1:8774'
def change(source,scenario='manual',values=None):
    req=urllib.request.Request(BASE+'/api/source',json.dumps({'source':source,'scenario':scenario,'values':values or {}}).encode(),headers={'Content-Type':'application/json'})
    return json.load(urllib.request.urlopen(req,timeout=5))
try:
    for case,expected in [('manual',True),('reverse',True),('reverse_alert',False),('manual',True),('assist',False),('lateral',False),('longitudinal',False),('mads',False),('alert',False),('stale',False),('manual',True)]:
        result=change('test',case)
        assert result['source']=='test' and result['show']==expected,(case,result)
    s=change('test','left',{'vEgo':20,'steeringAngleDeg':-300})
    assert (s['speed'],s['wheelAngle'],s['car']['leftBlinker'])==(72,-20,True)
    for values in ({'vEgo':-1},{'steeringAngleDeg':999},{'vEgo':float('nan')},{'unexpected':1}):
        try:change('test','manual',values)
        except urllib.error.HTTPError as e:assert e.code==400
        else:raise AssertionError('Invalid input accepted')
    print('PASS: HTTP transitions, live-unit conversion, isolated fixtures and input rejection')
finally:
    s=change('replay')
    print('Restored original Replay:', {k:s[k] for k in ('source','live','speed','steering','wheelAngle','ratioSource','missing')})
    assert s['source']=='replay' and s['live'], 'Original Replay is not delivering valid carState'
