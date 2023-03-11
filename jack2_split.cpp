#include <boost/program_options.hpp>
#include <jack/jack.h>
#include <jack/control.h>
#include <stdbool.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>
#include <atomic>
#include <stdint.h>

jack_client_t *jack_input_client;
jack_client_t *jack_output_client;

std::vector<jack_port_t*> in_ports;
std::vector<jack_port_t*> out_ports;

typedef std::pair<std::atomic<jack_nframes_t>, std::vector<std::vector<float>>> timestamped_buffers;
typedef std::pair<timestamped_buffers, timestamped_buffers> timestamped_double_buffers;

bool input_to_first_buffer = true;

timestamped_double_buffers buffers;

size_t number_of_channels = 2;

extern "C" {
  int buffer_size_callback(jack_nframes_t nframes, void *arg) {
    // jack_info("buffer_size_callback: %d", nframes);
    for (size_t index = 0; index < number_of_channels; ++index) {
      buffers.first.second[index].resize(nframes);
      buffers.first.first = 0;
      buffers.second.second[index].resize(nframes);
      buffers.second.first = 0;
    }

    return 0;
  }

  int process_input(jack_nframes_t nframes, void *arg) {
    const jack_nframes_t last_frame_time = jack_last_frame_time(jack_input_client);

    for (size_t index = 0; index < number_of_channels; ++index) {
      if (input_to_first_buffer) {
        memcpy(&(buffers.first.second[index][0]), jack_port_get_buffer(in_ports[index], nframes), nframes*(sizeof(float)));
        buffers.first.first = last_frame_time;
      } else {
        memcpy(&(buffers.second.second[index][0]), jack_port_get_buffer(in_ports[index], nframes), nframes*(sizeof(float)));
        buffers.second.first = last_frame_time;
      }
    }

    input_to_first_buffer = !input_to_first_buffer;
    return 0;
  }

  int process_output(jack_nframes_t nframes, void *arg) {
    const jack_nframes_t last_frame_time = jack_last_frame_time(jack_input_client);
    const size_t buffer_size = buffers.first.second[0].size();
    const bool use_first_buffer = (buffers.first.first == last_frame_time - buffer_size);
    const bool use_second_buffer = (buffers.second.first == last_frame_time - buffer_size);

    // jack_info("buffer %d %d", (int)use_first_buffer, (int)use_second_buffer);
    if (!use_first_buffer && !use_second_buffer) {
      jack_info("No buffer available");
    }

    for (size_t index = 0; index < number_of_channels; ++index) {
      if (use_first_buffer) {
        memcpy(jack_port_get_buffer(out_ports[index], nframes), &(buffers.first.second[index][0]), nframes*(sizeof(float)));
      } 
      else 
      if (use_second_buffer) {
        memcpy(jack_port_get_buffer(out_ports[index], nframes), &(buffers.first.second[index][0]), nframes*(sizeof(float)));
      }
      else {
        memset(jack_port_get_buffer(out_ports[index], nframes), 0, nframes*sizeof(float)); 
      }
    }
 
    return 0;
  }
}

int main(int argc, char *argv[]) {
  jack_status_t jack_status;

  jack_output_client = jack_client_open("jack2_split_out", JackNullOption, &jack_status);
  jack_input_client = jack_client_open("jack2_split_in", JackNullOption, &jack_status);

  // TODO: error checking and reporting
  if (!(jack_input_client && jack_output_client)) {
    std::cout << "Failed to open at least one client. Exiting...\n";
    return EXIT_FAILURE;
  }

  in_ports.resize(number_of_channels);
  out_ports.resize(number_of_channels);

  buffers.first.second.resize(number_of_channels);
  buffers.second.second.resize(number_of_channels);

  for (size_t index = 0; index < number_of_channels; ++index) {
    std::stringstream out_name_stream;
    out_name_stream << "output" << index;
    std::stringstream in_name_stream;
    in_name_stream << "input" << index;

    // TODO: error checking and reporting
    in_ports[index] = jack_port_register(jack_input_client, in_name_stream.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
    out_ports[index] = jack_port_register(jack_output_client, out_name_stream.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);
  }

  jack_set_process_callback(jack_input_client, process_input, 0);
  jack_set_process_callback(jack_output_client, process_output, 0);

  jack_set_buffer_size_callback(jack_input_client, buffer_size_callback, 0);

  std::cout << "   Activating output client..." << std::endl << std::flush;
  jack_activate(jack_output_client);
  std::cout << "...Activating input client..." << std::endl << std::flush;
  jack_activate(jack_input_client);
  std::cout << "...Done." << std::endl << std::flush;
 
  // TODO: signal handling and clean exit
  while(true) {
    sleep(1);
  }
  
  return EXIT_SUCCESS;
}

