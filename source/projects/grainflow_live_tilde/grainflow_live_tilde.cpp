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
		bool rec = true;
		bool play = true;

		static void on_data_loop(Grainflow_Live_Tilde* x)
		{
			if (x->recorder == nullptr) { return; }
			std::array<t_atom, 2> outputs;
			outputs[0].a_type = A_SYMBOL;
			outputs[0].a_w.w_symbol = gensym("recordHead");
			outputs[1].a_type = A_FLOAT;
			outputs[1].a_w.w_float = x->recorder->write_position_norm;
			outlet_list(x->info_outlet, &s_list, outputs.size(), outputs.data());

			outputs[0].a_type = A_SYMBOL;
			outputs[0].a_w.w_symbol = gensym("recordHeadSamps");
			outputs[1].a_type = A_FLOAT;
			outputs[1].a_w.w_float = x->recorder->write_position_samps;
			outlet_list(x->info_outlet, &s_list, outputs.size(), outputs.data());
		}

		static void grainflow_live_free(Grainflow_Live_Tilde* x)
		{
			delete[] x->traversal_phasor;
			grainflow_free(x);
		}

		static void message_overdub(Grainflow_Live_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->recorder->overdub = std::clamp(av[0].a_w.w_float, 0.0f, 1.0f);
		}

		static void message_freeze(Grainflow_Live_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->recorder->freeze = av[0].a_w.w_float >= 1;
		}

		static void message_record(Grainflow_Live_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->rec = av[0].a_w.w_float >= 1;
		}

		static void message_play(Grainflow_Live_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->play = av[0].a_w.w_float >= 1;
		}

		static void* grainflow_live_tilde_new(t_symbol* s, int ac, t_atom* av)
		{
			s = nullptr;
			auto x = reinterpret_cast<Grainflow_Live_Tilde*>(pd_new(grainflow_live_class));
			x->buffer_reader = pd_buffer_reader::get_buffer_reader();
			x->recorder = std::make_unique<gfRecorder<pd_buffer, internal_block, t_sample>>(x->buffer_reader);
			x->play = true;
			x->rec = true;
			x->additional_args.push_back({gensym("overdub"), &message_overdub});
			x->additional_args.push_back({gensym("rec"), &message_record});
			x->additional_args.push_back({gensym("freeze"), &message_freeze});
			x->additional_args.push_back({gensym("play"), &message_play});
			x->data_clock_ex.push_back(&on_data_loop);
			x->update_buffers_each_frame = true;
			auto ptr = grainflow_create(x, ac, av);

			return ptr;
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
			for (int i = 0; i < x->input_channel_counts[0]; ++i)
			{
				x->record_input_ptr[i] = x->input_channel_ptrs[0][i];
			}

			config.grain_clock = x->input_channel_ptrs[1];
			config.traversal_phasor = &x->traversal_phasor;
			config.fm = x->input_channel_ptrs[2];
			config.am = x->input_channel_ptrs[3];
		}

		static void grainflow_live_setup_outputs(Grainflow_Live_Tilde* x, Grainflow::gf_io_config<t_sample>& config)
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
			config.grain_buffer_channel = x->output_channel_ptrs[6];
			config.grain_stream_channel = x->output_channel_ptrs[7];
		}

		static t_int* grainflow_live_tilde_perform(t_int* w)
		{
			auto x = reinterpret_cast<Grainflow_Live_Tilde*>(w[1]);
			Grainflow::gf_io_config<t_sample> config;
			config.block_size = x->blockSize;
			config.samplerate = x->samplerate;


			grainflow_live_setup_inputs(x, config);
			grainflow_live_setup_outputs(x, config);

			x->recorder->state = x->state && x->rec;
			x->recorder->process(x->record_input_ptr.data(), 0,
			                     x->grain_collection->get_buffer(Grainflow::gf_buffers::buffer, 0), config.block_size,
			                     x->inlet_data[0].nchans, config.traversal_phasor[0]);

			config.livemode = true; //We do not know the buffer samplerate

			if (!x->state || !x->play) return w + 2;
			x->grain_collection->process(config);
			if (x->data_update) { return w + 2; }
			collect_grain_info(x, &x->grain_data, &config, x->grain_collection->active_grains());
			x->data_update = true;
			return w + 2;
		}

		static void grainflow_live_tilde_dsp(Grainflow_Live_Tilde* x, t_signal** sp)
		{
			x->max_grains = std::max<t_int>(x->max_grains, 1);
			if (x->grain_collection == nullptr) { return; }
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
			x->record_input_ptr.resize(x->inlet_data[0].nchans);
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
