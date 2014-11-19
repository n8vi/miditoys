#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

#define CYCLEN (8192)
#define POLYPHONES (8)

jack_port_t *output_port;

static jack_default_audio_sample_t cycle[CYCLEN];

static double fskiplen[POLYPHONES], fpos[POLYPHONES];

int process (jack_nframes_t nframes, void *arg)
{
  unsigned long i, j, pos, poly;

  jack_default_audio_sample_t *out = (jack_default_audio_sample_t *) jack_port_get_buffer (output_port, nframes);

  for (i=0; i<nframes; i++) {
    out[i] = 0;
    poly = 0;
    for (j=0; j<POLYPHONES; j++) {
      if (fskiplen[j] > 0.0001) {
        fpos[j] += fskiplen[j];
        pos = fpos[j];
        pos %= CYCLEN;
        out[i] += cycle[pos];
        poly++;
        }
      }
    out[i] /= poly;
    }
  return 0;      
}

void jack_shutdown (void *arg)
{
  exit (1);
}

int main (int argc, char *argv[])
{
  jack_client_t *client;
  const char **ports;
  double freq;
  unsigned long sr;
  unsigned long i;

  if (argc != 2) {
    printf("Usage: gensquare <frequency>\n");
    exit(0);
    }

  freq = atof(argv[1]);

  if ((client = jack_client_open("gensquare", (jack_options_t)0, NULL)) == NULL) {
    fprintf (stderr, "jack server not running?\n");
    return 1;
    }

  for (i=0; i<CYCLEN; i++) {
    if (i>CYCLEN/2) {
      cycle[i] = 0.0;
    } else {
      cycle[i] = 0.2;
      }
    }

  sr = jack_get_sample_rate (client);

  fskiplen[0] = freq*CYCLEN/sr;
  fskiplen[1] = freq*CYCLEN/sr;
  fskiplen[2] = freq*CYCLEN/sr;
  fskiplen[3] = freq*CYCLEN/sr;
  fskiplen[4] = freq*CYCLEN/sr;
  fskiplen[5] = freq*CYCLEN/sr;
  fskiplen[6] = freq*CYCLEN/sr;
  fskiplen[7] = freq*CYCLEN/sr;

  jack_set_process_callback (client, process, 0);

  jack_on_shutdown (client, jack_shutdown, 0);

  output_port = jack_port_register (client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    return 1;
    }

  if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
    fprintf(stderr, "Cannot find any physical playback ports\n");
    exit(1);
    }

  if (jack_connect (client, jack_port_name (output_port), ports[0])) {
    fprintf (stderr, "cannot connect output ports\n");
    }

  free (ports);

  while(1) {
    sleep (10);
    }

  jack_client_close (client);
  exit (0);
}
