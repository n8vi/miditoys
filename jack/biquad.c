/** @file simple_client.c
 *
 * @brief This is very simple client that demonstrates the basic
 * features of JACK as they would be used by many applications.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <math.h>

jack_port_t *input_port;
jack_port_t *output_port;

//init
static double mX1 = 0;
static double mX2 = 0;
static double mY1 = 0;
static double mY2 = 0;
static double pi = 22/7;

static double cutoff;

static double sr;

double biquad_filter(double input, double cutoff, double res)
{
  //res_slider range -25/25db

  cutoff = 2 * cutoff / sr;
  res = pow(10, 0.05 * -res);
  double k = 0.5 * res * sin(pi * cutoff);
  double c1 = 0.5 * (1 - k) / (1 + k);
  double c2 = (0.5 + c1) * cos(pi * cutoff);
  double c3 = (0.5 + c1 - c2) * 0.25;
    
  double mA0 = 2 * c3;
  double mA1 = 2 * 2 * c3;
  double mA2 = 2 * c3;
  double mB1 = 2 * -c2;
  double mB2 = 2 * c1;

  //loop
  double output = mA0*input + mA1*mX1 + mA2*mX2 - mB1*mY1 - mB2*mY2;

  mX2 = mX1;
  mX1 = input;
  mY2 = mY1;
  mY1 = output;

  return output;
}


/**
 * The process callback for this JACK application.
 * It is called by JACK at the appropriate times.
 */
int
process (jack_nframes_t nframes, void *arg)
{
  int i;

  jack_default_audio_sample_t *out = (jack_default_audio_sample_t *) jack_port_get_buffer (output_port, nframes);
  jack_default_audio_sample_t *in = (jack_default_audio_sample_t *) jack_port_get_buffer (input_port, nframes);

  for(i=0; i<nframes; i++) {
    out[i] = biquad_filter(in[i],cutoff,6);
    }
        
  return 0;      
}

/**
 * This is the shutdown callback for this JACK application.
 * It is called by JACK if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{

        exit (1);
}

int
main (int argc, char *argv[])
{
        jack_client_t *client;
        const char **ports;
        int i;

        if (argc != 2) {
          printf("cutoff?\n");
          exit(0);
          }

        cutoff = atof(argv[1]);

        /* try to become a client of the JACK server */

        if ((client = jack_client_open("biquad", (jack_options_t)0, NULL)) == NULL) {
                fprintf (stderr, "jack server not running?\n");
                return 1;
        }

        /* tell the JACK server to call `process()' whenever
           there is work to be done.
        */

        jack_set_process_callback (client, process, 0);

        /* tell the JACK server to call `jack_shutdown()' if
           it ever shuts down, either entirely, or if it
           just decides to stop calling us.
        */

        jack_on_shutdown (client, jack_shutdown, 0);

        /* display the current sample rate. 
         */

        sr = jack_get_sample_rate (client);

        printf ("engine sample rate: %f\n", sr);

        /* create two ports */

        input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        output_port = jack_port_register (client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        /* tell the JACK server that we are ready to roll */

        if (jack_activate (client)) {
                fprintf (stderr, "cannot activate client");
                return 1;
        }

        /* connect the ports. Note: you can't do this before
           the client is activated, because we can't allow
           connections to be made to clients that aren't
           running.
        */

        if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsOutput)) == NULL) {
                fprintf(stderr, "Cannot find any physical capture ports\n");
                exit(1);
        }

        i = 0;
        while(ports[i] != NULL) {
          if (strstr(ports[i], "jsynthosc")) { 
            printf("connecting %s\n", ports[i]);
            if (jack_connect (client, ports[i], jack_port_name (input_port))) {
              fprintf (stderr, "cannot connect input ports\n");
              }
            }
            i++;
          }

        free (ports);
        
        if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
                fprintf(stderr, "Cannot find any physical playback ports\n");
                exit(1);
        }

        if (jack_connect (client, jack_port_name (output_port), ports[0])) {
                fprintf (stderr, "cannot connect output ports\n");
        }

        free (ports);

        /* Since this is just a toy, run for a few seconds, then finish */

        while(1){
          sleep (10);
          }
        jack_client_close (client);
        exit (0);
}
