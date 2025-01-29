#pragma once
#include <m_pd.h>
#include <algorithm>
#include <vector>
#include <array>
#include "PdBufferReader.h"
#include <gfGrainCollection.h>
#include <cstring>
#include <gfRecord.h>

namespace Grainflow
{
	namespace
	{
		static t_class* grainflow_record_class;
		constexpr int internal_block = 16;
		struct iolet
		{
			t_sample* vec;
			t_int nchans;
		};
	};

	class Grainflow_Record_Tilde
	{
	public:
		t_object x_obj;
		gf_i_buffer_reader<pd_buffer, t_sample> buffer_reader;
		std::unique_ptr<gfRecorder<pd_buffer, internal_block, t_sample>> recorder{nullptr};
		int record_input_channels = 1;
		std::vector<t_sample*> record_input_ptr;
		t_sample* traversal_phasor;
		pd_buffer record_buffer;
		t_int blockSize = 0;
		t_int samplerate = 0;
		bool data_update;
		iolet sample_inlet;
		iolet traversal_outlet;
		t_inlet* in_1;
		t_outlet* out_1;
		t_outlet* out_2;
		t_float f;


		static void grainflow_record_free(Grainflow_Record_Tilde* x)
		{
			delete[] x->traversal_phasor;
			x->~Grainflow_Record_Tilde();
		}

		static void message_buffer(Grainflow_Record_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			for (int i = 0; i < ac; ++i)
			{
				if (av[i].a_type != A_SYMBOL) { return; }
			}
			x->record_buffer.set(av[0].a_w.w_symbol);
			x->record_buffer.set_channels(av, ac);
		}

		static void message_overdub(Grainflow_Record_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->recorder->overdub = std::clamp(av[0].a_w.w_float, 0.0f, 1.0f);
		}

		static void message_freeze(Grainflow_Record_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->recorder->freeze = av[0].a_w.w_float >= 1;
		}

		static void message_record(Grainflow_Record_Tilde* x, t_symbol* s, int ac, t_atom* av)
		{
			if (ac < 1) { return; }
			if (av[0].a_type != A_FLOAT) { return; }
			x->recorder->state = av[0].a_w.w_float >= 1;
		}

		static void message_float(Grainflow_Record_Tilde* x, t_float value)
		{
			x->recorder->state = value >= 1;
		}

		static void* grainflow_record_tilde_new(t_symbol* s, int ac, t_atom* av)
		{
			s = nullptr;
			auto x = reinterpret_cast<Grainflow_Record_Tilde*>(pd_new(grainflow_record_class));
			x->buffer_reader = pd_buffer_reader::get_buffer_reader();
			x->recorder = std::make_unique<gfRecorder<pd_buffer, internal_block, t_sample>>(x->buffer_reader);
			x->out_1 = outlet_new(&x->x_obj, &s_signal);
			message_buffer(x, gensym("buf"), ac, av);

			return (void*)x;
		}


		static t_int* grainflow_record_tilde_perform(t_int* w)
		{
			auto x = reinterpret_cast<Grainflow_Record_Tilde*>(w[1]);

			for (int i = 0; i < x->sample_inlet.nchans; ++i)
			{
				x->record_input_ptr[i] = &x->sample_inlet.vec[x->blockSize * i];
			}

			x->recorder->process(x->record_input_ptr.data(), 0, &x->record_buffer
			                     , x->blockSize,
			                     x->sample_inlet.nchans, x->traversal_phasor);

			std::copy_n(x->traversal_phasor, x->blockSize, x->traversal_outlet.vec);
			x->data_update = true;
			return w + 2;
		}

		static void grainflow_record_tilde_dsp(Grainflow_Record_Tilde* x, t_signal** sp)
		{
			x->blockSize = sp[0]->s_length;
			x->samplerate = sp[0]->s_sr;
			x->sample_inlet.vec = sp[0]->s_vec;
			x->sample_inlet.nchans = sp[0]->s_nchans;
			x->record_input_ptr.resize(x->sample_inlet.nchans);
			signal_setmultiout(&sp[1], 1);
			x->traversal_outlet.vec = sp[1]->s_vec;
			delete x->traversal_phasor;
			x->traversal_phasor = new t_sample[x->blockSize];
			std::fill_n(x->traversal_phasor, x->blockSize, 0.0f);
			dsp_add(grainflow_record_tilde_perform, 1, x);
		}

	public:
		static void register_object()
		{
			grainflow_record_class = class_new(gensym("grainflow.record~"),
			                                   reinterpret_cast<t_newmethod>(grainflow_record_tilde_new),
			                                   reinterpret_cast<t_method>(grainflow_record_free),
			                                   sizeof(Grainflow_Record_Tilde),
			                                   CLASS_MULTICHANNEL, A_GIMME, 0);
			class_addmethod(grainflow_record_class,
			                reinterpret_cast<t_method>(grainflow_record_tilde_dsp), gensym("dsp"), A_CANT, 0);
			CLASS_MAINSIGNALIN(grainflow_record_class, Grainflow_Record_Tilde, f);
			class_addfloat(grainflow_record_class, reinterpret_cast<t_method>(message_float));
			class_addmethod(grainflow_record_class, reinterpret_cast<t_method>(message_record), gensym("rec"),
			                A_FLOAT, 0);
			class_addmethod(grainflow_record_class, reinterpret_cast<t_method>(message_freeze), gensym("freeze"),
			                A_FLOAT, 0);
			class_addmethod(grainflow_record_class, reinterpret_cast<t_method>(message_overdub), gensym("overdub"),
			                A_FLOAT, 0);
			class_addmethod(grainflow_record_class, reinterpret_cast<t_method>(message_buffer), gensym("buf"),
			                A_GIMME, 0);
		}
	};
}

extern "C" void setup_grainflow0x2erecord_tilde(void)
{
	Grainflow::Grainflow_Record_Tilde::register_object();
}
