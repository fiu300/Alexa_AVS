/*
 * Sensory Confidential
 *
 * Copyright (C)2000-2015 Sensory Inc.
 *
 *---------------------------------------------------------------------------
 * Sensory TrulyHandsfree SDK Example: PhraseSpot.c
 *---------------------------------------------------------------------------
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <trulyhandsfree.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "console.h"
#include "audio.h"
#include "datalocations.h"

#include "asoundlib.h"
#include "teraVad.h"

#include "console.h"
#include "audio.h"
#include "datalocations.h"

#include "mw_asr.h"
#include "phraseSpot.h"
#include "light_msg.h"

#define MODE_LOAD_CUSTOM  (1)  // Load pre-built custom HBG vocabulary from file (uses custom HBG acoustic model)
#define MODE_BUILD_MANUAL (2)  // Build vocabulary with explicit phrasespot parameters (uses gerenic acoustic model)
#define MODE_BUILD_AUTO   (3)  // Build vocabulary with automatic phrasespot parameters (uses generic acoustic model)

#define BUILD_MODE        (MODE_LOAD_CUSTOM)
   
#define GENDER_DETECT (1)  // Enable/disable gender detection

#define HBG_GENDER_MODEL "hbg_genderModel.raw"
#define HBG_NETFILE      "sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_am.raw"//"sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_am.raw"
#define HBG_SEARCHFILE   "sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_9.raw"//"sensory_demo_hbg_en_us_sfs_delivery04_pruned_search_9.raw" // pre-built search
#define HBG_OUTFILE      "myhbg.raw"           /* hbg output search */

#define NBEST              (1)                /* Number of results */
#define PHRASESPOT_PARAMA_OFFSET   (0)        /* Phrasespotting ParamA Offset */
#define PHRASESPOT_BEAM    (100.f)            /* Pruning beam */
#define PHRASESPOT_ABSBEAM (100.f)            /* Pruning absolute beam */
#if GENDER_DETECT
# define PHRASESPOT_DELAY  15                 /* Phrasespotting Delay */
#else
# define PHRASESPOT_DELAY  PHRASESPOT_DELAY_ASAP  /* Phrasespotting Delay */
#endif
#define MAXSTR             (512)              /* Output string size */
#define SHOWAMP            (0)                /* Display amplitude */

#define THROW(a) { ewhere=(a); goto error; }
#define THROW2(a,b) {ewhere=(a); ewhat=(b); goto error; }



static void* g_userInfo = NULL;

static audioData_Callback g_data_cb = NULL;
static audioStatus_Changed g_status_cb = NULL;

static struct timeval startTime,endTime;  
static float Timeuse;  

static int is_recog = 0;
static int is_record = 0;
static int seq;
static int record_wait = 6;

void set_mw_callback(void* userp, audioData_Callback data_cb, audioStatus_Changed status_cb) {
  g_userInfo = userp;
  g_data_cb = data_cb;
  g_status_cb = status_cb;
}


char *formatExpirationDate(time_t expiration)
{
  static char expdate[33];
  if (!expiration) return (char*)("never");
  strftime(expdate, 32, "%m/%d/%Y 00:00:00 GMT", gmtime(&expiration));
  return expdate;
}


