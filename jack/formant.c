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

jack_port_t *input_port;
jack_port_t *output_port;

static int vowel;

/*
Public source code by alex@smartelectronix.com
Simple example of implementation of formant filter
Vowelnum can be 0,1,2,3,4 <=> A,E,I,O,U
Good for spectral rich input like saw or square
*/
//-------------------------------------------------------------VOWEL COEFFICIENTS
const double coeff[5][11]= {
{ 8.11044e-06,
8.943665402,    -36.83889529,    92.01697887,    -154.337906,    181.6233289,
-151.8651235,   89.09614114,    -35.10298511,    8.388101016,    -0.923313471  ///A
},
{4.36215e-06,
8.90438318,    -36.55179099,    91.05750846,    -152.422234,    179.1170248,  ///E
-149.6496211,87.78352223,    -34.60687431,    8.282228154,    -0.914150747
},
{ 3.33819e-06,
8.893102966,    -36.49532826,    90.96543286,    -152.4545478,    179.4835618,
-150.315433,    88.43409371,    -34.98612086,    8.407803364,    -0.932568035  ///I
},
{1.13572e-06,
8.994734087,    -37.2084849,    93.22900521,    -156.6929844,    184.596544,   ///O
-154.3755513,    90.49663749,    -35.58964535,    8.478996281,    -0.929252233
},
{4.09431e-07,
8.997322763,    -37.20218544,    93.11385476,    -156.2530937,    183.7080141,  ///U
-153.2631681,    89.59539726,    -35.12454591,    8.338655623,    -0.910251753
}
};
//---------------------------------------------------------------------------------
static double memory[10]={0,0,0,0,0,0,0,0,0,0};
//---------------------------------------------------------------------------------
double formant_filter(double in, int vowelnum)
{
   double res= (double) ( coeff[vowelnum][0]  *in +
                     coeff[vowelnum][1]  *memory[0] +  
                     coeff[vowelnum][2]  *memory[1] +
                     coeff[vowelnum][3]  *memory[2] +
                     coeff[vowelnum][4]  *memory[3] +
                     coeff[vowelnum][5]  *memory[4] +
                     coeff[vowelnum][6]  *memory[5] +
                     coeff[vowelnum][7]  *memory[6] +
                     coeff[vowelnum][8]  *memory[7] +
                     coeff[vowelnum][9]  *memory[8] +
                     coeff[vowelnum][10] *memory[9] );

memory[9]= memory[8];
memory[8]= memory[7];
memory[7]= memory[6];
memory[6]= memory[5];
memory[5]= memory[4];
memory[4]= memory[3];
memory[3]= memory[2];
memory[2]= memory[1];                    
memory[1]= memory[0];
memory[0]=(double) res;
return res;
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
    out[i] = formant_filter(in[i],vowel);
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
          printf("which vowel?\n");
          exit(0);
          }

        vowel = atoi(argv[1]);

        if (vowel < 0 || vowel > 4) {
          printf("invalid vowel\n");
          exit(1);
          }

        /* try to become a client of the JACK server */

        if ((client = jack_client_open("formant", (jack_options_t)0, NULL)) == NULL) {
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

        printf ("engine sample rate: %d\n", // " PRIu"\n",
                jack_get_sample_rate (client));

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
