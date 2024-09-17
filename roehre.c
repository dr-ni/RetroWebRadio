/*
 * Raspberry pi retro radio
 * (C) 2023 Amrhein & Niethammer
 * GNU GENERAL PUBLIC LICENSE Version 3
 *
 * most significant changes:
 * UN 2016 removed parser with memory leaks and included xml parser
 * UN 2016 potentiometer deamon for raspberry pi 2/3 B+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// sudo apt-get install libxml2-dev
// sudo apt-get install libsdl-ttf2.0-dev
// sudo apt-get install mpd
// sudo apt-get install mpc
// add to root's .bashrc
//    export XAUTHORITY=~pi/.Xauthority
//    export DISPLAY=:0.0

#include "SDL.h"
#include "SDL_ttf.h"
#include "xmlparse.h"
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#define WIN_WIDTH 644 // width of screen
  // 644/14=46 grid
#define WIN_HEIGHT 428 // hight of screen
#define VISIBLE_WIDTH 500// width of visible window on screen
#define VISIBLE_HEIGHT 428 // hight of visible window on screen
#define OFFSET_X 80 // horizontal offset of visible window
#define HIGHLIGHT_XWINDOW 10 // horizontal movable selection window
#define OFFSET_Y 0 // vertical offset of visible window
#define SCREEN_DEPTH 24

const char FONTFILE[]= {"/home/uwe/radio/VeraMono.ttf"};
const char STATIONS[]= {"/home/uwe/radio/stations.xml"};
FILE *fp;
FILE *dfp;
SDL_Surface *screen;
int xpos=0;
int tty;
TTF_Font *station_font;
TTF_Font *station_font_big;
TTF_Font *track_font;
char current_station_url[500];
int current_station_tune_time=0;
char current_playing_url[500];
int loopcounter=0;
char current_track[300];
int refreshNow=0;
int current_page=1;
int pages=2;
int volume=3;
int search_l=0;
int search_r=0;
int textflow=0;
int highlight_global_autotune_index=HIGHLIGHT_XWINDOW/2; //half start-value for auto-scann
char toast[500]; // a "toast" is a short message that appears on the bottom of the screen, like when the volume changes
int toast_timeout=0;

// get current time in milliseconds
double time_in_ms(){
  struct timeval  tv;
  int time_in_mill;
  gettimeofday(&tv, NULL);
   time_in_mill = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ; // convert tv_sec & tv_usec to millisecond
  return time_in_mill;
}

/*
 * process SDL events: TERM signal and keyboard events.
 * the keyboard events are usually not used, but are nice when you want to
 * test the software without a micro controller. You can switch cannels
 * with the cursor keys.
 */
