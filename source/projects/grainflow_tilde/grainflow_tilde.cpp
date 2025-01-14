#include"grainflowBase.h"

namespace Grainflow
{
	namespace
	{
		static t_class* grainflow_tidle_class;
		constexpr int internal_block = 16;
	};

	class Grainflow_Tilde : public Grainflow_Base<Grainflow_Tilde, internal_block>
	{
		static void* grainflow_tilde_new(t_symbol* s, int ac, t_atom* av)
		{
			s = nullptr;
			auto x = reinterpret_cast<Grainflow_Tilde*>(pd_new(grainflow_tidle_class));
			return grainflow_create(x, ac, av);
		}

		static void grainflow_setup_inputs(Grainflow_Tilde* x, Grainflow::gf_io_config<t_sample>& config)
		{
			config.grain_clock_chans = x->inlet_data[0].nchans;
			config.traversal_phasor_chans = x->inlet_data[1].nchans;
			config.fm_chans = x->inlet_data[2].nchans;
			config.am_chans = x->inlet_data[3].nchans;

			for (int i = 0; i < x->inlet_data.size(); ++i)
			{
				const auto vec = x->inlet_data[i].vec;
				std::copy_n(vec, x->input_channel_counts[i] * config.block_size, x->input_channels[i]);
			}

			for (int i = 0; i < x->input_channel_ptrs.size(); ++i)
			{
				const int chan_size = x->blockSize;
				for (int j = 0; j < x->input_channel_counts[i]; ++j)
				{
					x->input_channel_ptrs[i][j] = &(x->input_channels[i][j * chan_size]);
				}
			}

			config.grain_clock = x->input_channel_ptrs[0];
			config.traversal_phasor = x->input_channel_ptrs[1];
			config.fm = x->input_channel_ptrs[2];
			config.am = x->input_channel_ptrs[3];
		}

		static void grainflow_setup_outputs(Grainflow_Tilde* x, Grainflow::gf_io_config<t_sample>& config)
		{
			for (int i = 0; i < x->output_channel_ptrs.size(); ++i)
			{
				const int chan_size = x->blockSize;
				for (int j = 0; j < x->max_grains; ++j)
				{
					x->output_channel_ptrs[i][j] = &(x->outlet_data[i].vec[j * chan_size]);
					std::fill_n(x->output_channel_ptrs[i][j], chan_size, 0.0f);
				}
			}

			config.grain_output = x->output_channel_ptrs[0];
			config.grain_state = x->output_channel_ptrs[1];
			config.grain_progress = x->output_channel_ptrs[2];
			config.grain_playhead = x->output_channel_ptrs[3];
			config.grain_amp = x->output_channel_ptrs[4];
			config.grain_envelope = x->output_channel_ptrs[5];
			config.grain_buffer_channel = x->output_channel_ptrs[6];;
			config.grain_stream_channel = x->output_channel_ptrs[7];
		}

		static t_int* grainflow_tilde_perform(t_int* w)
		{
			auto x = reinterpret_cast<Grainflow_Tilde*>(w[1]);
			Grainflow::gf_io_config<t_sample> config;
			config.block_size = x->blockSize;
			config.samplerate = x->samplerate;
			config.livemode = true; //We do not know the buffer samplerate
			grainflow_setup_inputs(x, config);
			grainflow_setup_outputs(x, config);
			if (!x->state) return w + 2;
			x->grain_collection->process(config);
			if (x->data_update) { return w + 2; }
			collect_grain_info(x, &x->grain_data, &config, x->grain_collection->active_grains());
			x->data_update = true;
			return w + 2;
		}

		static void grainflow_tilde_dsp(Grainflow_Tilde* x, t_signal** sp)
		{
			x->max_grains = std::max<t_int>(x->max_grains, 1);

			x->grain_data.resize(
				(x->grain_collection != nullptr) ? x->grain_collection->active_grains() : x->max_grains);
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

	public:
		static void register_object()
		{
			grainflow_tidle_class = class_new(gensym("grainflow~"),
			                                  reinterpret_cast<t_newmethod>(grainflow_tilde_new),
			                                  reinterpret_cast<t_method>(grainflow_free), sizeof(Grainflow_Tilde),
			                                  CLASS_MULTICHANNEL, A_GIMME, 0);
			class_addmethod(grainflow_tidle_class,
			                reinterpret_cast<t_method>(grainflow_tilde_dsp), gensym("dsp"), A_CANT, 0);
			CLASS_MAINSIGNALIN(grainflow_tidle_class, Grainflow_Tilde, f);
			class_addfloat(grainflow_tidle_class, reinterpret_cast<t_method>(message_float));
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(message_float), gensym("state"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(param_message), gensym("g"),
			                A_GIMME, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(set_ngrains), gensym("ngrains"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(set_auto_overlap), gensym("autoOverlap"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(param_deviate), gensym("deviate"),
			                A_GIMME, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(param_spread), gensym("spread"),
			                A_GIMME, 0);

			//Streams
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(stream_set), gensym("streamSet"),
			                A_GIMME, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(stream_param_set), gensym("stream"),
			                A_GIMME, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(stream_param_deviate),
			                gensym("streamDeviate"),
			                A_GIMME, 0);
			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(stream_param_spread),
			                gensym("streamSpread"),
			                A_GIMME, 0);

			class_addmethod(grainflow_tidle_class, reinterpret_cast<t_method>(message_anything), gensym("anything"),
			                A_GIMME, 0);
		}
	};
}

extern "C" void grainflow_tilde_setup(void)
{
	Grainflow::Grainflow_Tilde::register_object();
}
