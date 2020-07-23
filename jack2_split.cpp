#include <boost/program_options.hpp>
#include <jack/jack.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <atomic>
#include <stdint.h>

jack_client_t *jack_input_client;
jack_client_t *jack_output_client;

std::vector<jack_port_t*> in_ports;
std::vector<jack_port_t*> out_ports;
std::vector<std::vector<float>> in_buffers;
std::vector<std::vector<float>> out_buffers;

// TODO: check semantics of std::atomic to see if it really gives us what we need here...
std::atomic<jack_nframes_t> frame_time1;
std::atomic<jack_nframes_t> frame_time2;

std::atomic<jack_nframes_t> previous_frame_time;

jack_nframes_t previous_frame_time1;
jack_nframes_t previous_frame_time2;

size_t number_of_channels = 2;

void copy_buffers(jack_nframes_t nframes) {
  auto t1 = frame_time1.load(std::memory_order_acquire); 
  auto t2 = frame_time2.load(std::memory_order_acquire); 

  auto pt = previous_frame_time.load(std::memory_order_acquire);

  if (t1 == t2) {
    if ((t1 != pt) && (0 != pt) && ((t1 - pt) != nframes)) {
      std::cout << "oy - missed a buffer! (t1 - pt): " << (t1 - pt) << "\n";
    }
    for (size_t index = 0; index < number_of_channels; ++index) {
      memcpy(&(out_buffers[index][0]), &(in_buffers[index][0]), nframes*(sizeof(float)));
    }

    previous_frame_time.store(t1, std::memory_order_release);
  }
}

extern "C" {
  int buffer_size_callback(jack_nframes_t nframes, void *arg) {
    std::cout << "buffer_size_callback: " << nframes << "\n";
    for (size_t index = 0; index < number_of_channels; ++index) {
      in_buffers[index].resize(nframes);
      out_buffers[index].resize(nframes);
    }

    return 0;
  }

  int process_input(jack_nframes_t nframes, void *arg) {
    for (size_t index = 0; index < number_of_channels; ++index) {
      memcpy(&(in_buffers[index][0]), jack_port_get_buffer(in_ports[index], nframes), nframes*(sizeof(float)));
    }

    jack_nframes_t last_frame_time = jack_last_frame_time(jack_input_client);
    frame_time1.store(last_frame_time, std::memory_order_release);

    copy_buffers(nframes);

    if (previous_frame_time1 != 0 && last_frame_time - previous_frame_time1 != nframes) {
      std::cout << "ay1 - missed a buffer\n";
    } 
    previous_frame_time1 = last_frame_time;
    return 0;
  }

  int process_output(jack_nframes_t nframes, void *arg) {
    for (size_t index = 0; index < number_of_channels; ++index) {
      memcpy(jack_port_get_buffer(out_ports[index], nframes), &(out_buffers[index][0]), nframes*(sizeof(float)));
    }

    jack_nframes_t last_frame_time = jack_last_frame_time(jack_input_client);
    frame_time2.store(last_frame_time, std::memory_order_release);

    copy_buffers(nframes);

    if (previous_frame_time2 != 0 && last_frame_time - previous_frame_time2 != nframes) {
      std::cout << "ay2 - missed a buffer\n";
    } 
    previous_frame_time2 = last_frame_time;
    return 0;
  }
}

int main(int argc, char *argv[]) {
  frame_time1 = 0;
  frame_time2 = 0;

  jack_status_t jack_status;

  jack_input_client = jack_client_open("jack2_split_in", JackNullOption, &jack_status);
  jack_output_client = jack_client_open("jack2_split_out", JackNullOption, &jack_status);

  // TODO: error checking and reporting
  if (!(jack_input_client && jack_output_client)) {
    std::cout << "Failed to open at least one client. Exiting...\n";
    return EXIT_FAILURE;
  }

  in_ports.resize(number_of_channels);
  out_ports.resize(number_of_channels);
  in_buffers.resize(number_of_channels);
  out_buffers.resize(number_of_channels);

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

  jack_activate(jack_input_client);
  jack_activate(jack_output_client);
 
  // TODO: signal handling and clean exit
  while(true) {
    sleep(1);
  }
  
  return EXIT_SUCCESS;
}

