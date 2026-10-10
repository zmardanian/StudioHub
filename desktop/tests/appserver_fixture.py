#!/usr/bin/env python3
"""Protocol fixture for deterministic client tests; never used by the product."""
import json, os, sys

def send(value):
    # Exercise incremental frames across writes.
    text = json.dumps(value) + '\n'
    sys.stdout.write(text[:7]); sys.stdout.flush()
    sys.stdout.write(text[7:]); sys.stdout.flush()

def event(method, params): send({'method':method,'params':params})
for line in sys.stdin:
    q = json.loads(line)
    if q.get('id') == 'legacy-patch-fixture':
        assert q['result']['decision'] == 'approved'
        event('item/commandExecution/outputDelta', {'delta':'legacy patch approval verified\n'})
        continue
    if q.get('id') == 'unsupported-fixture':
        assert q['error']['code'] == -32601
        event('item/commandExecution/outputDelta', {'delta':'unsupported request rejected\n'})
        continue
    if q.get('id') == 'approval-fixture' and 'result' in q:
        assert q['result']['decision'] == 'accept'
        event('item/commandExecution/outputDelta', {'delta':'automatic approval verified\n'})
        continue
    method = q.get('method'); p = q.get('params', {})
    result = {}
    if method == 'initialize':
        if os.environ.get('STUDIOHUB_FIXTURE_HANG') == '1': continue
        result = {'userAgent':'fixture'}
    elif method == 'account/read': result = {'account':{'type':'chatgpt'},'requiresOpenaiAuth':True}
    elif method == 'model/list':
        result = {'data':[{'model':'gpt-6-luna','id':'gpt-6-luna','isDefault':True,'defaultReasoningEffort':'low',
            'supportedReasoningEfforts':[{'reasoningEffort':'low'},{'reasoningEffort':'high'}]}]}
    elif method in ('thread/start','thread/resume'):
        assert p['model'] == 'gpt-6-luna'
        assert p['config']['model_reasoning_effort'] == 'low'
        result = {'thread':{'id':p.get('threadId','fixture-thread')},'model':p['model'],
                  'reasoningEffort':'low','cwd':os.path.realpath(p['cwd']),
                  'sandbox':{'type':'workspaceWrite'},'approvalPolicy':'on-request','approvalsReviewer':'user'}
        event('item/commandExecution/outputDelta', {'delta':method+' observed\n'})
    elif method == 'turn/start':
        assert p['model']=='gpt-6-luna' and p['effort']=='low'
        result={'turn':{'id':'fixture-turn','status':'inProgress'}}
    elif method == 'turn/interrupt':
        assert p['turnId']=='fixture-turn'
        event('turn/completed', {'turn':{'id':'fixture-turn','status':'interrupted'}})
    elif method in ('initialized', None): continue
    else: raise AssertionError(method)
    send({'id':q['id'],'result':result})
    if method == 'turn/start':
        if os.environ.get('STUDIOHUB_FIXTURE_EXIT') == '1': sys.exit(17)
        send({'id':'approval-fixture','method':'item/commandExecution/requestApproval','params':{'command':'fixture safe command','cwd':p['cwd']}})
        send({'id':'legacy-patch-fixture','method':'applyPatchApproval','params':{}})
        send({'id':'unsupported-fixture','method':'unsupported/requestApproval','params':{}})
        event('item/agentMessage/delta',{'delta':'message finished but turn active'})
        event('item/completed',{'item':{'type':'agentMessage','text':'message finished but turn active; final payload retained'}})
        event('turn/completed',{'turn':{'id':'unrelated-turn','status':'completed'}})
        event('turn/completed',{'turn':{'id':'fixture-turn','status':'inProgress'}})
