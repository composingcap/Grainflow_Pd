#pragma once
#include <algorithm>
#include <cstring>
#include <m_pd.h>
#include "gfGrain.h"
#include "gfIBufferReader.h"
#include "gfEnvelopes.h"

namespace Grainflow
{

	class pd_buffer
	{
	public:
		t_symbol *name;
		t_gpointer ptr {};

		bool get(t_int* size, t_word** vec)
		{
			if (name == nullptr || strcmp(name->s_name, "") == 0) return false;
			t_garray* buffer_ref;
			if (!(buffer_ref = (t_garray*)pd_findbyclass(name, garray_class)))
			{
				return false;
			}
			int i_size;
			if (!garray_getfloatwords(buffer_ref, &i_size, vec))
				{
					return false;
				}
			*size = i_size;
			return true;
		}
		void set(t_symbol* name)
		{
			this->name = name;
		}

		pd_buffer(t_symbol* name = nullptr)
		{
			gpointer_unset(&ptr);
			this->name = name;
		}
		~pd_buffer()
		{
			gpointer_unset(& ptr);
		}
	};

	class pd_buffer_reader
	{
	public:
		static bool update_buffer_info(pd_buffer* buffer, const gf_io_config<t_sample>& io_config,
		                               gf_buffer_info* buffer_info)
		{
			if (buffer == nullptr) return false;
			t_int size{0};
			t_word* vec;
			if (!buffer->get(&size, &vec)) return false;
			if (buffer_info == nullptr) return true;
					
			buffer_info->buffer_frames = size;
			buffer_info->sample_rate_adjustment = 1;
			buffer_info->samplerate = io_config.samplerate;
			buffer_info->n_channels = 1;
			return true;
		}

		static bool sample_param_buffer(pd_buffer* buffer, gf_param* param, const int grain_id)
		{
			std::random_device rd;
			if (param->mode == gf_buffer_mode::normal || buffer == nullptr)
			{
				return false;
			}
			
			t_int frame = 0;
			t_int frames = 0;
			t_word* vec;
			if(!buffer->get(&frames, &vec))return false;
			if (frames <= 0) return false;
			if (param->mode == gf_buffer_mode::buffer_sequence)
			{
				frame = grain_id % frames;
			}
			else if (param->mode == gf_buffer_mode::buffer_random)
			{
				frame = (rd() % frames);
			}
			param->value = vec[frame].w_float + param->random * (rd() % 10000) * 0.0001 + param->offset *
				grain_id;
			return true;
		}

		static void sample_buffer(pd_buffer* buffer, const int channel, t_sample* __restrict samples,
		                          const t_sample* positions,
		                          const int size)
		{
			t_int bsize{0};
			t_word* vec;
			if (!buffer->get(&bsize, &vec)) return;
			
			for (int i = 0; i < size; ++i)
			{
				auto mix = positions[i]- std::floor(positions[i]);
				samples[i] = vec[(int)(positions[i]) % bsize].w_float * (1-mix) + vec[(int)(positions[i]+1) % bsize].w_float * mix;
			}
		};

		static void write_buffer(pd_buffer* buffer, const int channel, const t_sample* samples,
		                         t_sample* __restrict scratch,
		                         const int start_position, const float overdub, const int size)
		{
			if (buffer == nullptr) return;
			t_int frames{ 0 };
			t_word* vec;
			if (!buffer->get(&frames, &vec)) return;

			auto is_segmented = (start_position + size) >= frames;
			auto use_overdub = overdub >= 0.0001f;
			int channels = 1;

			if (use_overdub)
			{
				if (!is_segmented)
				{
					for (int i = 0; i < size; i++)
					{
						scratch[i] = vec[(start_position + i) * channels + channel].w_float * overdub;
					}
				}
				else
				{
					for (int i = 0; i < size; i++)
					{
						scratch[i] = vec[(((start_position + i) % frames) * channels + channel)].w_float * overdub;
					}
				}
			}
			else
			{
				memset(vec, 0.0, sizeof(double) * size);
			}

			if (!is_segmented)
			{
				for (int i = 0; i < size; i++)
				{
					vec[(start_position + i) * channels + channel].w_float = samples[i] * (1 - overdub) + scratch[i];
				}
				return;
			}
			auto first_chunk = (start_position + size) - frames;
			for (int i = 0; i < size; i++)
			{
				vec[(((start_position + i) % frames) * channels + channel)].w_float = samples[i] * (1 - overdub) +
					scratch[i];
			}

		};


		static void sample_envelope(pd_buffer* buffer, const bool use_default, const int n_envelopes,
		                            const float env2d_pos, t_sample* __restrict samples,
		                            const t_sample* __restrict grain_clock, const int size)
		{
			if (use_default) {
				for (int i = 0; i < size; ++i)
				{
					samples[i] = gf_envelopes::hanning_envelope[static_cast<int>(grain_clock[i] * 1024)];
				}
				return;
			}
			if (n_envelopes < 2)
			{
				t_int bsize{ 0 };
				t_word* vec;
				if (!buffer->get(&bsize, &vec)) return;
				if (bsize == 0) return;
				for (int i = 0; i < size; ++i)
				{
					samples[i] = vec[(int)(grain_clock[i]* bsize)].w_float;
				}
				return;
			}
			t_int bsize{ 0 };
			t_word* vec;
			if (!buffer->get(&bsize, &vec)) return;
			if (bsize == 0) return;
			bsize /= n_envelopes;
			float mix = env2d_pos - std::floor(env2d_pos);
			int a = std::floor(env2d_pos);
			int b = (a + 1) % n_envelopes;
			for (int i = 0; i < size; ++i)
			{
				samples[i] = vec[(int)(grain_clock[i]* bsize + a * bsize)].w_float * (1-mix) + vec[(int)(grain_clock[i] * bsize + b * bsize)].w_float * (mix);
			}			
		};

		static gf_i_buffer_reader<pd_buffer, t_sample> get_buffer_reader()
		{
			gf_i_buffer_reader<pd_buffer, t_sample> _bufferReader;
			_bufferReader.sample_buffer = pd_buffer_reader::sample_buffer;
			_bufferReader.sample_envelope = pd_buffer_reader::sample_envelope;
			_bufferReader.update_buffer_info = pd_buffer_reader::update_buffer_info;
			_bufferReader.sample_param_buffer = pd_buffer_reader::sample_param_buffer;
			_bufferReader.write_buffer = pd_buffer_reader::write_buffer;
			return _bufferReader;
		}
	};
}
