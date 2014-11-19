#include <jack/jack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error_cb(const char *msg)
{
  fprintf(stderr, "JACK ERROR: %s\n", msg);
}

int main(void)
{
  int i;
  const char **available;


  jack_client_t* client;

  jack_set_error_function(error_cb);

  client = jack_client_open ("midils", JackNullOption, NULL);

  if (client == NULL) {
    fprintf (stderr, "Could not create JACK client.\n");
    return 1;
    }

  available = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);

  if (available != NULL) {
    for (i = 0; available[i] != NULL; i++) {
      if (! strstr(available[i], "Through")) {
        printf("%s\n", available[i]);
        }
      }
    }

  free(available);
  return 0;
}

