import subprocess,json,sys
p=subprocess.Popen([sys.argv[1]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
def rpc(method,params=None):
 rpc.seq+=1;i=rpc.seq;p.stdin.write(json.dumps(dict(id=i,method=method,params=params or {}))+'\n');p.stdin.flush()
 while True:
  data=json.loads(p.stdout.readline())
  if data.get('id')==i:return data['result']
rpc.seq=0
assert rpc('initialize')['userAgent']=='mock'
p.stdin.write('{"method":"initialized"}\n');p.stdin.flush()
assert rpc('config/read')['config']['model_provider']=='usage_widget_https'
assert rpc('account/rateLimits/read')['rateLimitResetCredits']['availableCount']==2
assert rpc('account/rateLimitResetCredit/consume',{'idempotencyKey':'stable-id'})['outcome']=='reset'
assert rpc('account/rateLimitResetCredit/consume',{'idempotencyKey':'stable-id'})['outcome']=='alreadyRedeemed'
assert rpc('account/rateLimits/read')['rateLimitResetCredits']['availableCount']==1
assert rpc('thread/start')['thread']['id']=='mock-thread'
assert rpc('turn/start',{'threadId':'mock-thread','input':[{'type':'text','text':'hello'}]})['turn']['id']=='mock-turn-1'
assert rpc('thread/delete',{'threadId':'mock-thread'})=={}
p.stdin.close();assert p.wait(timeout=5)==0
print('PASS separate-process fixture: handshake, config, quota/reset counts, idempotent retry, turn stream and deletion protocol')
