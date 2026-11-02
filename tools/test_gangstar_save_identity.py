"""Save imports keep their native identity; bad input/I/O never resets progress."""
from pathlib import Path
import ctypes as C,struct,tempfile,subprocess,os

def hash_native(data):
 table=[0]*256;seed=0x100001
 for i in range(256):
  for j in range(5):
   seed=(seed*125+3)%0x2aaaab;high=(seed&65535)<<16
   seed=(seed*125+3)%0x2aaaab
   if not j:table[i]=high|(seed&65535)
 a,b=0x7fed7fed,0xeeeeeeee
 for x in data:a=(table[x]^(a+b))&0xffffffff;b=(x+a+b+(b<<5)+3)&0xffffffff
 return a

def encrypt(data,key):
 out=bytearray();k=struct.unpack('<4I',key)
 for off in range(0,len(data),8):
  a,b=struct.unpack_from('<2I',data,off);total=0
  for _ in range(32):
   total=(total+0x9e3779b9)&0xffffffff
   a=(a+(((b<<4)+k[0])^((b>>5)+k[1])^(b+total)))&0xffffffff
   b=(b+(((a<<4)+k[2])^((a>>5)+k[3])^(a+total)))&0xffffffff
  out+=struct.pack('<2I',a,b)
 return bytes(out)

def block(tag,data):return tag+struct.pack('<I',len(data))+data

def pair(identity):
 key=bytes(range(16));header=lambda kind:struct.pack('<i5I',-3,32,0x01010530,kind,12345,0xffffffff)+key
 identity=identity.encode();main=block(b' VFD',struct.pack('<I',len(identity))+identity)+block(b'HSAC',struct.pack('<I',87556))+block(b' LVL',struct.pack('<I',9))
 main+=block(b'XDNE',struct.pack('<I',hash_native(header(1)+main)))
 main+=bytes((-len(main))%8)
 missions=header(2)+block(b'KSMM',struct.pack('<II',1,2))+block(b'XDNE',struct.pack('<I',0xffffffff))
 return header(1)+encrypt(main,key),missions

