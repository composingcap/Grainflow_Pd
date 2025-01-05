#include <m_pd.h>
#include <algorithm>
#include <vector>
#include <array>
#include "PdBufferReader.h"
#include <gfGrainCollection.h>
#include <cstring>
#include "timer.h"
#include <mutex>
#include <chrono>

static t_class* grainflow_tidle_class;

namespace
{
	constexpr int internal_block = 16;

	struct iolet
	{
		t_sample* vec;
		t_int nchans;
	};
};

struct grain_info
{
public:
	std::vector<t_atom> grain_state;
	std::vector<t_atom> grain_window;
	std::vector<t_atom> grain_progress;
	std::vector<t_atom> grain_position;
	std::vector<t_atom> grain_channel;
	std::vector<t_atom> grain_stream;
	std::vector<t_atom> grain_amp;


	void resize(int size)
	{
		grain_state.resize(size + 1);
		grain_progress.resize(size + 1);
		grain_position.resize(size + 1);
		grain_amp.resize(size + 1);
		grain_window.resize(size + 1);
		grain_channel.resize(size + 1);
		grain_stream.resize(size + 1);

		grain_state[0].a_type = A_FLOAT;
		grain_state[0].a_w.w_float = 0;

		grain_progress[0].a_type = A_FLOAT;
		grain_progress[0].a_w.w_float = 1;

		grain_position[0].a_type = A_FLOAT;
		grain_position[0].a_w.w_float = 2;

		grain_amp[0].a_type = A_FLOAT;
		grain_amp[0].a_w.w_float = 3;

		grain_window[0].a_type = A_FLOAT;
		grain_window[0].a_w.w_float = 4;

		grain_channel[0].a_type = A_FLOAT;
		grain_channel[0].a_w.w_float = 5;

		grain_stream[0].a_type = A_FLOAT;
		grain_stream[0].a_w.w_float = 6;
	}
};

typedef struct _grainflow_tilde
{
	t_object x_obj;
	t_inlet* main_inlet;
	t_outlet* info_outlet;
	t_float f;
	std::array<t_inlet*, 3> inlets{};
	std::array<t_outlet*, 8> outlets{};
	t_int max_grains = 1;
	std::unique_ptr<Grainflow::gf_grain_collection<Grainflow::pd_buffer, internal_block, t_sample>> grain_collection;
	Grainflow::gf_i_buffer_reader<Grainflow::pd_buffer, t_sample> buffer_reader;
	std::array<std::vector<t_sample>, 4> input_channels;
	std::array<std::vector<t_sample*>, 4> input_channel_ptrs;
	std::array<std::vector<t_sample*>, 8> output_channel_ptrs;
	int blockSize = 0;
	int samplerate = 0;
	std::array<iolet, 4> inlet_data;
	std::array<iolet, 8> outlet_data;
	grain_info grain_data;
	std::unique_ptr<Timer> data_thread;
	std::mutex data_lock;
	bool data_update;

	bool state = false;
} t_grainflow_tilde;

void on_data_thread(int* w)
{
	auto x = reinterpret_cast<t_grainflow_tilde*>(w);
	if (!x->data_update) { return; }
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_state.size(), x->grain_data.grain_state.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_progress.size(), x->grain_data.grain_progress.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_position.size(), x->grain_data.grain_position.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_state.size(), x->grain_data.grain_amp.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_window.size(), x->grain_data.grain_window.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_state.size(), x->grain_data.grain_channel.data());
	outlet_list(x->info_outlet, &s_list, x->grain_data.grain_state.size(), x->grain_data.grain_stream.data());

	x->data_update = false;
}

