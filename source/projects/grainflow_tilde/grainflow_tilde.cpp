#include <m_pd.h>
#include <algorithm>
#include <vector>
#include <array>
#include "PdBufferReader.h"
#include <gfGrainCollection.h>
#include <format>


static t_class* grainflow_tile_class;

namespace
{
	constexpr int internal_block = 16;
}

typedef struct _grainflow_tilde
{
	t_object x_obj;
	t_inlet* main_inlet;
	std::array<t_inlet*, 3> inlets{};
	std::array<t_outlet*, 8> outlets{};
	t_int n_grains = 1;
	std::unique_ptr<Grainflow::gf_grain_collection<t_garray, internal_block, t_sample>> grain_collection;
	Grainflow::gf_i_buffer_reader<t_garray, t_sample> buffer_reader;
	std::array<std::vector<t_sample>, 4> input_channels;
	std::array<std::vector<t_sample*>, 4> input_channel_ptrs;
	std::array<std::vector<t_sample*>, 8> output_channel_ptrs;
	std::string default_buffer_name;

	bool state = false;
} t_grainflow_tilde;


void* grainflow_tilde_new(t_symbol* s, int ac, t_atom* av)
{
	s = nullptr;
	t_grainflow_tilde* x = reinterpret_cast<t_grainflow_tilde*>(pd_new(grainflow_tile_class));

	for (auto& inlet : x->inlets)
	{
		inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
	}
	for (auto& outlet : x->outlets)
	{
		outlet = outlet_new(&x->x_obj, &s_signal);
	}
	if (ac <= 0)return (void*)x;
	x->default_buffer_name = std::string(av[0].a_w.w_symbol->s_name);

	if (ac <= 1) return (void*)x;
	x->n_grains = static_cast<int>(av[1].a_w.w_float);

	x->buffer_reader = Grainflow::pd_buffer_reader::get_buffer_reader();
	x->grain_collection = std::make_unique<Grainflow::gf_grain_collection<t_garray, internal_block, t_sample>>(
		x->buffer_reader, x->n_grains);
	auto buffer_ref = reinterpret_cast<struct _garray*>(pd_findbyclass(gensym(x->default_buffer_name.c_str()),
	                                                                   garray_class));
	for (int i = 0; i < x->grain_collection->grains(); ++i)
	{
		x->grain_collection->get_grain(i)->set_buffer(Grainflow::gf_buffers::buffer, buffer_ref);
	}

	return (void*)x;
}

void grainflow_init(t_grainflow_tilde* x, t_signal** inputs)
{
	for (int i = 0; i < x->input_channels.size(); ++i)
	{
		x->input_channels[i].resize(inputs[i]->s_length * inputs[i]->s_nchans);
	}
}

void grainflow_setup_inputs(t_grainflow_tilde* x, Grainflow::gf_io_config<t_sample>& config, t_signal** inputs)
{
	config.grain_clock_chans = inputs[0]->s_nchans;
	config.traversal_phasor_chans = inputs[1]->s_nchans;
	config.fm_chans = inputs[2]->s_nchans;
	config.am_chans = inputs[3]->s_nchans;

	for (int i = 0; i < x->input_channels.size(); ++i)
	{
		std::copy_n(inputs[i]->s_vec, x->input_channels[i].size(), x->input_channels[i].begin());
	}

	for (int i = 0; i < x->input_channel_ptrs.size(); ++i)
	{
		x->input_channel_ptrs[i].resize(inputs[i]->s_nchans);
		const int chan_size = inputs[i]->s_length;
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

void grainflow_setup_outputs(t_grainflow_tilde* x, Grainflow::gf_io_config<t_sample>& config, t_signal** outputs)
{
	for (int i = 0; i < x->output_channel_ptrs.size(); ++i)
	{
		x->output_channel_ptrs[i].resize(x->n_grains);
		const int chan_size = outputs[i]->s_length;
		for (int j = 0; j < x->n_grains; ++j)
		{
			x->output_channel_ptrs[i][j] = &(outputs[i]->s_vec[j * chan_size]);
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
	auto inputs = reinterpret_cast<t_signal**>(w[2]);
	auto outputs = reinterpret_cast<t_signal**>(w[3]);

	Grainflow::gf_io_config<t_sample> config;
	config.block_size = inputs[0]->s_length;
	config.samplerate = inputs[0]->s_sr;
	config.livemode = true; //We do not know the buffer samplerate
	grainflow_setup_inputs(x, config, inputs);
	grainflow_setup_outputs(x, config, outputs);
	if (!x->state) return w + 4;
	x->grain_collection->process(config);


	return w + 4;
}

void grainflow_tilde_float(t_grainflow_tilde* x, t_floatarg f)
{
	x->state = f >= 1;
}

void grainflow_tilde_anything(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av)
{
	if (ac < 1) return;
	auto success = Grainflow::GF_RETURN_CODE::GF_ERR;
	if (ac < 2)
	{
		success = x->grain_collection->param_set(0, std::string(s->s_name), av[0].a_w.w_float);
	}
	else
	{
		for (int i = 0; i < ac; ++i)
		{
			success = x->grain_collection->param_set((i % (x->grain_collection->grains()) + 1), std::string(s->s_name),
			                                         av[i].a_w.w_float);
		}
	}
	if (success != Grainflow::GF_RETURN_CODE::GF_SUCCESS)
	{
		auto message = std::format("grainflow: {} is not a valid parameter name", s->s_name);
		pd_error(x, message.data());
	}
}

void grainflow_tilde_free(t_grainflow_tilde* x)
{
	for (auto& inlet : x->inlets)
	{
		inlet_free(inlet);
	}
	for (auto& outlet : x->outlets)
	{
		outlet_free(outlet);
	}
}

static void grainflow_tile_dsp(t_grainflow_tilde* x, t_signal** sp)
{
	x->n_grains = std::max<t_int>(x->n_grains, 1);
	grainflow_init(x, sp);
	for (int i = 0; i < x->outlets.size(); ++i)
	{
		signal_setmultiout(&sp[x->inlets.size() + 1 + i], x->n_grains); //Main inlet + additional ones
	}
	dsp_add(grainflow_tilde_perform, 3, x, &sp[0], &sp[x->inlets.size() + 1]);
}

void grainflow_tilde_setup(void)
{
	grainflow_tile_class = class_new(gensym("grainflow~"),
	                                 reinterpret_cast<t_newmethod>(grainflow_tilde_new),
	                                 0, sizeof(t_grainflow_tilde),
	                                 CLASS_MULTICHANNEL, A_GIMME, 0);
	class_addmethod(grainflow_tile_class,
	                reinterpret_cast<t_method>(grainflow_tile_dsp), gensym("dsp"), A_CANT, 0);
	CLASS_MAINSIGNALIN(grainflow_tile_class, t_grainflow_tilde, main_inlet);
	class_addfloat(grainflow_tile_class,
	               (t_method)grainflow_tilde_float);
	class_addmethod(grainflow_tile_class,
	                (t_method)grainflow_tilde_anything, gensym("anything"),
	                A_GIMME, 0);
}