with tempfile.TemporaryDirectory(prefix='rio-save-') as temp:
 p=Path(temp);faults=p/'faults.c'
 faults.write_text('''#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
int fail_write,fail_rename,nonreplacing_fs,fail_commit_after_move;
ssize_t __real_write(int,const void *,size_t);
int __real_rename(const char *,const char *);
ssize_t __wrap_write(int fd,const void *data,size_t n){if(fail_write){errno=ENOSPC;return -1;}return __real_write(fd,data,n);}
int __wrap_rename(const char *a,const char *b){
 if(fail_rename){errno=EIO;return -1;}
 if(nonreplacing_fs && access(b,F_OK)==0){errno=EEXIST;return -1;}
 if(fail_commit_after_move && strstr(a,".pending-")){errno=EIO;return -1;}
 return __real_rename(a,b);
}
''')
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC','-Isource','source/gangstar_save_identity.c',str(faults),'-Wl,--wrap=write','-Wl,--wrap=rename','-o',str(p/'save.so')],check=True)
 lib=C.CDLL(str(p/'save.so'));lib.gangstar_save_prepare.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.c_void_p,C.c_size_t];lib.gangstar_save_prepare.restype=C.c_int
 lib.gangstar_save_read_identity.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.gangstar_save_read_identity.restype=C.c_int
 def prepare(root,success=True):
  identity=C.create_string_buffer(48);error=C.create_string_buffer(256)
  result=lib.gangstar_save_prepare(str(root).encode(),b'vita-new-install',identity,error,len(error))
  assert bool(result)==success,(result,error.value)
  if not success:assert not identity.value and error.value
  return identity.value.decode()
 def stage(root,identity='vita-imported'):
  (root/'saves').mkdir(parents=True,exist_ok=True);a,b=pair(identity)
  (root/'saves/Gangstar3.sav').write_bytes(a);(root/'saves/Missions.sav').write_bytes(b)
  return a,b
 def assert_pair(root,a,b):assert (root/'saves/Gangstar3.sav').read_bytes()==a and (root/'saves/Missions.sav').read_bytes()==b
 def snapshots(root):return sorted((root/'save-backups').glob('*/complete'))
 fresh=p/'fresh';fresh.mkdir();assert prepare(fresh)=='vita-new-install';assert prepare(fresh)=='vita-new-install'
 imported=p/'imported';a,b=stage(imported);assert prepare(imported)=='vita-imported';assert_pair(imported,a,b)
 snapshot=snapshots(imported)[0].parent;assert (snapshot/'Gangstar3.sav').read_bytes()==a and (snapshot/'Missions.sav').read_bytes()==b
 (imported/'installation-id.txt').write_text('vita-conflicting\n');assert prepare(imported)=='vita-imported';assert_pair(imported,a,b)
 assert any((f.parent/'installation-id.txt').exists() and (f.parent/'installation-id.txt').read_bytes()==b'vita-conflicting\n' for f in snapshots(imported))
 (imported/'installation-id.txt').unlink();assert prepare(imported)=='vita-imported';assert_pair(imported,a,b)
 assert prepare(imported)=='vita-imported';count=len(snapshots(imported));assert prepare(imported)=='vita-imported' and len(snapshots(imported))==count
 a,b=stage(imported,'android-device-uuid');assert prepare(imported)=='android-device-uuid';assert_pair(imported,a,b)
 incomplete=p/'incomplete';a,b=stage(incomplete);(incomplete/'saves/Missions.sav').unlink();prepare(incomplete,False);assert (incomplete/'saves/Gangstar3.sav').read_bytes()==a and not (incomplete/'installation-id.txt').exists()
 for which in ('Gangstar3.sav','Missions.sav'):
  bad=p/('bad-'+which);a,b=stage(bad);(bad/'saves'/which).write_bytes((bad/'saves'/which).read_bytes()[:-1]);prepare(bad,False)
  assert not (bad/'installation-id.txt').exists() and snapshots(bad)
 blocked=p/'blocked';a,b=stage(blocked);(blocked/'save-backups').write_text('occupied');prepare(blocked,False);assert_pair(blocked,a,b)
 io=p/'io';a,b=stage(io);(io/'installation-id.txt').write_text('old\n');C.c_int.in_dll(lib,'fail_write').value=1;prepare(io,False);C.c_int.in_dll(lib,'fail_write').value=0;assert_pair(io,a,b);assert (io/'installation-id.txt').read_bytes()==b'old\n'
 C.c_int.in_dll(lib,'fail_rename').value=1;prepare(io,False);C.c_int.in_dll(lib,'fail_rename').value=0;assert_pair(io,a,b);assert (io/'installation-id.txt').read_bytes()==b'old\n';assert prepare(io)=='vita-imported'
 # Non-replacing Vita-style rename and failed second-stage commit preserve IDs.
 vita=p/'vita-fs';a,b=stage(vita);(vita/'installation-id.txt').write_text('old-vita-id\n')
 C.c_int.in_dll(lib,'nonreplacing_fs').value=1;C.c_int.in_dll(lib,'fail_commit_after_move').value=1
 prepare(vita,False);assert_pair(vita,a,b);assert (vita/'installation-id.txt').read_bytes()==b'old-vita-id\n'
 C.c_int.in_dll(lib,'fail_commit_after_move').value=0;assert prepare(vita)=='vita-imported';assert_pair(vita,a,b)
 assert any(f.read_bytes()==b'old-vita-id\n' for f in vita.glob('installation-id.txt.previous-*'))
 C.c_int.in_dll(lib,'nonreplacing_fs').value=0
 # Actual parser rejects bad checksum, truncated records, wrong version, and missing IDs.
 a,b=pair('vita-reader');buf=C.create_string_buffer(a);result=C.create_string_buffer(48)
 assert lib.gangstar_save_read_identity(buf,len(a),result) and result.value==b'vita-reader' and buf.raw[:len(a)]==a
 for bad in (a[:-1],a+b'\0'*8,a[:8]+b'bad!'+a[12:],a[:45]+bytes([a[45]^128])+a[46:]):
  assert not lib.gangstar_save_read_identity(C.create_string_buffer(bad),len(bad),result)
 # Exercise the same parser and startup I/O under sanitizers, without putting
 # any proprietary save bytes into the repository.
 driver=p/'sanitized.c';driver.write_text(r'''
#include "gangstar_save_identity.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
int main(int argc,char **argv){
 assert(argc==3);char id[48],error[256];
 assert(gangstar_save_prepare(argv[1],"vita-sanitizer",id,error,sizeof(error)));
 assert(!strcmp(id,"android-device-uuid"));
 assert(!gangstar_save_prepare(argv[2],"vita-sanitizer",id,error,sizeof(error)));
 char name[1024];snprintf(name,sizeof(name),"%s/saves/Gangstar3.sav",argv[1]);FILE *f=fopen(name,"rb");assert(f);
 unsigned char header[40];assert(fread(header,1,40,f)==40);assert(!fclose(f));
 uint8_t *data=malloc(2048);assert(data);uint32_t seed=1;
 for(unsigned iteration=0;iteration<3000;++iteration){
  seed=seed*1664525u+1013904223u;size_t n=48+(seed%2000);
  for(size_t i=0;i<n;++i){seed=seed*1664525u+1013904223u;data[i]=(uint8_t)(seed>>24);}
  if(iteration%2)memcpy(data,header,40);
  (void)gangstar_save_read_identity(data,n,id);
 }
 free(data);return 0;
}
''')
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isource','source/gangstar_save_identity.c',str(driver),'-o',str(p/'sanitized')],check=True)
 subprocess.run([str(p/'sanitized'),str(imported),str(incomplete)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 dialog=Path('source/utils/dialog.c').read_text();handler=dialog[dialog.index('void save_protection_error('):]
 dialog_test=p/'dialog.c';dialog_test.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
static jmp_buf exit_jump;static int flushed,ready,polls,frames,unavailable;
#define GL_COLOR_BUFFER_BIT 1
#define GL_TRUE 1
#define l_error(...) ((void)0)
static void gangstar_log_finish(void){flushed=1;}
static void gl_init(void){assert(flushed);ready=1;}
static int init_msg_dialog(const char *text){assert(ready && strstr(text,"Save protection") && strstr(text,"test reason"));return unavailable?-1:0;}
static int get_msg_dialog_result(void){return ++polls==4;}
static void glClearColor(float r,float g,float b,float a){assert(r==0 && g==0 && b==0 && a==1);}
static void glClear(int mask){assert(mask==GL_COLOR_BUFFER_BIT);}
static void vglSwapBuffers(int dialogs){assert(dialogs==GL_TRUE);++frames;}
static void sceKernelDelayThread(unsigned us){assert(us==16000);}
static void sceKernelExitProcess(int status){assert(status==1);longjmp(exit_jump,1);}
'''+handler+r'''
int main(void){
 if(!setjmp(exit_jump))save_protection_error("test reason");
 assert(flushed && ready && frames==3 && polls==4);
 unavailable=1;frames=polls=flushed=ready=0;
 if(!setjmp(exit_jump))save_protection_error("test reason");
 assert(flushed && ready && !frames && !polls);return 0;
}
''')
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(dialog_test),'-o',str(p/'dialog')],check=True)
 subprocess.run([str(p/'dialog')],check=True)
 main=Path('source/main.c').read_text();assert main.index('    gangstar_save_startup();')<main.index('    gangstar_java_install();')
 java=Path('source/gangstar_java.c').read_text();assert 'gangstar_save_prepare(DATA_PATH' in java and 'save_protection_error(error)' in java
print('Save identity: checksum-verified imports, missing/conflicting ID adoption, immutable snapshots/dedup, incomplete/corrupt save stop, failed writes/rename, native startup order, visible error dialog, ASan/UBSan parser checks and unchanged source bytes passed')