void* grainflow_tilde_new(t_symbol* s, int ac, t_atom* av)
{
	s = nullptr;
	t_grainflow_tilde* x = reinterpret_cast<t_grainflow_tilde*>(pd_new(grainflow_tidle_class));
	x->max_grains = static_cast<int>(av[1].a_w.w_float);

	for (auto& inlet : x->inlets)
	{
		inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
	}
	x->outlets.at(0) = outlet_new(&x->x_obj, &s_signal);
	x->info_outlet = outlet_new(&x->x_obj, &s_list);
	for (int i = 1; i < x->outlets.size(); ++i)
	{
		auto outlet = x->outlets[i];
		outlet = outlet_new(&x->x_obj, &s_signal);
	}

	if (ac < 2)
	{
		pd_error("grainflow~: must be instantiated with two arguments- buffer-name ngrains", "");
		return (void*)x;
	}

	t_symbol* envName = gensym("default");
	if (ac > 2)
	{
		envName = av[2].a_w.w_symbol;
	}


	x->buffer_reader = Grainflow::pd_buffer_reader::get_buffer_reader();
	x->grain_collection = std::make_unique<Grainflow::gf_grain_collection<
		Grainflow::pd_buffer, internal_block, t_sample>>(
		x->buffer_reader, x->max_grains);
		
	x->grain_collection->set_active_grains(x->max_grains);

	for (int i = 0; i < x->grain_collection->grains(); ++i)
	{
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::buffer,
		                                              new Grainflow::pd_buffer(av[0].a_w.w_symbol));
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::envelope,
		                                              new Grainflow::pd_buffer(envName));
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::rate_buffer,
		                                              new Grainflow::pd_buffer());
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::delay_buffer,
		                                              new Grainflow::pd_buffer());
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::window_buffer,
		                                              new Grainflow::pd_buffer());
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::glisson_buffer,
		                                              new Grainflow::pd_buffer());
	}

	x->data_thread = std::make_unique<Timer>();
	x->data_thread->start(std::chrono::milliseconds(33), on_data_thread, reinterpret_cast<int*>(x));

	return (void*)x;
}

void grainflow_init(t_grainflow_tilde* x, t_signal** inputs)
{
	for (int i = 0; i < x->input_channels.size(); ++i)
	{
		x->input_channels[i].resize(inputs[i]->s_length * inputs[i]->s_nchans);
	}
}

void grainflow_setup_inputs(t_grainflow_tilde* x, Grainflow::gf_io_config<t_sample>& config)
{
	config.grain_clock_chans = x->inlet_data[0].nchans;
	config.traversal_phasor_chans = x->inlet_data[1].nchans;
	config.fm_chans = x->inlet_data[2].nchans;
	config.am_chans = x->inlet_data[3].nchans;

	for (int i = 0; i < x->inlet_data.size(); ++i)
	{
		const auto vec = x->inlet_data[i].vec;
		std::copy_n(vec, x->input_channels[i].size(), x->input_channels[i].data());
	}

	for (int i = 0; i < x->input_channel_ptrs.size(); ++i)
	{
		x->input_channel_ptrs[i].resize(x->inlet_data[i].nchans);
		const int chan_size = x->blockSize;
		for (int j = 0; j < x->input_channel_ptrs[i].size(); ++j)
		{
			x->input_channel_ptrs[i][j] = &(x->input_channels[i].data()[j * chan_size]);
		}
	}

	config.grain_clock = x->input_channel_ptrs[0].data();
	config.traversal_phasor = x->input_channel_ptrs[1].data();
	config.fm = x->input_channel_ptrs[2].data();
	config.am = x->input_channel_ptrs[3].data();
}

