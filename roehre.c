#include "SDL.h"
#include <stdio.h>
#include "iniparser/iniparser.h"
#include "SDL_ttf.h" 
#include <fcntl.h>
#include <math.h>
#include <time.h>          
#include <signal.h>
       

#define SCREEN_WIDTH 650 // width of visible window on screen
#define SCREEN_HEIGHT 410 // hight of visible window on screen
#define OFFSET_X 190 // horizontal offset of visible window
#define OFFSET_Y 310 // vertical offset of visible window
#define SCREEN_DEPTH 24

// License: GPL v3, see LICENSE file

SDL_Surface *screen;
int xpos=4;
int tty;
TTF_Font *station_font;
TTF_Font *station_font_big;
TTF_Font *track_font;
char *current_station_url;
int current_station_tune_time=0;
char current_playing_url[500];
int loopcounter=0;
char current_track[300];
int refreshNow=0;
int current_band=1;
int band_count=2;
int volume=3;

char toast[100]; // a "toast" is a short message that appears on the bottom of the screen, like when the volume changes
int toast_timeout=0;

// get current time in milliseconds
double time_in_ms() {
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
int process_events () {
  SDL_Event event;
  int status;
  char *key;
  int quit = 0;
  while(SDL_PollEvent(&event)) {
    switch (event.type) {		//check the event type
      case SDL_KEYDOWN:			//if a key has been pressed
	key = SDL_GetKeyName(event.key.keysym.sym);
    	printf("The %s key was pressed! %i\n", key,event.key.keysym.sym );
	if ( event.key.keysym.sym == SDLK_ESCAPE )	//quit if 'ESC' pressed
	  quit = 1;
    	else if ( key[0] == 'q'  )	//quit if 'q'  pressed
	  quit = 1;			//same as "if ( event.key.keysym.sym == SDLK_q )"
        else if(key,event.key.keysym.sym==275) // right
          xpos+=10;
        else if(key,event.key.keysym.sym==276) // left
          xpos-=10;
        else if(key,event.key.keysym.sym==273) { // up
          current_band++;
          if(current_band>band_count)
            current_band=1;
        } else if(key,event.key.keysym.sym==274) { // down
          current_band--;
          if(current_band<1)
            current_band=band_count;
        }
    	break;
       case SDL_MOUSEMOTION:             //mouse moved
        printf("Mouse motion x:%d, y:%d\n", event.motion.x, event.motion.y );
        break;
      case SDL_MOUSEBUTTONUP:           //mouse button pressed
        printf("Mouse pressed x:%d, y:%d\n", event.button.x, event.button.y );
        xpos=event.button.x-OFFSET_X;
        break; 
     case SDL_QUIT:			//'x' of Window clicked
	//  exit (1);
        quit=1;
    	break;
    }
    refreshNow=1;
  } //while
  if(quit) {
    SDL_Quit();
    exit(1);
  }
  return 1;
}

/* read data from micro controller */
int receive_tty() {
  char line[80];
  char temps[160]="";
  int x=0;  
  int p=0;
  int c=0;
  int lastc=0;
  int tmp;
  int time=0;
  // for elect
  fd_set fds;
  struct timeval timeout;
  int rc, result;
  
  if(tty==0) {
    // no micro controller connected. wait a bit, then exit
    usleep(100000);
    return 0;
  }

  rc=1000;
  while(rc>0) {
    // wait till there is data
    timeout.tv_sec = 0;
    timeout.tv_usec = rc==1000 ? 100000 : 0;;
    FD_ZERO(&fds);
    FD_SET(tty, &fds);
    printf("start tty select...\n");
    rc = select(tty+1, &fds, NULL, NULL, &timeout);
    printf("end tty select...%i\n",rc);
  
    // start reading
    c=0;
    p=0;
    lastc=0;
    time=time_in_ms();
    while(rc>0 && lastc!='\n') {
      lastc=c;
      //c=getc(tty);
      read(tty,&c,1);
      if(c=='\0') {
        
      } else if(c=='\n') {
        printf("zeilenende gelesen\n");
      } else {
        printf("sonstiges zeichen gelesen: %i %i\n",c,p);
        line[p]=c;
          p++;
      }
      //refreshNow=1;
      if(p>50)
        break;
      if(time_in_ms()-time>100) {
        printf("timeout!\n");
        rc=0;
        break;
      }
    }
    
    line[p]=0;
    if(line[0]=='p' && line[1]=='1') {
      // first potentiometer (tuner)
      x=atoi(&line[2]);
      printf("pot1: %s %d\n",&line[2],x);
      tmp=SCREEN_WIDTH-x*((float)SCREEN_WIDTH/1024);
      if(tmp!=xpos) {
        refreshNow=1;
        xpos=tmp;
      }
    }
    if(line[0]=='p' && line[1]=='2') {
      // second potentiometer (volume)
      tmp=(1024-atoi(&line[2]))/12;
      printf("pot2: %s %d\n",&line[2],x);
      if(tmp!=volume) {
        volume=tmp;
        sprintf(temps,"amixer -c 1 sset PCM %i",volume);
        system(temps);
        sprintf(toast,"Volume %i",volume);
        toast_timeout=time_in_ms()+300;
        refreshNow=1;
      }
    }

    if(line[0]=='e' && line[1]=='1') {
      // first encoder (band)
      tmp=atoi(&line[2]);
      if(tmp==2) {
        current_band++;
        if(current_band>band_count)
          current_band=band_count;
      }
      if(tmp==1) {
        current_band--;
        if(current_band<1)
          current_band=1;
      }
      // stop playing current track
      if(current_station_url!=NULL)
        free(current_station_url);
      current_station_url=NULL;
      current_track[0]='\0';
      // show toast
      sprintf(toast,"%i / %i",current_band,band_count);
      toast_timeout=time_in_ms()+500;
      refreshNow=1;
    }
  }
  if(p>0)
    return 1;
  return 0;
}

/* draw the green lines on the display */
int draw_grid() {
  int x=0;
  int y=0;
  Uint32       *p;
  for(x=0; x<SCREEN_WIDTH; x+=50) {
    for(y=0; y<SCREEN_HEIGHT; y++) {
      p = (Uint8 *)screen->pixels + (y+OFFSET_Y) * screen->pitch + (x+OFFSET_X) * screen->format->BytesPerPixel;
      *p=0x004400;
    }
  }
}

/*
 * read stations from .ini file and draw them on the screen. also track
 * if the red bar is currently over a station
 */
int draw_stations() {
  dictionary *stations;
  char temps[160]="";
  char *temps2=NULL;
  char *temps3=NULL;
  char *name;
  int i=0;
  SDL_Color station_color={ 109,207,50 };
  SDL_Color station_color_highlight={ 255,255,255 };
  SDL_Surface *text_surface;
  SDL_Rect rect;
  int highlight=0;
  int highlighted_something=0;
  int w=0;
  int h=0;
  int count=0;
  
  stations=iniparser_load("stations.ini");
  // number of pages
  name=iniparser_getstring(stations,"global:pages","1");
  band_count=atoi(name);

  // stations on this page
  sprintf(temps,"p%i:count",current_band);
  name=iniparser_getstring(stations,temps,"0");
  count=atoi(name);
  
  int lines=count/2+1;
  
  for(i=1; i<=count; i++) {
    sprintf(temps,"p%is%i:name",current_band,i);
    name=iniparser_getstring(stations,temps,"");
    TTF_SizeText(station_font, name, &w, &h);
    rect.x=OFFSET_X+(i-1)*((SCREEN_WIDTH-60)/count)+30;
    rect.y=OFFSET_Y+((i-1)%lines)*((SCREEN_HEIGHT-120)/(lines>=2 ? lines-1 : 1))+35;
    highlight=rect.x>xpos-20+OFFSET_X && rect.x<xpos+20+OFFSET_X ? 1 : 0;
    if(highlight) {
      highlighted_something=1;
      TTF_SizeText(station_font_big, name, &w, &h);
      rect.y-=1;
    }
    text_surface=TTF_RenderText_Solid(highlight ? station_font_big : station_font, name, highlight ? station_color_highlight : station_color);
    if (text_surface != NULL) {
      rect.x-=w/2;
      SDL_BlitSurface(text_surface, NULL, screen, &rect);
      SDL_FreeSurface(text_surface);
    }
    free(name);
    // set current station
    if(highlight) {
      // try to find link to "PLS" playlist
      sprintf(temps,"p%is%i:pls",current_band,i);
      temps3=iniparser_getstring(stations,temps,NULL);
      if(temps3!=NULL) {
        // PLS found
        temps2=malloc(1000);
        sprintf(temps2,"pls:%s",temps3);
        free(temps3);
        printf("PLS found: %s",temps2);
      }
 
      // try to find direct link to file
      if(temps2==NULL) {
        sprintf(temps,"p%is%i:url",current_band,i);
        temps2=iniparser_getstring(stations,temps,"");
      }
      if(current_station_url==NULL || strcmp(current_station_url,temps2)!=0) {
        printf("new tuned station: %s\n",temps2);
        if(current_station_url!=NULL)
          free(current_station_url);
        current_station_url=temps2;
        current_station_tune_time=time_in_ms();
      }
    }
  }
  if(!highlighted_something) {
    // no station tuned
    if(current_station_url!=NULL)
      free(current_station_url);
    current_station_url=NULL;  
  }
}

/*
 * draw the tuner (that red bar)
 */
int draw_tuner() {
  int y;
  Uint32       *p;
  for(y=0; y<SCREEN_HEIGHT; y++) {
    p = (Uint8 *)screen->pixels + (y+OFFSET_Y) * screen->pitch + (xpos+OFFSET_X) * screen->format->BytesPerPixel;
    *p=0xff0000;
    p = (Uint8 *)screen->pixels + (y+OFFSET_Y) * screen->pitch + (xpos+1+OFFSET_X) * screen->format->BytesPerPixel;
    *p=0xff0000;
  }
}

/*
 * draw the currently playing track (bottom of the screen)
 */
int draw_current_track() {
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
    rect.y=SCREEN_HEIGHT-30+OFFSET_Y;
    SDL_BlitSurface(text_surface, NULL, screen, &rect);
    SDL_FreeSurface(text_surface);
  }
}

