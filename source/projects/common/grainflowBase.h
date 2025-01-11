#pragma once
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

namespace Grainflow{
    
	struct iolet
	{
		t_sample* vec;
		t_int nchans;
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
template<typename T, int internal_block>
class Grainflow_Base{
    public:
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

    public:
    static void on_data_thread(int* w)
    {
        auto x = reinterpret_cast<T*>(w);
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

    static void message_anything(T* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 1) return;
	if (strcmp(s->s_name, "state") == 0)
	{
		x->state = av->a_type == A_FLOAT ? av->a_w.w_float >= 1 : 0;
		return;
	}
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
					if (type == Grainflow::gf_buffers::buffer)
					{
						x->grain_collection->param_set(i, Grainflow::gf_param_name::channel,
						                               Grainflow::gf_param_type::value, 1);
					}
				}
			}
			else
			{
				auto arg_counter = 0;
				auto buffer_counter = 0;
				auto n_buffers = 0;
				for (int i = 0; i < ac; ++i) { if (av[i].a_type == A_SYMBOL) ++n_buffers; }
				if (n_buffers < 1) { return; }
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
						auto n_param = type == Grainflow::gf_buffers::envelope
							               ? Grainflow::gf_param_name::n_envelopes
							               : Grainflow::gf_param_name::glisson_rows;
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
					else if (type == Grainflow::gf_buffers::buffer)
					{
						x->grain_collection->param_set(i, Grainflow::gf_param_name::channel,
						                               Grainflow::gf_param_type::base, buffer_counter % n_buffers);
					}
					buffer_counter++;
				}
			}
			return;
		}
		pd_error(x, "grainflow: %s is not a valid parameter name", s->s_name);
	}
}

static void arg_parse(T* x, int ac, t_atom* av)
{
	int i = 0;
	while (i < ac)
	{
		if (av[i].a_type != A_SYMBOL)
		{
			++i;
			continue;
		}
		auto arg_name = av[i].a_w.w_symbol;
		if (arg_name->s_name[0] != '-')
		{
			++i;
			continue;
		}

		auto str_arg_name = &arg_name->s_name[1];
		auto arg_pin = i;
		++i;
		auto arg_count = 0;
		while (i < ac)
		{
			if (av[i].a_type == A_SYMBOL)
			{
				auto arg_field = av[i].a_w.w_symbol;
				if (arg_field->s_name[0] == '-') break;
			}
			++arg_count;
			++i;
		}
		message_anything(x, gensym(str_arg_name), arg_count, &av[arg_pin + 1]);
	}
}

static void* grainflow_create(T* x, int ac, t_atom* av)
{

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

	arg_parse(x, ac - 2, &av[2]);

	x->data_thread = std::make_unique<Timer>();
	x->data_thread->start(std::chrono::milliseconds(33), on_data_thread, reinterpret_cast<int*>(x));

	return (void*)x;
}

static void message_float(T* x, t_floatarg f)
{
	x->state = f >= 1;
}


static void param_message(T* x, t_symbol* s, int ac, t_atom* av)
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

static void stream_set(T* x, t_symbol* s, int ac, t_atom* av)
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

static void stream_param_set(T* x, t_symbol* s, int ac, t_atom* av)
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

static void stream_param_deviate(T* x, t_symbol* s, int ac, t_atom* av)
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

static void stream_param_spread(T* x, t_symbol* s, int ac, t_atom* av)
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


static void param_deviate(T* x, t_symbol* s, int ac, t_atom* av)
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

static void param_spread(T* x, t_symbol* s, int ac, t_atom* av)
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

static void set_ngrains(T* x, t_floatarg f)
{
	x->grain_collection->set_active_grains(f);
	x->grain_data.resize(x->max_grains);
}

static void set_auto_overlap(T* x, t_floatarg f)
{
	x->grain_collection->set_auto_overlap(f >= 1);
}

static void grainflow_free(T* x)
{
	x->data_thread->stop();
}


static void grainflow_init(T* x, t_signal** inputs)
{
	for (int i = 0; i < x->input_channels.size(); ++i)
	{
		x->input_channels[i].resize(inputs[i]->s_length * inputs[i]->s_nchans);
	}
}

static void collect_grain_info (grain_info* grain_data, const gf_io_config<t_sample>* config, const int grains) {
	for (int i = 0; i < grains; ++i)
	{
		auto j = i + 1;
		grain_data->grain_stream[j].a_w.w_float = config->grain_stream_channel[i][0];
		grain_data->grain_stream[j].a_type = A_FLOAT;
		grain_data->grain_state[j].a_w.w_float = config->grain_state[i][0];
		grain_data->grain_state[j].a_type = A_FLOAT;
		grain_data->grain_channel[j].a_w.w_float = config->grain_buffer_channel[i][0];
		grain_data->grain_channel[j].a_type = A_FLOAT;
		grain_data->grain_window[j].a_w.w_float = config->grain_envelope[i][0];
		grain_data->grain_window[j].a_type = A_FLOAT;
		grain_data->grain_position[j].a_w.w_float = config->grain_playhead[i][0];
		grain_data->grain_position[j].a_type = A_FLOAT;
		grain_data->grain_progress[j].a_w.w_float = config->grain_progress[i][0];
		grain_data->grain_progress[j].a_type = A_FLOAT;
		grain_data->grain_amp[j].a_w.w_float = config->grain_amp[i][0];
		grain_data->grain_amp[j].a_type = A_FLOAT;
	}

}

};
}