// #if defined(__iOS__)
void * Phrasespot(void *args/*void *inst*/)
// #else
// int main(int argc, char **argv)
// #define inst NULL
// #endif
{
  #define inst NULL

  const char *rres;
  thf_t *ses=NULL;
  recog_t *r=NULL;
  searchs_t *s=NULL;
  pronuns_t *pp=NULL;
  char str[MAXSTR];
  const char *ewhere, *ewhat=NULL;
  float score, delay;
  unsigned short status, done=0;
  audiol_t *p;
  unsigned short i=0;
  void *cons=NULL;

  int vadcnt = 0;
  int VadState;
  int rtn;

  struct pcm_config config;
  struct pcm *pcm;
  unsigned int size;
  char *cap_buf;
  char *cap_buf_double;
  int cx20921_num;
  int single_num;


  /* Draw console */
  if (!(cons=initConsole(inst))) return NULL;

  /* Create SDK session */
  if(!(ses=thfSessionCreate())) {
    panic(cons, "thfSessionCreate", thfGetLastError(NULL), 0);
    return NULL;
  }

  /* Display TrulyHandsfree SDK library version number */
  dispv(cons, "TrulyHandsfree SDK Library version: %s\n", thfVersion());
  dispv(cons, "Expiration date: %s\n\n",
  formatExpirationDate(thfGetLicenseExpiration()));

  /* Create recognizer */
  /* NOTE: HBG_NETFILE is a custom model for 'hello blue genie'. Use a generic acoustic model if you change vocabulary or contact Sensory for assistance in developing a custom acoustic model specific to your vocabulary. */
  disp(cons,"Loading recognizer: "  HBG_NETFILE);
  if(!(r=thfRecogCreateFromFile(ses, SAMPLES_DATADIR HBG_NETFILE,(unsigned short)(AUDIO_BUFFERSZ/1000.f*SAMPLERATE),-1,NO_SDET))) 
    THROW("thfRecogCreateFromFile");

  disp(cons,"Loading custom HBG search: " SAMPLES_DATADIR HBG_SEARCHFILE);
  if(!(s=thfSearchCreateFromFile(ses,r,SAMPLES_DATADIR HBG_SEARCHFILE,NBEST)))
    THROW("thfSearchCreateFromFile");

  /* Configure parameters - only DELAY... others are saved in search already */
  disp(cons,"Setting parameters...");  
  delay=PHRASESPOT_DELAY;
  if (!thfPhrasespotConfigSet(ses,r,s,PS_DELAY,delay))
    THROW("thfPhrasespotConfigSet: delay");


  if (thfRecogGetSampleRate(ses,r) != SAMPLERATE)
    THROW("Acoustic model is incompatible with audio samplerate");

  /* Initialize recognizer */
  disp(cons,"Initializing recognizer...");


  if(!thfRecogInit(ses,r,s,RECOG_KEEP_NONE))
    THROW("thfRecogInit");

  /* Initialize audio */
  disp(cons,"Initializing audio...");
  //initAudio(inst,cons);

	
	system("tinymix 13 1");
	system("tinymix 5 1");

	

	config.channels = 2;
	config.rate = 16000;
	config.period_size = 1024;
	config.period_count = 4;
	config.format = (pcm_format)16;
	config.start_threshold = 0;
	config.stop_threshold = 0;
	config.silence_threshold = 0;


	pcm = pcm_open(1, 0, PCM_IN, &config);
	if (!pcm) {
		printf("Unable to open PCM device : (!pcm)\n");
	}
	if (!pcm_is_ready(pcm)) {
		printf("Unable to open PCM device : (!pcm_is_ready(pcm))\n");
	}

	usleep(30*1000);
	
	size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));

	size = 320;
	cap_buf = (char*)malloc(size*2);
	cap_buf_double = (char*)malloc(size*4);
	if (!cap_buf || !cap_buf_double) {
		printf("Unable to allocate %d bytes\n", size);
		free(cap_buf);
		pcm_close(pcm);
	}

  printf("size = %d\n", size);

	printf("tinycap start\n");

	p = NULL;
 	p = (audiol_t*)memset(malloc(sizeof(audiol_t)), 0, sizeof(audiol_t));
  p->len = size;
  p->audio = NULL;
  p->flags = 0;
  
  // FILE *fp = NULL;

  seq = 0;

  is_recog = 0;

  rtn = TeraVadCreate();
  if (rtn) 
  {
    printf("%s - Error Creating Vad Inst !!\n", rtn);
    return NULL;
  }
  
  for(;;)
  {
    // sprintf(str,"\n===== LOOP %i of 5 ======",i); disp(cons,str);
    disp(cons,"Recording...");

    disp(cons,">>> Say 'alexa' <<<");

    done=0;

    while (!done) 
    {

        cx20921_num = 0;
        single_num = 0;
        pcm_read(pcm, cap_buf_double, size*4);
		
				for(cx20921_num=0; cx20921_num<size*4; cx20921_num++){
					if((cx20921_num%4) == 2){
						cap_buf[single_num] = cap_buf_double[cx20921_num];
						single_num++;
					}
					if((cx20921_num%4) == 3){
						cap_buf[single_num] = cap_buf_double[cx20921_num];
						single_num++;
					}
				}

        p->audio = (short int*)cap_buf;

        rtn = TeraVadProcess(p->audio, &VadState);
        // printf("vad=%d\n", VadState);
        if (VadState == 0) {
          vadcnt++;
          if (vadcnt > 400) vadcnt = 0;
          // printf("cnt=%d\n", vadcnt);
        }

        if (is_recog || is_record) {
          gettimeofday(&endTime,NULL);  
          
          Timeuse = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec - startTime.tv_usec) / 1000000; 

          if (vadcnt >= 80) {
            Timeuse = 9; 
            if (record_wait-->0){
              vadcnt = 0;
              Timeuse = 6;
            } 
          }

          if (Timeuse >= 8) {
            is_record = 0;
            is_recog = 0;
            seq = 0;
            if(g_status_cb)
              g_status_cb(g_userInfo, VAD_END);  
              send_light_msg(THINKING);
          } else {
            // printf("write .........\n");
            if((seq == 0) && g_status_cb) {
              g_status_cb(g_userInfo,VAD_BEGIN);
              vadcnt = 0;
            }

            if (g_data_cb)
              g_data_cb(g_userInfo, 0, (unsigned short*)p->audio, p->len); 

            seq++;
          }
          continue;
        }

#if SHOWAMP // display max amplitude
        unsigned short maxamp=0;
        int k;
    
        for(k=0; k < p->len; k++) {
          if (abs(p->audio[k]) > maxamp)
            maxamp=abs(p->audio[k]);
        }

        sprintf(str,"samples=%d, amplitude=%d",p->len,maxamp);
        disp(cons,str);
#endif

        if (!thfRecogPipe(ses, r, p->len, p->audio, RECOG_ONLY, &status))
          THROW("recogPipe");

        if (status == RECOG_DONE) {
          done = 1;
//          stopRecord();
        }
    }
    
    /* Report N-best recognition result */
    rres=NULL;
    if (status==RECOG_DONE) {
      disp(cons,"Recognition results...");
      score=0;
      if (!thfRecogResult(ses,r,&score,&rres,NULL,NULL,NULL,NULL,NULL,NULL)) 
        THROW("thfRecogResult");
      sprintf(str,"Result: %s (%.3f)",rres,score); disp(cons,str);

      seq = 0;
      is_recog = 1;
      vadcnt = 0;
      record_wait = 0;
      gettimeofday(&startTime,NULL);

      send_light_msg(LISTENING);

      if(g_status_cb) {
        g_status_cb(g_userInfo,WAKEN);
      }

    } else { 
      // printf("\njeffrey_test_G\n\n");    
      disp(cons,"No recognition result");
	  }
    thfRecogReset(ses,r);
  }


  printf("exit\n\n");
  thfRecogDestroy(r); r=NULL;
  thfSearchDestroy(s); s=NULL;
  thfPronunDestroy(pp); pp=NULL;
  thfSessionDestroy(ses); ses=NULL;
  disp(cons,"Done");

  free(cap_buf);
  free(cap_buf_double);
  free(p);
  TeraVadFree();

  return NULL;

  /* Async error handling, see console.h for panic */

error:

  printf("error\n\n");

  if(!ewhat && ses) ewhat=thfGetLastError(ses);

  if(r) thfRecogDestroy(r);
  if(s) thfSearchDestroy(s);
  if(pp) thfPronunDestroy(pp);
  panic(cons,ewhere,ewhat,0);
  if(ses) thfSessionDestroy(ses);
  free(cap_buf);
  free(cap_buf_double);
  free(p);
  TeraVadFree();
  return NULL;
}


void sen_startRecord(){
  record_wait = 2;
  is_record = 1;
  seq = 0;
  gettimeofday(&startTime,NULL);
}

void sen_stopRecord(){
  is_record = 0;
  is_recog = 0;
  record_wait = 0;
}




