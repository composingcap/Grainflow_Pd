#include"grainflowBase.h"
#include <gfRecord.h>

namespace Grainflow
{
	namespace
	{
		static t_class* grainflow_live_class;
		constexpr int internal_block = 16;
	};

	class Grainflow_Live_Tilde : public Grainflow_Base<Grainflow_Live_Tilde, internal_block>
	{
	public:
		std::unique_ptr<gfRecorder<pd_buffer, internal_block, t_sample>> recorder;
		int record_input_channels = 1;
		std::vector<t_sample*> record_input_ptr;
		t_sample* traversal_phasor;

		static void* grainflow_live_tilde_new(t_symbol* s, int ac, t_atom* av)
		{
			s = nullptr;
			auto x = reinterpret_cast<Grainflow_Live_Tilde*>(pd_new(grainflow_live_class));
			auto ptr = grainflow_create(x, ac, av);
			x->recorder = std::make_unique<gfRecorder<pd_buffer, internal_block, t_sample>>(x->buffer_reader);
			return ptr;
		}

		static void grainflow_live_free(Grainflow_Live_Tilde* x)
		{
			delete[] x->traversal_phasor;
			grainflow_free(x);
		}

		static void grainflow_live_setup_inputs(Grainflow_Live_Tilde* x, Grainflow::gf_io_config<t_sample>& config)
		{
			config.grain_clock_chans = x->inlet_data[1].nchans;
			config.traversal_phasor_chans = 1;
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
			x->record_input_ptr[0] = x->input_channel_ptrs[0][0];

			config.grain_clock = x->input_channel_ptrs[1].data();
			config.traversal_phasor = &x->traversal_phasor;
			config.fm = x->input_channel_ptrs[2].data();
			config.am = x->input_channel_ptrs[3].data();
		}

		static void grainflow_live_setup_outputs(Grainflow_Live_Tilde* x, Grainflow::gf_io_config<t_sample>& config)
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

		static t_int* grainflow_live_tilde_perform(t_int* w)
		{
			auto x = reinterpret_cast<Grainflow_Live_Tilde*>(w[1]);
			Grainflow::gf_io_config<t_sample> config;
			config.block_size = x->blockSize;
			config.samplerate = x->samplerate;


			grainflow_live_setup_inputs(x, config);
			grainflow_live_setup_outputs(x, config);

			x->recorder->state = x->state;
			x->recorder->process(x->record_input_ptr.data(), 0,
			                     x->grain_collection->get_buffer(Grainflow::gf_buffers::buffer, 0), config.block_size,
			                     1, config.traversal_phasor[0]);

			config.livemode = true; //We do not know the buffer samplerate

			if (!x->state) return w + 2;
			x->grain_collection->process(config);
			if (x->data_update) { return w + 2; }
			collect_grain_info(&x->grain_data, &config, x->grain_collection->active_grains());
			x->data_update = true;
			return w + 2;
		}

		static void grainflow_live_tilde_dsp(Grainflow_Live_Tilde* x, t_signal** sp)
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
			x->record_input_ptr.resize(1);
			delete x->traversal_phasor;
			x->traversal_phasor = new t_sample[x->blockSize];
			dsp_add(grainflow_live_tilde_perform, 1, x);
		}

	public:
		static void register_object()
		{
			grainflow_live_class = class_new(gensym("grainflow.live~"),
			                                 reinterpret_cast<t_newmethod>(grainflow_live_tilde_new),
			                                 reinterpret_cast<t_method>(grainflow_live_free),
			                                 sizeof(Grainflow_Live_Tilde),
			                                 CLASS_MULTICHANNEL, A_GIMME, 0);
			class_addmethod(grainflow_live_class,
			                reinterpret_cast<t_method>(grainflow_live_tilde_dsp), gensym("dsp"), A_CANT, 0);
			CLASS_MAINSIGNALIN(grainflow_live_class, Grainflow_Live_Tilde, f);
			class_addfloat(grainflow_live_class, reinterpret_cast<t_method>(message_float));
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(message_float), gensym("state"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(param_message), gensym("g"),
			                A_GIMME, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(set_ngrains), gensym("ngrains"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(set_auto_overlap), gensym("autoOverlap"),
			                A_DEFFLOAT, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(param_deviate), gensym("deviate"),
			                A_GIMME, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(param_spread), gensym("spread"),
			                A_GIMME, 0);

			//Streams
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(stream_set), gensym("streamSet"),
			                A_GIMME, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(stream_param_set), gensym("stream"),
			                A_GIMME, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(stream_param_deviate),
			                gensym("streamDeviate"),
			                A_GIMME, 0);
			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(stream_param_spread),
			                gensym("streamSpread"),
			                A_GIMME, 0);

			class_addmethod(grainflow_live_class, reinterpret_cast<t_method>(message_anything), gensym("anything"),
			                A_GIMME, 0);
		}
	};
}

extern "C" void setup_grainflow0x2elive_tilde(void)
{
	Grainflow::Grainflow_Live_Tilde::register_object();
}