/*
 * find out which track is currenty playing. If user has recently changed
 * volume or changed band: show information about this event instead
 */
int get_current_track() {
  FILE *fp;
  char out[1000];
  char new_track[1000];

  new_track[0]='\0';
  if(strcmp(current_playing_url,"")==0) {
    // no station playing
    current_track[0]='\0';
    return;
  }
  if(loopcounter%45!=15)
    return;
  // get new track info
  fp = popen("/usr/bin/mpc", "r");
  if (fp == NULL) {
    strcpy(new_track,"Error receiving station info");
  } else {
    fgets(out, sizeof(out)-1, fp);
    printf("current track: %s\n",out);
    if(strncmp(out,"volume: ",8)==0) {
      strcpy(current_track,"");
    } else if(strncmp(out,"http",4)==0 || strncmp(out,"mms://",6)==0) {
      strcpy(new_track,"");
    } else {
      strcpy(new_track,out);
      if(strlen(out)>2)
        new_track[strlen(out)-1]='\0';
    }
    // read a second line (there might be an error message)
    if(fgets(out, sizeof(out)-1, fp)!=NULL) {
      if(strncmp(out,"ERROR",5)==0)
        strcpy(new_track,out);
    }
    pclose(fp);    
  }
  if(strcmp(current_track,new_track)!=0) {
    refreshNow=1;
    strcpy(current_track,new_track);
  }
}