void grainflow_setup_outputs(t_grainflow_tilde* x, Grainflow::gf_io_config<t_sample>& config)
{
	for (int i = 0; i < x->output_channel_ptrs.size(); ++i)
	{
		x->output_channel_ptrs[i].resize(x->max_grains);
		const int chan_size = x->blockSize;
		for (int j = 0; j < x->max_grains; ++j)
		{
			x->output_channel_ptrs[i][j] = &(x->outlet_data[i].vec[j * chan_size]);
			std::fill_n(x->output_channel_ptrs[i][j], chan_size, 0.0f);
		}
	}

	config.grain_output = x->output_channel_ptrs[0].data();
	config.grain_state = x->output_channel_ptrs[1].data();
	config.grain_progress = x->output_channel_ptrs[2].data();
	config.grain_playhead = x->output_channel_ptrs[3].data();
	config.grain_amp = x->output_channel_ptrs[4].data();
	config.grain_envelope = x->output_channel_ptrs[5].data();
	config.grain_buffer_channel = x->output_channel_ptrs[6].data();
	config.grain_stream_channel = x->output_channel_ptrs[7].data();
}

t_int* grainflow_tilde_perform(t_int* w)
{
	auto x = reinterpret_cast<t_grainflow_tilde*>(w[1]);
	Grainflow::gf_io_config<t_sample> config;
	config.block_size = x->blockSize;
	config.samplerate = x->samplerate;
	config.livemode = true; //We do not know the buffer samplerate
	grainflow_setup_inputs(x, config);
	grainflow_setup_outputs(x, config);
	if (!x->state) return w + 2;
	x->grain_collection->process(config);

	if (x->data_update) { return w + 2; }
	
	for (int i = 0; i < x->grain_collection->active_grains(); ++i)
	{
		auto j = i + 1;
		x->grain_data.grain_stream[j].a_w.w_float = config.grain_stream_channel[i][0];
		x->grain_data.grain_stream[j].a_type = A_FLOAT;
		x->grain_data.grain_state[j].a_w.w_float = config.grain_state[i][0];
		x->grain_data.grain_state[j].a_type = A_FLOAT;
		x->grain_data.grain_channel[j].a_w.w_float = config.grain_buffer_channel[i][0];
		x->grain_data.grain_channel[j].a_type = A_FLOAT;
		x->grain_data.grain_window[j].a_w.w_float = config.grain_envelope[i][0];
		x->grain_data.grain_window[j].a_type = A_FLOAT;
		x->grain_data.grain_position[j].a_w.w_float = config.grain_playhead[i][0];
		x->grain_data.grain_position[j].a_type = A_FLOAT;
		x->grain_data.grain_progress[j].a_w.w_float = config.grain_progress[i][0];
		x->grain_data.grain_progress[j].a_type = A_FLOAT;
		x->grain_data.grain_amp[j].a_w.w_float = config.grain_amp[i][0];
		x->grain_data.grain_amp[j].a_type = A_FLOAT;
	}
	x->data_update = true;
	

	return w + 2;
}

void grainflow_tilde_float(t_grainflow_tilde* x, t_floatarg f)
{
	x->state = f >= 1;
}

