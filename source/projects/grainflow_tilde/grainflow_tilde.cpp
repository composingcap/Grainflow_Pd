#include <m_pd.h>
#include <algorithm>
#include <vector>
#include <array>
#include "PdBufferReader.h"
#include <gfGrainCollection.h>
#include <cstring>

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


typedef struct _grainflow_tilde
{
	t_object x_obj;
	t_inlet* main_inlet;
	t_float f;
	std::array<t_inlet*, 3> inlets{};
	std::array<t_outlet*, 8> outlets{};
	t_int n_grains = 1;
	std::unique_ptr<Grainflow::gf_grain_collection<Grainflow::pd_buffer, internal_block, t_sample>> grain_collection;
	Grainflow::gf_i_buffer_reader<Grainflow::pd_buffer, t_sample> buffer_reader;
	std::array<std::vector<t_sample>, 4> input_channels;
	std::array<std::vector<t_sample*>, 4> input_channel_ptrs;
	std::array<std::vector<t_sample*>, 8> output_channel_ptrs;
	int blockSize = 0;
	int samplerate = 0;
	std::array<iolet, 4> inlet_data;
	std::array<iolet, 8> outlet_data;

	bool state = false;
} t_grainflow_tilde;


void* grainflow_tilde_new(t_symbol* s, int ac, t_atom* av)
{
	s = nullptr;
	t_grainflow_tilde* x = reinterpret_cast<t_grainflow_tilde*>(pd_new(grainflow_tidle_class));

	for (auto& inlet : x->inlets)
	{
		inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
	}
	for (auto& outlet : x->outlets)
	{
		outlet = outlet_new(&x->x_obj, &s_signal);
	}
	if (ac < 2){
		pd_error("grainflow~: must be instantiated with two arguments- buffer-name ngrains", "");
		return (void*)x;
	}
	x->n_grains = static_cast<int>(av[1].a_w.w_float);
	t_symbol* envName = gensym("default");
	if (ac > 2){
		envName= av[2].a_w.w_symbol;
	}


	x->buffer_reader = Grainflow::pd_buffer_reader::get_buffer_reader();
	x->grain_collection = std::make_unique<Grainflow::gf_grain_collection<
		Grainflow::pd_buffer, internal_block, t_sample>>(
		x->buffer_reader, x->n_grains);

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
		x->output_channel_ptrs[i].resize(x->n_grains);
		const int chan_size = x->blockSize;
		for (int j = 0; j < x->n_grains; ++j)
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
				}
			}
			else
			{
				auto arg_counter = 0;
				for (int i = 0; i < x->grain_collection->grains(); ++i)
				{
					auto tries = 0;
					while (av[arg_counter % (ac - start) + start].a_type!= t_atomtype::A_SYMBOL){
						if (tries > 3) return;
						arg_counter++;
						tries++;
					};
					x->grain_collection->get_buffer(type, i)->set(av[arg_counter % (ac - start) + start].a_w.w_symbol);
					arg_counter++;
					if (type == Grainflow::gf_buffers::envelope){
						if (av[arg_counter % (ac - start) + start].a_type== t_atomtype::A_FLOAT){
							x->grain_collection->param_set(i+1,Grainflow::gf_param_name::n_envelopes, Grainflow::gf_param_type::value, static_cast<float>(av[arg_counter % (ac - start) + start].a_w.w_float));
							arg_counter++;
						}
						else{
							x->grain_collection->param_set(i+1,Grainflow::gf_param_name::n_envelopes, Grainflow::gf_param_type::value, 1);
						}
					}
				}
			}
			return;
		}
		pd_error(x, "grainflow: %s is not a valid parameter name", s->s_name);
	}
}

static void grainflow_tilde_dsp(t_grainflow_tilde* x, t_signal** sp)
{
	x->n_grains = std::max<t_int>(x->n_grains, 1);
	grainflow_init(x, sp);
	for (int i = 0; i < x->outlets.size(); ++i)
	{
		signal_setmultiout(&sp[x->inlets.size() + 1 + i], x->n_grains); //Main inlet + additional ones
		x->outlet_data[i].nchans = x->n_grains;
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

void grainflow_stream_set(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av){
	if (ac < 1) {return;}
	
	if (ac< 2){
		if (av[0].a_type != A_FLOAT) {return;}
		x->grain_collection->stream_set(Grainflow::gf_stream_set_type::automatic_streams, av[1].a_w.w_float);
	return;
	}
	if(av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT) {return;}
	auto typeSym = av[0].a_w.w_symbol;
	Grainflow::gf_stream_set_type type;
	if (strcmp(typeSym->s_name, "auto") == 0) {type = Grainflow::gf_stream_set_type::automatic_streams;}
	else if (strcmp(typeSym->s_name, "per") == 0) {type = Grainflow::gf_stream_set_type::per_streams;}
	else if (strcmp(typeSym->s_name, "random") == 0) {type = Grainflow::gf_stream_set_type::random_streams;}
	x->grain_collection->stream_set(type, av[1].a_w.w_float);
}
void grainflow_stream_param_set(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av){
	if (ac < 3) {return;}
	if (av[0].a_type != A_FLOAT || av[1].a_type != A_SYMBOL) {return;}
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;

	if (Grainflow::param_reflection(av[1].a_w.w_symbol->s_name, param, type)){
		if (av[2].a_type != A_FLOAT) {return;}
		x->grain_collection->stream_param_set(av[0].a_w.w_float,param, type, av[2].a_w.w_float);
		return;
	}
}
void grainflow_stream_param_deviate(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av){
	if (ac < 3) {return;}
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) {return;}
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type)){
		x->grain_collection->stream_param_func(param,type, &Grainflow::gf_utils::deviate, av[1].a_w.w_float, av[2].a_w.w_float);
		return;
	}
}
void grainflow_stream_param_spread(t_grainflow_tilde* x, t_symbol* s, int ac, t_atom* av){
	if (ac < 3) {return;}
	if (av[0].a_type != A_SYMBOL || av[1].a_type != A_FLOAT || av[2].a_type != A_FLOAT) {return;}
	Grainflow::gf_param_name param;
	Grainflow::gf_param_type type;
	if (Grainflow::param_reflection(av[0].a_w.w_symbol->s_name, param, type)){
		x->grain_collection->stream_param_func(param,type, &Grainflow::gf_utils::lerp, av[1].a_w.w_float, av[2].a_w.w_float);
		return;
	}
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
	
	// class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(), gensym("g"),
	//                 A_GIMME, 0);
	// class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(), gensym("deviate"),
	//                 A_GIMME, 0);
	// class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(), gensym("spread"),
	//                 A_GIMME, 0);
	
	//Streams
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_set), gensym("streamSet"),
	                A_SYMBOL, A_FLOAT, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_set), gensym("stream"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_deviate), gensym("streamDeviate"),
	                A_GIMME, 0);
	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_stream_param_spread), gensym("streamSpread"),
	                A_GIMME, 0);

	class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(grainflow_tilde_anything), gensym("anything"),
	                A_GIMME, 0);
}