int process_events (){
  SDL_Event event;
  char *key;
  int quit = 0;
  while(SDL_PollEvent(&event)){
    switch(event.type){		    //check the event type
      case SDL_KEYDOWN:			//if a key has been pressed
	     key = SDL_GetKeyName(event.key.keysym.sym);
    	 fprintf(stderr, "The %s key was pressed! %i\n", key,event.key.keysym.sym);
	     if(event.key.keysym.sym == SDLK_ESCAPE)	//quit if 'ESC' pressed
	        quit = 1;
    	 else if(key[0] == 'q')	//quit if 'q'  pressed
	        quit = 1;			//same as "if(event.key.keysym.sym == SDLK_q)"
         else if(event.key.keysym.sym==275){ // right
            if(xpos<=(WIN_WIDTH-OFFSET_X-5)){
            /*
            dfp=fopen("/tmp/debug", "w");
            fprintf(dfp, "x=%d ", xpos);
            fclose(dfp);
            */
            xpos+=5;
          } else {
             xpos=-OFFSET_X;
             current_page++;
             if(current_page>pages)
               current_page=1;
          }
        }
        else if(event.key.keysym.sym == SDLK_r){
           search_r=1;
           search_l=0;
        }
        else if(event.key.keysym.sym == SDLK_l){
           search_l=1;
           search_r=0;
        }
        else if(event.key.keysym.sym == SDLK_v){
           system ("/usr/bin/mpc volume +3 &");
           sprintf(toast,"Volume +3");
           toast_timeout=time_in_ms()+300;
           refreshNow=1;
        }
        else if(event.key.keysym.sym == SDLK_MINUS){
           system ("/usr/bin/mpc volume -3 &");
           sprintf(toast,"Volume -3");
           toast_timeout=time_in_ms()+300;
           refreshNow=1;
        }
        else if(event.key.keysym.sym==276){ // left
          if(xpos+OFFSET_X>=5){
             xpos-=5;
             /*
             dfp=fopen("/tmp/debug", "w");
             fprintf(dfp, "x=%d ", xpos);
             fclose(dfp);
             */
          } else {
             xpos=WIN_WIDTH-OFFSET_X;
             current_page--;
             if(current_page<1){
               current_page=pages;
             }
          }
        }
        else if(event.key.keysym.sym==273){ // up
          current_page++;
          if(current_page>pages)
            current_page=1;
        } else if(event.key.keysym.sym==274){ // down
          current_page--;
          if(current_page<1)
            current_page=pages;
        }
    	break;
       case SDL_MOUSEMOTION:             //mouse moved
//        fprintf(stderr, "Mouse motion x:%d, y:%d\n", event.motion.x, event.motion.y);
;;
        break;
      case SDL_MOUSEBUTTONUP:           //mouse button pressed
;;
//        fprintf(stderr, "Mouse pressed x:%d, y:%d\n", event.button.x, event.button.y);
        xpos=event.button.x-OFFSET_X;
        break; 
     case SDL_QUIT:			//'x' of Window clicked
	//  exit (1);
        quit=1;
    	break;
    }
    refreshNow=1;
  } //while
  if(quit){
    SDL_Quit();
    exit(1);
  }
  return 1;
}

void DrawPixel(int x, int y,Uint8 R, Uint8 G,Uint8 B){
    Uint32 color = SDL_MapRGB(screen->format, R, G, B);
    if(SDL_MUSTLOCK(screen)){
        if(SDL_LockSurface(screen) < 0){
            return;
        }
    }
    switch (screen->format->BytesPerPixel){
        case 1: { /* vermutlich 8 Bit */
            Uint8 *bufp;
            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            *bufp = color;
        }
        break;
        case 2: { /* vermutlich 15 Bit oder 16 Bit */
            Uint16 *bufp;

            bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
            *bufp = color;
        }
        break;
        case 3: { /* langsamer 24-Bit-Modus, selten verwendet */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x * 3;
            if(SDL_BYTEORDER == SDL_LIL_ENDIAN){
                bufp[0] = color;
                bufp[1] = color >> 8;
                bufp[2] = color >> 16;
            } else {
                bufp[2] = color;
                bufp[1] = color >> 8;
                bufp[0] = color >> 16;
            }
        }
        break;
        case 4: { /* vermutlich 32 Bit */
            Uint32 *bufp;

            bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
            *bufp = color;
        }
        break;
    }
    if(SDL_MUSTLOCK(screen)){
        SDL_UnlockSurface(screen);
    }
}



/* draw the green lines on the display */
void draw_grid(){
  int x=0;
  int y=0;
  // 644/14=46 grid 
  for(x=0; x<(WIN_WIDTH); x+=46){
    for(y=0; y<WIN_HEIGHT; y++){
       DrawPixel(x, y, 0x00, 0x44, 0x00);
    }
  }
}

/*
 * read stations from STATIONS file and draw them on the screen. also track
 * if the red bar is currently over a station
 */