void grainflow_tilde_anything(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 1) return;
	int start = 0;
	if (strcmp(s->s_name, "list") == 0)
	{
		start = 1;
		s = av[0].a_w.w_symbol;
	}
	auto success = Grainflow::GF_RETURN_CODE::GF_ERR;
	if (ac < 2 + start)
	{
		success = x->grain_collection->param_set(0, std::string(s->s_name), av[0 + start].a_w.w_float);
	}
	else
	{
		for (int i = 0; i < x->grain_collection->grains(); ++i)
		{
			success = x->grain_collection->param_set(i, std::string(s->s_name),
			                                         av[(i % (ac - start)) + start].a_w.w_float);
		}
	}
	if (success != Grainflow::GF_RETURN_CODE::GF_SUCCESS)
	{
		Grainflow::gf_buffers type;
		if (buffer_reflection(s->s_name, type))
		{
			if (ac < 2)
			{
				for (int i = 0; i < x->grain_collection->grains(); ++i)
				{
					x->grain_collection->get_buffer(type, i)->set(av[0].a_w.w_symbol);
					if (type == Grainflow::gf_buffers::buffer) {x->grain_collection->param_set(i, Grainflow::gf_param_name::channel, Grainflow::gf_param_type::value, 1);}
				}
			}
			else
			{
				auto arg_counter = 0;
				auto buffer_counter = 0;
				auto n_buffers = 0;
				for(int i = 0; i < ac; ++i){if (av[i].a_type==A_SYMBOL) ++n_buffers;}
				if (n_buffers < 1) {return;}
				for (int i = 0; i < x->grain_collection->grains(); ++i)
				{
					x->grain_collection->get_buffer(type, i)->channels = n_buffers;
					auto tries = 0;
					while (av[arg_counter % (ac - start) + start].a_type != t_atomtype::A_SYMBOL)
					{
						if (tries > 3) return;
						arg_counter++;
						tries++;
					};
					x->grain_collection->get_buffer(type, i)->set(av[arg_counter % (ac - start) + start].a_w.w_symbol);
					arg_counter++;
					if (type == Grainflow::gf_buffers::envelope || type == Grainflow::gf_buffers::glisson_buffer)
					{
						auto n_param = type == Grainflow::gf_buffers::envelope ? Grainflow::gf_param_name::n_envelopes : Grainflow::gf_param_name::glisson_rows;
						if (av[arg_counter % (ac - start) + start].a_type == t_atomtype::A_FLOAT)
						{
							x->grain_collection->param_set(i + 1, n_param,
							                               Grainflow::gf_param_type::value,
							                               static_cast<float>(av[arg_counter % (ac - start) + start].a_w
								                               .w_float));
							arg_counter++;
						}
						else
						{
							x->grain_collection->param_set(i + 1, n_param, Grainflow::gf_param_type::value, 1);
						}
					}
					else if (type == Grainflow::gf_buffers::buffer) {x->grain_collection->param_set(i, Grainflow::gf_param_name::channel, Grainflow::gf_param_type::base, buffer_counter%n_buffers);}
					buffer_counter++;
				}
			}
			return;
		}
		pd_error(x, "grainflow: %s is not a valid parameter name", s->s_name);
	}
}

static void grainflow_tilde_dsp(t_grainflow_tilde* x, t_signal** sp)
{
	x->max_grains = std::max<t_int>(x->max_grains, 1);
	x->grain_data.resize(x->grain_collection->active_grains());
	grainflow_init(x, sp);
	for (int i = 0; i < x->outlets.size(); ++i)
	{
		signal_setmultiout(&sp[x->inlets.size() + 1 + i], x->max_grains); //Main inlet + additional ones
		x->outlet_data[i].nchans = x->max_grains;
		x->outlet_data[i].vec = sp[x->inlets.size() + 1 + i]->s_vec;
	}
	for (int i = 0; i < x->inlet_data.size(); ++i)
	{
		x->inlet_data[i].vec = sp[i]->s_vec;
		x->inlet_data[i].nchans = sp[i]->s_nchans;
	}
	x->blockSize = sp[0]->s_length;
	x->samplerate = sp[0]->s_sr;
	dsp_add(grainflow_tilde_perform, 1, x);
}

void grainflow_stream_set(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 1) { return; }

	if (ac < 2)
	{
		if (av[0].a_type != A_FLOAT) { return; }
		x->grain_collection->stream_set(Grainflow::gf_stream_set_type::automatic_streams, av[1].a_w.w_float);
		return;
	}
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT) { return; }
	auto typeSym = av[0].a_w.w_symbol;
	Grainflow::gf_stream_set_type type;
	if (strcmp(typeSym->s_name, "auto") == 0) { type = Grainflow::gf_stream_set_type::automatic_streams; }
	else if (strcmp(typeSym->s_name, "per") == 0) { type = Grainflow::gf_stream_set_type::per_streams; }
	else if (strcmp(typeSym->s_name, "random") == 0) { type = Grainflow::gf_stream_set_type::random_streams; }
	x->grain_collection->stream_set(type, av[1].a_w.w_float);
}

