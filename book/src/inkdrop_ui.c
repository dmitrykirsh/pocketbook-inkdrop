/* InkDrop front screen for PocketBook InkView. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "inkview.h"
#include "qr.h"
#define CFG "/mnt/ext1/system/inkdrop/config"
#define PID "/var/tmp/inkdrop.pid"
#define PORT 8790
typedef struct {int x,y,w,h;} rect;
static ifont *title,*body,*small,*button;static rect refresh,closeb;static int W,H;static char token[33],last_received[180];
static int ip(char *out,size_t z){int s=socket(AF_INET,SOCK_DGRAM,0),i;const char *ns[]={"wlan0","eth0"};if(s<0)return 0;for(i=0;i<2;i++){struct ifreq f;memset(&f,0,sizeof(f));strncpy(f.ifr_name,ns[i],IFNAMSIZ-1);if(ioctl(s,SIOCGIFADDR,&f)==0){snprintf(out,z,"%s",inet_ntoa(((struct sockaddr_in*)&f.ifr_addr)->sin_addr));close(s);return 1;}}close(s);return 0;}
static void load(){FILE*f=fopen(CFG,"r");char l[180];token[0]=0;last_received[0]=0;if(f){while(fgets(l,sizeof(l),f))if(!strncmp(l,"token=",6)){memcpy(token,l+6,32);token[32]=0;}fclose(f);}f=fopen("/mnt/ext1/system/inkdrop/last_received","r");if(f){if(fgets(last_received,sizeof(last_received),f)){last_received[strcspn(last_received,"\r\n")]=0;}fclose(f);}}
static void btn(rect r,const char*s){FillArea(r.x,r.y,r.w,r.h,BLACK);SetFont(button,WHITE);DrawTextRect(r.x,r.y,r.w,r.h,s,ALIGN_CENTER|VALIGN_MIDDLE);}
static void draw(){char a[48],url[100],state[210];uint8_t m[QR_MAX_SIZE*QR_MAX_SIZE];int x,y,ms,side,size,u,margin,buttons_y,formats_y,qr_limit;W=ScreenWidth();H=ScreenHeight();u=W/28;margin=W/18;buttons_y=H-margin-u*2;formats_y=buttons_y-u*3;ClearScreen();SetFont(title,BLACK);DrawTextRect(margin,margin,W-2*margin,u*2,"InkDrop",ALIGN_CENTER|VALIGN_MIDDLE);SetFont(small,DGRAY);DrawTextRect(margin,margin+u*2,W-2*margin,u*2,"Книги и изображения по Wi-Fi",ALIGN_CENTER|VALIGN_MIDDLE);FillArea(margin,margin+u*4,W-2*margin,2,BLACK);y=margin+u*5;SetFont(small,BLACK);snprintf(state,sizeof(state),!ip(a,sizeof(a))?"Ожидание сети":last_received[0]?"Получено: %s":"Готов к приёму файлов",last_received);DrawTextRect(margin,y,W-2*margin,u*2,state,ALIGN_CENTER|VALIGN_MIDDLE);y+=u*2;if(!ip(a,sizeof(a))||!token[0]){SetFont(body,BLACK);DrawTextRect(margin,y,W-2*margin,u*7,"Нет подключения к Wi-Fi.\nПроверьте точку доступа и сеть ридера.",ALIGN_CENTER|VALIGN_MIDDLE);}else{snprintf(url,sizeof(url),"http://%s:%d/?t=%s",a,PORT,token);size=qr_encode(url,m);qr_limit=formats_y-y-u*2-10;ms=(W-2*margin)/(size+8);if(qr_limit/(size+8)<ms)ms=qr_limit/(size+8);if(ms<3)ms=3;side=ms*(size+8);x=(W-side)/2;FillArea(x,y,side,side,WHITE);for(int r=0;r<size;r++)for(int c=0;c<size;c++)if(m[r*size+c])FillArea(x+(c+4)*ms,y+(r+4)*ms,ms,ms,BLACK);y+=side;SetFont(small,BLACK);DrawTextRect(margin,y,W-2*margin,u*2,"Сканируйте QR камерой телефона",ALIGN_CENTER|VALIGN_MIDDLE);}SetFont(small,DGRAY);DrawTextRect(margin,formats_y,W-2*margin,u*2,"EPUB · FB2 · PDF · DJVU · DOCX · CBZ\nJPG · PNG · BMP · TIFF → Downloads",ALIGN_CENTER|VALIGN_MIDDLE);refresh=(rect){margin,buttons_y,W/2-margin-4,u*2};closeb=(rect){W/2+4,buttons_y,W/2-margin-4,u*2};btn(refresh,"Обновить");btn(closeb,"Закрыть");FullUpdate();}
static void received_timer(void){static char previous_ip[48]="";char before[180],current_ip[48]="";snprintf(before,sizeof(before),"%s",last_received);load();ip(current_ip,sizeof(current_ip));if(strcmp(before,last_received)||strcmp(previous_ip,current_ip)){snprintf(previous_ip,sizeof(previous_ip),"%s",current_ip);draw();}SetWeakTimer("inkdrop_received",received_timer,2500);}
static int hit(rect r,int x,int y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
static void stop_server(void){static int stopped;FILE*f;int n=0;if(stopped)return;stopped=1;f=fopen(PID,"r");if(f){fscanf(f,"%d",&n);fclose(f);}if(n>1){int r=kill(n,SIGTERM);fprintf(stderr,"stop server pid=%d result=%d errno=%d\n",n,r,errno);fflush(stderr);}}
static void stop(void){ClearTimer(received_timer);stop_server();CloseApp();}
static int handler(int t,int p1,int p2){fprintf(stderr,"event %d %d %d\n",t,p1,p2);fflush(stderr);switch(t){case EVT_INIT:SetPanelType(PANEL_DISABLED);/* no firmware panel offset or stale top strip */case EVT_SHOW:case EVT_REPAINT:case EVT_FOREGROUND: W=ScreenWidth();if(!title){title=OpenFont("Roboto-Bold",W/11,1);body=OpenFont("Roboto-Regular",W/24,1);small=OpenFont("Roboto-Regular",W/31,1);button=OpenFont("Roboto-Bold",W/28,1);if(!title)title=(ifont*)GetFont();if(!body)body=(ifont*)GetFont();if(!small)small=(ifont*)GetFont();if(!button)button=(ifont*)GetFont();}load();draw();if(t==EVT_INIT)SetWeakTimer("inkdrop_received",received_timer,2500);break;case EVT_POINTERUP:if(hit(refresh,p1,p2)){load();draw();}else if(hit(closeb,p1,p2))stop();break;case EVT_KEYPRESS:if(p1==IV_KEY_BACK||p1==IV_KEY_HOME)stop();break;case EVT_EXIT:ClearTimer(received_timer);stop_server();break;}return 0;}
int main(){InkViewMain(handler);return 0;}