void draw_stations(){
  char temps_1[500];
  char temps_2[500];
  char name[500];
  int i=0;
  SDL_Color station_color={ 149, 207, 50 };
  SDL_Color station_color_highlight={ 255, 255, 155 };
  SDL_Surface *text_surface;
  SDL_Rect rect;
  int highlight=0;
  int highlighted_something=0;
  int w=0;
  int h=0;
  int count=0;
  int ret=0;

  memset(temps_1, 0, sizeof(temps_1));
  memset(temps_2, 0, sizeof(temps_2));
  memset(name, 0, sizeof(name));

  if(search_r==1){
     if(!highlight){
        if(xpos<=(WIN_WIDTH-OFFSET_X-1)){
           xpos+=1;
        } else {
          xpos=-OFFSET_X;
          current_page++;
          if(current_page>pages)
             current_page=1;
        }
     }
  }
  if(search_l==1){
     if(!highlight){
        if(xpos>=1){
           xpos-=1;
        } else {
           xpos=WIN_WIDTH-OFFSET_X;
          current_page--;
          if(current_page<1)
             current_page=pages;
        }
     }
  }
  // number of pages
  memset(name,0,500);
  ret=parse1(STATIONS, "pages", name);
  if(ret!=1) exit(-1);
  pages=atoi(name);

  // stations on this page
  sprintf(temps_1,"p%d",current_page);
  memset(name,0,500);
  ret=parse2(STATIONS, temps_1, "count", name);
  if(ret!=1) exit(-1);
  count=atoi(name);
//  int lines=count/2+1; Verteilung der Stationen auf Schrägreihen
  int lines=count/2+0; //UN

  for(i=1; i<=count; i++){
    memset(temps_1,0,500);
    memset(temps_2,0,500);
    memset(name,0,500);
    sprintf(temps_1,"p%d", current_page);
    sprintf(temps_2,"s%d", i);
    ret=parse3(STATIONS, temps_1, temps_2, "name", name);
    if(ret!=1) exit(-1);
    TTF_SizeText(station_font, name, &w, &h);
    rect.x=OFFSET_X+(i-1)*((VISIBLE_WIDTH-60)/count)+30;
    rect.y=OFFSET_Y+((i-1)%lines)*((VISIBLE_HEIGHT-120)/(lines>=2 ? lines-1 : 1))+35;
    highlight=rect.x>xpos-HIGHLIGHT_XWINDOW+OFFSET_X && rect.x<xpos+HIGHLIGHT_XWINDOW+OFFSET_X ? 1 : 0;
    if(highlight){
      highlighted_something=1;
      TTF_SizeText(station_font_big, name, &w, &h);
      rect.y-=1;
    }
    text_surface=TTF_RenderText_Solid(highlight ? station_font_big : station_font, name, highlight ? station_color_highlight : station_color);
    if(text_surface != NULL){
      rect.x-=w/2;
      SDL_BlitSurface(text_surface, NULL, screen, &rect);
      SDL_FreeSurface(text_surface);
    }
    memset(name,0,500);
    // set current station
    if(highlight){
       if(search_r){
          highlight_global_autotune_index+=1;
          if(highlight_global_autotune_index>=2*HIGHLIGHT_XWINDOW-1){
             search_r=0;
             highlight_global_autotune_index=0;
          }
       }
       if(search_l){
          highlight_global_autotune_index+=1;
          if(highlight_global_autotune_index>=2*HIGHLIGHT_XWINDOW-1){
             search_l=0;
             highlight_global_autotune_index=0;
          }
       }
        memset(temps_1,0,500);
        memset(temps_2,0,500);
        sprintf(temps_1,"p%d", current_page);
        sprintf(temps_2,"s%d", i);
        ret=parse3(STATIONS, temps_1, temps_2, "url", name);
        if(ret!=1) exit(-1);
        if(strlen(current_station_url)==0 || strcmp(current_station_url,name)!=0){
        fprintf(stderr, "new tuned station: %s\n",name);
        memset(current_station_url,0,500);
        memcpy(current_station_url,name,strlen(name));
        current_station_tune_time=time_in_ms();
      }
    }
  }
  if(!highlighted_something){
    // no station tuned
    if(strlen(current_station_url)!=0)
      memset(current_station_url,0,500);
  }
}

/*
 * draw the tuner (that red bar)
 */
void draw_tuner(){
  int y;
  for(y=0; y<VISIBLE_HEIGHT; y++){
    DrawPixel(xpos+OFFSET_X, y+OFFSET_Y, 0xff, 0x77, 0x00);
    DrawPixel(xpos+OFFSET_X+1, y+OFFSET_Y, 0xff, 0x77, 0x00);
    DrawPixel(xpos+OFFSET_X+2, y+OFFSET_Y, 0xff, 0x77, 0x00);
    DrawPixel(xpos+OFFSET_X+3, y+OFFSET_Y, 0xff, 0x77, 0x00);
  }
}

