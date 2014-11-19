#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <math.h>

#ifdef __MINGW32__
#include <pthread.h>
#endif

#ifndef WIN32
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#endif

#ifndef MAX
#define MAX(a,b) ( (a) < (b) ? (b) : (a) )
#endif

#define CYCLEN (8192)
#define POLYPHONES (16)

#ifndef PI
#define PI (3.1415926536)
#endif

jack_port_t *output_port;

static unsigned long sr;

static double pbend = 1.0;
static double dutyc = 1.0;


static jack_default_audio_sample_t cycle[CYCLEN];

static double fskiplen[POLYPHONES], fpos[POLYPHONES], fvel[POLYPHONES];

static jack_port_t* port;
static jack_ringbuffer_t *rb = NULL;
static pthread_mutex_t msg_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t data_ready = PTHREAD_COND_INITIALIZER;

static int keeprunning = 1;
static uint64_t monotonic_cnt = 0;

#define RBSIZE 512

typedef struct {
  uint8_t  buffer[128];
  uint32_t size;
  uint32_t tme_rel;
  uint64_t tme_mon;
  } midimsg;

void error_cb(const char *msg)
{
  fprintf(stderr, "JACK ERROR: %s\n", msg);
}



static void handlemsg (midimsg* event)
{
  int i;

  if (event->size == 0) {
    return;
    }

  uint8_t type = event->buffer[0] & 0xf0;
  uint8_t channel = event->buffer[0] & 0xf;

  switch (type) {
    case 0x90:
      assert (event->size == 3);
      printf(" ON: chan %2d vel %3d freq %f\n", channel, event->buffer[2], 32*exp2(event->buffer[1]/12.0));
      for (i=0; i<POLYPHONES; i++) {
        if (fskiplen[i] < .001) {
          fskiplen[i] = (32*exp2(event->buffer[1]/12.0))*CYCLEN/sr;
          fvel[i] = event->buffer[2]/127.0;
          break;
          }
        }
      break;
    case 0x80:
      assert (event->size == 3);
      // printf("OFF: chan %2d vel %3d freq %f\n", channel, event->buffer[2], exp2(event->buffer[1]/12.0));
      for (i=0; i<POLYPHONES; i++) {
        if (fskiplen[i] - (32*exp2(event->buffer[1]/12.0))*CYCLEN/sr < .001 && fskiplen[i] - (32*exp2(event->buffer[1]/12.0))*CYCLEN/sr > -.001) {
          fskiplen[i] = 0;
          }
        }
      break;
    case 0xb0:
      assert (event->size == 3);
      printf(" CC: chan %2d ctl %3d  val %3d\n", channel, event->buffer[1], event->buffer[2]);
      switch (event->buffer[1]) {
        case 0x01:
          dutyc = 1+ ((event->buffer[2] - 63.0)/128.0);
          printf("%f", dutyc);
          break;
        }
      break;
    case 0xc0:
      // patch
      printf("%d\n", event->size);
      assert (event->size == 2);
      printf("PCH: chan %d %d\n", channel, event->buffer[1]);
        switch (event->buffer[1]) {
          case 0x01:
            for (i=0; i<CYCLEN; i++) {
              if (i>CYCLEN/2+1) {
                cycle[i] = 0.0;
              } else {
                cycle[i] = 0.2;
                }
              }
            break;
          case 0x02:
            for (i=0; i<CYCLEN; i++) {
              if (i<CYCLEN/2) {
                cycle[i] = 0.4*i/CYCLEN;
              } else {
                cycle[i] = 0.4*(CYCLEN-i)/CYCLEN;
                }
              }
            break;
          case 0x03:
            for (i=0; i<CYCLEN; i++) {
              cycle[i] = 0.2*sin(2*PI*i/CYCLEN);
              }
            break;
          case 0x04:
            for (i=0; i<CYCLEN; i++) {
                cycle[i] = 0.2*i/CYCLEN;
                }
            break;
            }
          break;
    case 0xe0:
      // pitch
      assert (event->size == 3);
      pbend = 1 + (((event->buffer[2]-64.0)/64.0) / 12.0);
      break;
    default:
      break;
    }
}