/*
 * play currently selected track
 */
int play_current() {
  int i=0;
  char temps[360]="";
  if(strcmp(current_playing_url,"")!=0 && current_station_url==NULL
      || current_station_url>0 && strcmp(current_station_url, current_playing_url)!=0) {
    // station has changed
    if(time_in_ms()-current_station_tune_time<1000 && current_station_url!=NULL) {
      // station is not tuned in long enough. wait a bit more.
      return;
    }
    if(current_station_url==0)
      strcpy(current_playing_url,"");
    else
      strcpy(current_playing_url,current_station_url);

      
    printf("neue station: %s\n",current_playing_url);
    if(fork()==0) {
      system("mpc clear");
      if(strncmp(current_playing_url,"pls:",4)==0) {
        // pls playlist
        sprintf(temps,"./play_pls.sh '%s'",&current_playing_url[4]);
      } else {
        sprintf(temps,"mpc add '%s'",current_playing_url);
      }
      printf("command: %s\n",temps);
      system(temps);    
      system("mpc play");
      exit(0);
    }
  }
}

/*
 * draw everything on the screen
 */
int draw_everything() {
  printf("draw_everything\n");
  if(xpos<0)
    xpos=0;
  if(xpos>=SCREEN_WIDTH)
    xpos=SCREEN_WIDTH-1;
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
int loop() {
  int did_something=0;
  while(1) {
    loopcounter++;
    if(loopcounter%10==1 || tty==0)
      process_events();
    receive_tty();
    get_current_track();
    if(loopcounter%40==0 || refreshNow==1) {
      draw_everything();
    }
    play_current();
    refreshNow=0;
  }
}

int main(int argc, char *argv[]) {
     int         x = 10; //x coordinate of our pixel
     int         y = 20; //y coordinate of our pixel
     int flags=0;
     
     printf("initializing...\n");
     signal(SIGCHLD, SIG_IGN);
     
     
     /* Initialize SDL */
     SDL_Init(SDL_INIT_VIDEO);
     
     /* Initialize the screen / window */
     screen = SDL_SetVideoMode(1024,768, SCREEN_DEPTH, SDL_FULLSCREEN);
     //screen = SDL_SetVideoMode(1024,768, SCREEN_DEPTH, SDL_SWSURFACE /* SDL_FULLSCREEN*/);
     if(screen==NULL) {
       printf("SDL Error: %s\n",SDL_GetError());
     }

     // load font for text
     TTF_Init();
     station_font=TTF_OpenFont("VeraMono.ttf", 15);
     station_font_big=TTF_OpenFont("VeraMono.ttf", 16);
     if(station_font==0) {
       printf("ERROR: font defekt\n"); exit(1);
     }
     track_font=TTF_OpenFont("VeraMono.ttf", 12);
       
     // open connection to micro controller
     tty=open("/dev/ttyACM0",O_RDWR | O_NONBLOCK);
     if(tty==0)
       tty=open("/dev/ttyACM1", O_RDWR | O_NONBLOCK);
     
     printf("started...\n");
     printf("starting to receive stuff and so on...\n");
     loop();
}
