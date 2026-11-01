"""Validate offline billing completion without Java or purchase simulation."""
from pathlib import Path
import subprocess,struct,re
s=Path('source/gangstar_iap_offline.c').read_text()
e=Path('../work/libgl2jni.so').read_bytes()
for address,words in ((0x37b2bc,(0xe92d40f0,0xe59f40c0)),(0x378690,(0xe59f3014,0xe79f3003)),(0x37ae74,(0xe92d47f0,0xe59f413c))):
 assert struct.unpack_from('<2I',e,address)==words
 assert ','.join(hex(w) for w in words) in s
assert 'gangstar_iap_offline_poll();\n    if (!accelerometer_enabled)' in Path('source/gangstar_java.c').read_text()
a=s.index('static void (*catalogue_callback)');b=s.index('void gangstar_iap_offline_install(',a)
p=Path('work/tests/iap_offline.c')
p.write_text(r'''
#include <assert.h>
#include <string.h>
#include <stdatomic.h>
static unsigned logs,calls;
static int request_locked,request_again;
#define l_info(...) (++logs)
'''+s[a:b]+r'''
static void complete(const char *category,int count){
 assert(!request_locked && !strcmp(category,"offline") && count==0);++calls;
 if(request_again){request_again=0;offline_catalogue_request("next");}
}
int main(void){
 catalogue_callback=complete;
 assert(offline_billing_state()==0 && offline_total_items()==0);
 for(unsigned i=0;i<1000;++i){gangstar_iap_offline_poll();}
 assert(!calls && !logs);
 request_locked=1;offline_catalogue_request("cash");assert(!calls);request_locked=0;
 gangstar_iap_offline_poll();assert(calls==1 && logs==1);
 gangstar_iap_offline_poll();assert(calls==1);
 request_again=1;offline_catalogue_request("cash");gangstar_iap_offline_poll();
 assert(calls==2);gangstar_iap_offline_poll();assert(calls==3);
 assert(offline_billing_state()==0 && offline_total_items()==0);
}
''')
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/iap_offline'],check=True)
subprocess.run(['work/tests/iap_offline'],check=True)
print('Offline billing: verified native targets, empty catalogue/no pending transaction, no synchronous callback under request lock, callback re-entry, idle polling and accelerometer-independent completion passed')
