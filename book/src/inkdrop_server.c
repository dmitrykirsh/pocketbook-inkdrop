/* Use 64-bit filesystem counters on ARM, including for FAT volumes over 2 GiB. */
#define _FILE_OFFSET_BITS 64
/* InkDrop: deliberately small local-only HTTP receiver for PocketBook. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "inkview.h"

#define BASE "/mnt/ext1/system/inkdrop"
#define CFG BASE "/config"
#define PID "/var/tmp/inkdrop.pid"
#define DEST "/mnt/ext1/Downloads"
#define PORT 8790
#define MAX_FILE (500LL * 1024 * 1024)

static volatile sig_atomic_t quit;
static char token[33];
static int check_space(const char *path, struct statvfs *v) { int r=statvfs(path,v);if(r)fprintf(stderr,"statvfs failed: %s\n",strerror(errno));else fprintf(stderr,"available: %llu bytes\n",(unsigned long long)v->f_bavail*v->f_bsize);fflush(stderr);return r; }

static const char *page =
"<!doctype html><html lang=ru><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>InkDrop</title><style>body{margin:0;background:#f2f0eb;color:#171716;font:16px system-ui,-apple-system,sans-serif}main{max-width:620px;margin:auto;padding:24px}header{display:flex;gap:14px;align-items:center;border-bottom:2px solid #171716;padding-bottom:18px}.mark{width:46px;height:46px;border:3px solid;border-radius:14px;display:grid;place-items:center;font-size:25px}h1{margin:0;font-size:28px}p{line-height:1.5}.card{background:#fff;border:1px solid #bbb;border-radius:18px;padding:20px;margin:20px 0}.pick{display:block;border:2px dashed #555;border-radius:14px;padding:28px 12px;text-align:center;cursor:pointer}.pick input{display:none}button{background:#171716;color:#fff;border:0;border-radius:10px;padding:13px 18px;font-weight:700;font-size:16px;margin-top:12px}button:disabled{opacity:.45}small{color:#5b5a55}.formats{font-size:14px;line-height:1.55}#list{padding:0;list-style:none}#list li{padding:10px 0;border-top:1px solid #ddd}.ok{color:#176b3a}.bad{color:#a22720}</style>"
"<main><header><div class=mark>↓</div><div><h1>InkDrop</h1><small>Передача файлов на PocketBook</small></div></header>"
"<p>Выберите книги или изображения. Они будут сохранены на ридере в папке <b>Downloads</b>.</p>"
"<section class=card><label class=pick>Нажмите, чтобы выбрать файлы<input id=f type=file multiple accept='.epub,.fb2,.zip,.pdf,.djvu,.djv,.doc,.docx,.rtf,.txt,.htm,.html,.chm,.cbr,.cbz,.jpg,.jpeg,.png,.bmp,.tif,.tiff'></label><button id=send disabled>Отправить на ридер</button><ul id=list></ul></section>"
"<section class='card formats'><b>Поддерживаются</b><br>Книги: EPUB, FB2 / FB2.ZIP, PDF, DJVU, DOC/DOCX, RTF, TXT, HTML, CHM, CBR/CBZ.<br>Изображения: JPG, PNG, BMP, TIFF.<br><br><small>Телефон и ридер должны быть в одной Wi-Fi сети. Файлы передаются только по локальной сети.</small></section></main>"
"<script>const q=new URLSearchParams(location.search),t=q.get('t'),f=document.querySelector('#f'),b=document.querySelector('#send'),l=document.querySelector('#list');let a=[];f.onchange=()=>{a=[...f.files];b.disabled=!a.length;l.innerHTML=a.map(x=>'<li>'+x.name+' <small>'+Math.ceil(x.size/1024/1024)+' MB</small></li>').join('')};b.onclick=async()=>{b.disabled=1;for(const x of a){let e=document.createElement('li');e.textContent='Отправка: '+x.name;l.append(e);try{let r=await fetch('/upload?t='+encodeURIComponent(t)+'&n='+encodeURIComponent(x.name),{method:'PUT',body:x});let j=await r.json();if(!r.ok)throw Error(j.error||'Ошибка');e.className='ok';e.textContent='Готово: '+j.name}catch(z){e.className='bad';e.textContent='Не отправлено: '+x.name+' — '+z.message}}b.disabled=false}</script>";

static void on_sig(int s) { (void)s; quit = 1; }
static void sendall(int s, const char *p, size_t n) { while(n) { int w=send(s,p,n,0); if(w<=0)return; p+=w;n-=w; } }
static void reply(int s,int code,const char *type,const char *body) { char h[256]; int n=snprintf(h,sizeof(h),"HTTP/1.1 %d %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",code,code==200?"OK":code==401?"Unauthorized":"Bad Request",type,(unsigned)strlen(body)); sendall(s,h,n);sendall(s,body,strlen(body)); }
static int get_ip(char *out,size_t z) { int s=socket(AF_INET,SOCK_DGRAM,0),i; const char *ns[]={"wlan0","eth0"}; if(s<0)return 0; for(i=0;i<2;i++){struct ifreq f;memset(&f,0,sizeof(f));strncpy(f.ifr_name,ns[i],IFNAMSIZ-1);if(ioctl(s,SIOCGIFADDR,&f)==0){snprintf(out,z,"%s",inet_ntoa(((struct sockaddr_in*)&f.ifr_addr)->sin_addr));close(s);return 1;}}close(s);return 0; }
static void make_token(void) { int fd=open("/dev/urandom",O_RDONLY),i; unsigned char b[16]; if(fd>=0&&read(fd,b,sizeof(b))==sizeof(b))close(fd);else{if(fd>=0)close(fd);srand((unsigned)(time(0)^getpid()));for(i=0;i<16;i++)b[i]=(unsigned char)rand();}for(i=0;i<16;i++)sprintf(token+i*2,"%02x",b[i]); }
/* A new secret is made on every app run, so an old QR link stops working. */
static void load_token(void) { FILE *f; make_token();f=fopen(CFG,"w");if(f){fprintf(f,"token=%s\n",token);fclose(f);} }
static int allowed(const char *n) { const char *e=strrchr(n,'.'); const char *ok[]={".epub",".fb2",".zip",".pdf",".djvu",".djv",".doc",".docx",".rtf",".txt",".htm",".html",".chm",".cbr",".cbz",".jpg",".jpeg",".png",".bmp",".tif",".tiff",0};int i;if(!e)return 0;for(i=0;ok[i];i++)if(!strcasecmp(e,ok[i]))return 1;return 0; }
static void safe_name(char *d,size_t z,const char *s) { size_t i,j=0;for(i=0;s[i]&&j+1<z;i++){unsigned char c=s[i];if(c=='/'||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')d[j++]='_';else if(c>=32)d[j++]=c;}d[j]=0;while(d[0]=='.')memmove(d,d+1,strlen(d));if(!d[0])snprintf(d,z,"file"); }
static const char *param(const char *q,const char *key,char *out,size_t z) { size_t k=strlen(key);const char *p=q;while(p&&*p){if(!strncmp(p,key,k)&&p[k]=='='){p+=k+1;size_t j=0;while(*p&&*p!='&'&&j+1<z){if(*p=='%'&&isxdigit(p[1])&&isxdigit(p[2])){char x[3]={p[1],p[2],0};out[j++]=(char)strtol(x,0,16);p+=3;}else if(*p=='+'){out[j++]=' ';p++;}else out[j++]=*p++;}out[j]=0;return out;}p=strchr(p,'&');if(p)p++;}return 0; }
static int header_len(const char *p,int n) { int i;for(i=3;i<n;i++)if(p[i-3]=='\r'&&p[i-2]=='\n'&&p[i-1]=='\r'&&p[i]=='\n')return i+1;return 0; }
static long long length_of(const char *h) { const char *p=strchr(h,'\n');while(p&&*++p){if(*p=='\r'||*p=='\n')break;if(!strncasecmp(p,"Content-Length:",15))return atoll(p+15);p=strchr(p,'\n');}return -1; }
static void upload(int s,const char *name,long long len,const char *first,int have) { char clean[180],final[300],tmp[320],body[300],notice[300];int fd;FILE *nf;long long left=len;struct statvfs v;
 safe_name(clean,sizeof(clean),name);if(!allowed(clean)){reply(s,400,"application/json","{\"error\":\"Неподдерживаемый формат\"}");return;}if(len<0||len>MAX_FILE){reply(s,400,"application/json","{\"error\":\"Файл слишком большой (лимит 500 MB)\"}");return;}if(check_space(DEST,&v)||((unsigned long long)len+4*1024*1024>(unsigned long long)v.f_bavail*v.f_bsize)){reply(s,400,"application/json","{\"error\":\"Недостаточно места\"}");return;}snprintf(final,sizeof(final),DEST"/%s",clean);for(int i=2;access(final,F_OK)==0&&i<1000;i++)snprintf(final,sizeof(final),DEST"/%.*s (%d)%s",(int)(strrchr(clean,'.')-clean),clean,i,strrchr(clean,'.'));snprintf(tmp,sizeof(tmp),"%s.part",final);fd=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0666);if(fd<0){reply(s,400,"application/json","{\"error\":\"Не удалось создать файл\"}");return;}if(have>left)have=(int)left;if(have&&write(fd,first,have)!=have){close(fd);unlink(tmp);reply(s,400,"application/json","{\"error\":\"Ошибка записи\"}");return;}left-=have;while(left>0){char b[4096];int want=left>sizeof(b)?sizeof(b):(int)left,r=recv(s,b,want,0);if(r<=0||write(fd,b,r)!=r){close(fd);unlink(tmp);return;}left-=r;}fsync(fd);close(fd);if(rename(tmp,final)){unlink(tmp);reply(s,400,"application/json","{\"error\":\"Не удалось завершить запись\"}");return;}snprintf(notice,sizeof(notice),BASE"/last_received.tmp");nf=fopen(notice,"w");if(nf){fprintf(nf,"%s\n",clean);fclose(nf);rename(notice,BASE"/last_received");}snprintf(body,sizeof(body),"{\"name\":\"%s\"}",clean);reply(s,200,"application/json",body); }
static void client(int s) { char h[4096],t[80],n[260];int got=0,hl;while(got<(int)sizeof(h)-1&&(hl=header_len(h,got))==0){int r=recv(s,h+got,sizeof(h)-1-got,0);if(r<=0)return;got+=r;}h[got]=0;if(!hl){reply(s,400,"application/json","{\"error\":\"Некорректный запрос\"}");return;}char method[8],path[600];if(sscanf(h,"%7s %599s",method,path)!=2){return;}char *q=strchr(path,'?');if(q)*q++=0;else q="";if(!param(q,"t",t,sizeof(t))||strcmp(t,token)){reply(s,401,"application/json","{\"error\":\"Недействительная ссылка\"}");return;}if(!strcmp(method,"GET")&&!strcmp(path,"/")){reply(s,200,"text/html",page);return;}if(!strcmp(method,"PUT")&&!strcmp(path,"/upload")&&param(q,"n",n,sizeof(n))){fprintf(stderr,"upload request: %lld bytes\n",length_of(h));fflush(stderr);upload(s,n,length_of(h),h+hl,got-hl);return;}reply(s,400,"application/json","{\"error\":\"Неизвестный запрос\"}"); }
int main(void) { int s,one=1;struct sockaddr_in a;FILE *f;struct sigaction sa;time_t last_ban=0;mkdir(BASE,0777);mkdir(DEST,0777);load_token();InitInkview(0);memset(&sa,0,sizeof(sa));sa.sa_handler=on_sig;sigemptyset(&sa.sa_mask);sigaction(SIGTERM,&sa,0);sigaction(SIGINT,&sa,0);signal(SIGPIPE,SIG_IGN);f=fopen(PID,"w");if(f){fprintf(f,"%d\n",(int)getpid());fclose(f);}WiFiPower(1);NetConnect(NULL);s=socket(AF_INET,SOCK_STREAM,0);if(s<0)return 1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(PORT);if(bind(s,(struct sockaddr*)&a,sizeof(a))||listen(s,3))return 2;while(!quit){fd_set r;struct timeval tv={2,0};FD_ZERO(&r);FD_SET(s,&r);if(select(s+1,&r,0,0,&tv)>0){int c=accept(s,0,0);if(c>=0){struct timeval timeout={30,0};setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));client(c);close(c);}}if(!quit&&time(NULL)-last_ban>=10){BanSleep(15);last_ban=time(NULL);}}fprintf(stderr,"server stopped normally\n");fflush(stderr);close(s);unlink(PID);return 0; }
