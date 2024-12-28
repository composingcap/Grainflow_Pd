#pragma once
#include <algorithm>
#include <m_pd.h>
#include "gfGrain.h"
#include "gfIBufferReader.h"
#include "gfEnvelopes.h"

///Provides an interface to grainflow using Max Min API

namespace Grainflow
{
	class pd_buffer_reader
	{
	public:
		static bool update_buffer_info(t_garray* buffer, const gf_io_config<t_sample>& io_config,
		                               gf_buffer_info* buffer_info)
		{
			if (buffer == nullptr) return false;
			int size{0};
			t_word* vec;
			if (!garray_getfloatwords(buffer, &size, &vec))
			{
				return false;
			}
			buffer_info->buffer_frames = size;
			buffer_info->sample_rate_adjustment = 1;
			buffer_info->samplerate = 48000;
			buffer_info->n_channels = 1;
			return true;
		}

		static bool sample_param_buffer(t_garray* buffer, gf_param* param, const int grain_id)
		{
			return false;
		}

		static void sample_buffer(t_garray* buffer, const int channel, t_sample* __restrict samples,
		                          const t_sample* positions,
		                          const int size)
		{
			//garray_usedindsp(buffer);
			int bsize{0};
			t_word* vec;
			garray_getfloatwords(buffer, &bsize, &vec);

			for (int i = 0; i < size; ++i)
			{
				samples[i] = vec[(int)(positions[i]) % bsize].w_float;
			}
		};

		static void write_buffer(t_garray* buffer, const int channel, const t_sample* samples,
		                         t_sample* __restrict scratch,
		                         const int start_position, const float overdub, const int size)
		{
		};


		static void sample_envelope(t_garray* buffer, const bool use_default, const int n_envelopes,
		                            const float env2d_pos, t_sample* __restrict samples,
		                            const t_sample* __restrict grain_clock, const int size)
		{
			for (int i = 0; i < size; ++i)
			{
				samples[i] = gf_envelopes::hanning_envelope[static_cast<int>(grain_clock[i] * 1024)];
			}
		};

		static gf_i_buffer_reader<t_garray, t_sample> get_buffer_reader()
		{
			gf_i_buffer_reader<t_garray, t_sample> _bufferReader;
			_bufferReader.sample_buffer = pd_buffer_reader::sample_buffer;
			_bufferReader.sample_envelope = pd_buffer_reader::sample_envelope;
			_bufferReader.update_buffer_info = pd_buffer_reader::update_buffer_info;
			_bufferReader.sample_param_buffer = pd_buffer_reader::sample_param_buffer;
			_bufferReader.write_buffer = pd_buffer_reader::write_buffer;
			return _bufferReader;
		}
	};
}