/*
 * draw the currently playing track (bottom of the screen)

void draw_current_track() {
  char temps[500];
  SDL_Surface *text_surface;
  SDL_Rect rect;
  SDL_Color station_color={ 255,255,255 };
  // show a toast instead of current track?
  if(toast_timeout>time_in_ms()) {
    strcpy(temps,toast);
  } else {
    // show current track
    if(current_track==NULL)
      return;
    if(current_station_url==NULL)
      return;
    if(strcmp(current_playing_url,"")==0) {
      // playing of current station hasn't started yet
      strcpy(temps,"");
    } else {
      strcpy(temps,current_track);
    }
  }
  text_surface=TTF_RenderUTF8_Solid(track_font, temps, station_color);
  if (text_surface != NULL) {
    rect.x=10+OFFSET_X;
    rect.y=VISIBLE_HEIGHT-30+OFFSET_Y;
    SDL_BlitSurface(text_surface, NULL, screen, &rect);
    SDL_FreeSurface(text_surface);
      fprintf(stderr, ".");
  }
}
 */


/*
 * draw the currently playing track (bottom of the screen)
 */
void draw_current_track() {
  char temps[500];
  char temps2[500];
  char temps3[]={"          "};
  SDL_Surface *text_surface;
  SDL_Rect rect;
  SDL_Color station_color={ 255,255,255 };
  // show a toast instead of current track?
  if(toast_timeout>time_in_ms()) {
    strcpy(temps,toast);
  } else {
    // show current track
    if(strlen(current_track)==0)
      return;
    if(strlen(current_station_url)==0)
      return;
    if(strcmp(current_playing_url,"")==0) {
      // playing of current station hasn't started yet
      strcpy(temps,"");
    } else {
      strcpy(temps,current_track);
    }
  }
  if(strlen(temps)>=70){
     memset(temps2,0,500);
     memcpy(temps2,&temps[textflow],strlen(temps)-textflow);
     memcpy(&temps2[strlen(temps)-textflow],temps3,10);
     memcpy(&temps2[strlen(temps)-textflow+10],temps,textflow);
      //printf("%s\n",temps2);
     if(textflow>=strlen(temps)) textflow=0;
     textflow++;
usleep(40000);
     text_surface=TTF_RenderUTF8_Solid(track_font, temps2, station_color);
     if (text_surface != NULL) {
        rect.x=0;
        rect.y=VISIBLE_HEIGHT-20+OFFSET_Y;
        SDL_BlitSurface(text_surface, NULL, screen, &rect);
        SDL_FreeSurface(text_surface);
     }
  }
  else{
     textflow=0;
     //printf("%s\n",temps2);
     text_surface=TTF_RenderUTF8_Solid(track_font, temps, station_color);
     if (text_surface != NULL) {
        rect.x=10;
        rect.y=VISIBLE_HEIGHT-20+OFFSET_Y;
        SDL_BlitSurface(text_surface, NULL, screen, &rect);
        SDL_FreeSurface(text_surface);
     }
  }
}

/*
 * find out which track is currenty playing. If user has recently changed
 * volume or changed band: show information about this event instead
 */
void get_current_track(){
  FILE *fp;
  char out[1000];
  char new_track[1000];

  new_track[0]='\0';
  if(strcmp(current_playing_url,"")==0){
    // no station playing
    current_track[0]='\0';
    return;
  }
  if(loopcounter%45!=15)
    return;
  // get new track info
  fp = popen("/usr/bin/mpc", "r");
  if(fp == NULL){
    strcpy(new_track,"Error receiving station info");
  } else {
    fgets(out, sizeof(out)-1, fp);
   // fprintf(stderr, "current track: %s\n",out);
    if(strncmp(out,"volume: ",8)==0){
      strcpy(current_track,"");
    } else if(strncmp(out,"http",4)==0 || strncmp(out,"mms://",6)==0){
      strcpy(new_track,"");
    } else {
      strcpy(new_track,out);
      if(strlen(out)>2)
        new_track[strlen(out)-1]='\0';
    }
    // read a second line (there might be an error message)
    if(fgets(out, sizeof(out)-1, fp)!=NULL){
      if(strncmp(out,"ERROR",5)==0)
        strcpy(new_track,out);
    }
    pclose(fp);
  }
  if(strcmp(current_track,new_track)!=0){
    refreshNow=1;
    strcpy(current_track,new_track);
  }
}