int process (jack_nframes_t frames, void* arg)
{
  unsigned long j, pos;
  void* buffer;
  jack_nframes_t N;
  jack_nframes_t i;

  buffer = jack_port_get_buffer (port, frames);
  assert (buffer);

  N = jack_midi_get_event_count (buffer);
  for (i = 0; i < N; ++i) {
    jack_midi_event_t event;
    int r;

    r = jack_midi_event_get (&event, buffer, i);

    if (r == 0 && jack_ringbuffer_write_space (rb) >= sizeof(midimsg)) {
      midimsg m;
      m.tme_mon = monotonic_cnt;
      m.tme_rel = event.time;
      m.size    = event.size;
      memcpy (m.buffer, event.buffer, MAX(sizeof(m.buffer), event.size));
      jack_ringbuffer_write (rb, (void *) &m, sizeof(midimsg));
      }

    }

  monotonic_cnt += frames;

  if (pthread_mutex_trylock (&msg_thread_lock) == 0) {
    pthread_cond_signal (&data_ready);
    pthread_mutex_unlock (&msg_thread_lock);
    }

  jack_default_audio_sample_t *out = (jack_default_audio_sample_t *) jack_port_get_buffer (output_port, frames);

  for (i=0; i<frames; i++) {
    out[i] = 0;
    for (j=0; j<POLYPHONES; j++) {
      if (fskiplen[j] > 0.0001) {
        fpos[j] += fskiplen[j]*pbend;
        pos = fpos[j];
        pos %= CYCLEN;
        out[i] += cycle[lround(pos*dutyc)%CYCLEN]*fvel[j];
        }
      }
    }

  return 0;
}

static void wearedone(int sig) 
{
  keeprunning = 0;
}

int main (int argc, char* argv[])
{
  jack_client_t* client;
  char const default_name[] = "jsynthosc";
  char const * client_name;
  int time_format = 0;
  int r;
  const char **ports;
  int i;


  int cn = 1;


  fskiplen[0] = 0;
  fskiplen[1] = 0;
  fskiplen[2] = 0;
  fskiplen[3] = 0;
  fskiplen[4] = 0;
  fskiplen[5] = 0;
  fskiplen[6] = 0;
  fskiplen[7] = 0;

  for (i=0; i<CYCLEN; i++) {
    if (i>CYCLEN/2) {
      cycle[i] = 0.0;
    } else {
      cycle[i] = 0.2;
      }
    }

  jack_set_error_function(error_cb);
  client = jack_client_open ("jsynthosc", JackNullOption, NULL);
  if (client == NULL) {
    fprintf (stderr, "Could not create JACK client.\n");
    exit (EXIT_FAILURE);
    }

  sr = jack_get_sample_rate (client);

  rb = jack_ringbuffer_create (RBSIZE * sizeof(midimsg));

  jack_set_process_callback (client, process, 0);

  port = jack_port_register (client, "input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  if (port == NULL) {
    fprintf (stderr, "Could not register port.\n");
    exit (EXIT_FAILURE);
    }

  output_port = jack_port_register (client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  ports = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);



  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    return 1;
    }

  if (ports != NULL) {
    for (i = 0; ports[i] != NULL; i++) {
      if (! strstr(ports[i], "Through")) {
        printf("Connecting to %s\n", ports[i]);
        if (jack_connect (client, ports[i], jack_port_name (port))) {
          fprintf (stderr, "cannot connect \"%s\" to \"%s\"\n", ports[i], jack_port_name (port));
          // perror("cannot connect");
          }
        break;
        }
      }
    }




  if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
    fprintf(stderr, "Cannot find any physical playback ports\n");
    exit(1);
    }

  for (i=0; i<2; i++) {
    if (jack_connect (client, jack_port_name (output_port), ports[i])) {
      fprintf (stderr, "cannot connect output ports\n");
      }
    }

  free (ports);


#ifndef WIN32
  if (mlockall (MCL_CURRENT | MCL_FUTURE)) {
    fprintf (stderr, "Warning: Can not lock memory.\n");
    }
#endif

  r = jack_activate (client);
  if (r != 0) {
    fprintf (stderr, "Could not activate client.\n");
    exit (EXIT_FAILURE);
    }

#ifndef WIN32
  signal(SIGHUP, wearedone);
  signal(SIGINT, wearedone);
#endif

  pthread_mutex_lock (&msg_thread_lock);

  while (keeprunning) {
    const int mqlen = jack_ringbuffer_read_space (rb) / sizeof(midimsg);
    int i;
    for (i=0; i < mqlen; ++i) {
      size_t j;
      midimsg m;
      jack_ringbuffer_read(rb, (char*) &m, sizeof(midimsg));

      handlemsg (&m);
      }
    fflush (stdout);
    pthread_cond_wait (&data_ready, &msg_thread_lock);
    }
  pthread_mutex_unlock (&msg_thread_lock);
  
  jack_deactivate (client);
  jack_client_close (client);
  jack_ringbuffer_free (rb);
  
  return 0;
}