void grainflow_stream_param_set(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_FLOAT || av[1].a_type != A_SYMBOL) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;

	if (Grainflow::param_reflection(av[1].a_w.w_symbol->s_name, param, type))
	{
		if (av[2].a_type != A_FLOAT) { return; }
		x->grain_collection->stream_param_set(av[0].a_w.w_float, param, type, av[2].a_w.w_float);
		return;
	}
}

void grainflow_stream_param_deviate(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type))
	{
		x->grain_collection->stream_param_func(param, type, &Grainflow::gf_utils::deviate, av[1].a_w.w_float,
		                                       av[2].a_w.w_float);
		return;
	}
}

void grainflow_stream_param_spread(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type))
	{
		x->grain_collection->stream_param_func(param, type, &Grainflow::gf_utils::lerp, av[1].a_w.w_float,
		                                       av[2].a_w.w_float);
		return;
	}
}

void grainflow_param_message(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_FLOAT || av[1].a_type != A_SYMBOL) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	int grain_id = av[0].a_w.w_float;

	if (Grainflow::param_reflection(av[1].a_w.w_symbol->s_name, param, type))
	{
		if (av[2].a_type != A_FLOAT) { return; }
		x->grain_collection->param_set(grain_id, param, type, av[2].a_w.w_float);
		return;
	}
	Grainflow::gf_buffers buffer_type;
	if (Grainflow::buffer_reflection(av[1].a_w.w_symbol->s_name, buffer_type))
	{
		if (av[2].a_type != A_SYMBOL) { return; }
		x->grain_collection->get_buffer(buffer_type, grain_id)->set(av[2].a_w.w_symbol);
		return;
	}
}

void grainflow_param_deviate(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type))
	{
		x->grain_collection->grain_param_func(param, type, &Grainflow::gf_utils::deviate, av[1].a_w.w_float,
		                                      av[2].a_w.w_float);
		return;
	}
}

void grainflow_param_spread(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 3) { return; }
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) { return; }
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type))
	{
		x->grain_collection->grain_param_func(param, type, &Grainflow::gf_utils::lerp, av[1].a_w.w_float,
		                                      av[2].a_w.w_float);
		return;
	}
}

void grainflow_ngrains(t_grainflow_tilde* x, t_floatarg f){

	x->grain_collection->set_active_grains(f);
	x->grain_data.resize(x->max_grains);
}

void grainflow_auto_overlap(t_grainflow_tilde* x, t_floatarg f){
	x->grain_collection->set_auto_overlap(f >= 1);
}


extern "C" void grainflow_tilde_setup(void)
{
	grainflow_tidle_class = class_new(gensym("grainflow~"),
	                                  reinterpret_cast<t_newmethod>(grainflow_tilde_new),
	                                  0, sizeof(t_grainflow_tilde),
	                                  CLASS_MULTICHANNEL, A_GIMME, 0);
	class_addmethod(grainflow_tidle_class,
	                reinterpret_cast<t_method>(grainflow_tilde_dsp), gensym("dsp"), A_CANT, 0);
	CLASS_MAINSIGNALIN(grainflow_tidle_class, t_grainflow_tilde, f);
	class_addfloat(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_tilde_float));
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_tilde_float), gensym("state"),
	                A_DEFFLOAT, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_param_message), gensym("g"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_ngrains), gensym("ngrains"),
	                A_DEFFLOAT, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_auto_overlap), gensym("autoOverlap"),
	                A_DEFFLOAT, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_param_deviate), gensym("deviate"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_param_spread), gensym("spread"),
	                A_GIMME, 0);

	//Streams
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_set), gensym("streamSet"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_set), gensym("stream"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_deviate),
	                gensym("streamDeviate"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_spread),
	                gensym("streamSpread"),
	                A_GIMME, 0);

	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_tilde_anything), gensym("anything"),
	                A_GIMME, 0);
}