/*
 * play currently selected track
 */
void play_current(){
  char temps[522];
  memset(temps, 0, sizeof(temps));
  int ret=0;
  
  if((strcmp(current_playing_url,"")!=0 && strlen(current_station_url)==0)
      || (current_station_url>0 && strcmp(current_station_url, current_playing_url)!=0)){
    // station has changed
    if(time_in_ms()-current_station_tune_time<1000 && strlen(current_station_url)>0){
      // station is not tuned in long enough. wait a bit more.
      return;
    }
    if(strlen(current_station_url)==0){
      memset(current_playing_url, 0, sizeof(current_playing_url));
    }
    else{
      strcpy(current_playing_url, current_station_url);
    }
    fprintf(stderr, "\ntuning station: %s\n", current_playing_url);
    ret=fork();
    if(ret==0){
      system("mpc clear &");
      sprintf(temps,"mpc add '%s' &", current_playing_url);
      fprintf(stderr, "### command: %s\n",temps);
      system(temps);
      system("mpc play");
      exit(0);
    }
    else{
      fprintf(stderr, "=> Fork with ret=%d, retrying...\n",ret);
//      sprintf(temps,"kill '%d'\n", ret);
//      system(temps);
      ret=fork();
      if(ret==0){
        sleep(1);
//        system("mpc clear &");
        sprintf(temps,"mpc add '%s' &", current_playing_url);
        fprintf(stderr, "### command: %s\n",temps);
        system(temps);
        system("mpc play");
        exit(0);
      }
      else{
        fprintf(stderr, "=> Fork id=%d\n",ret);
      }
    }
    toast_timeout=time_in_ms()+500;
    refreshNow=1;
  }
}

/*
 * draw everything on the screen
 */
void draw_everything(){
 // fprintf(stderr, "draw_everything\n");
  SDL_FillRect(screen,NULL, 0x000000);
  draw_grid();
  draw_stations();
  draw_tuner();
  draw_current_track();
  SDL_Flip(screen);

}

/*
 * main loop
 */
int loop(){
 //// system("unclutter -display :0 -noevents -grab &");
  while(1){
    loopcounter++;
    if(loopcounter%10==1 || tty==0)
      process_events();
   // receive_tty();
    get_current_track();
    if(loopcounter%40==0 || refreshNow==1){
      draw_everything();
    }
    play_current();
    refreshNow=0;
  }
}

int main(int argc, char *argv[]){
     memset(toast, 0, sizeof(toast));
     sleep(2); // avoids boot radio behind taskbar
     fprintf(stderr, "initializing...\n");
     signal(SIGCHLD, SIG_IGN);

     /* Initialize SDL */
     SDL_Init(SDL_INIT_VIDEO);

     /* Initialize the screen / window */
//     screen = SDL_SetVideoMode(WIN_WIDTH,WIN_HEIGHT, SCREEN_DEPTH, SDL_FULLSCREEN);
     SDL_ShowCursor(1); // Mauszeiger an
     screen = SDL_SetVideoMode(WIN_WIDTH, WIN_HEIGHT, SCREEN_DEPTH, SDL_SWSURFACE);
     if(screen==NULL){
       fprintf(stderr, "SDL Error: %s\n",SDL_GetError());
     }

     // load font for text
     TTF_Init();
     station_font=TTF_OpenFont(FONTFILE, 22);
     station_font_big=TTF_OpenFont(FONTFILE, 22);
     if(station_font==0){
       fprintf(stderr, "ERROR: font defekt\n"); exit(1);
     }
     track_font=TTF_OpenFont(FONTFILE, 15);
     fprintf(stderr, "started...\n");
     fprintf(stderr, "starting to receive stuff and so on...\n");
     loop();
}
