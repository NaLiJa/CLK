//
//  CompoundSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "BufferSource.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <atomic>

namespace Outputs::Speaker {

/// @returns @c true if any of the templated sources is stereo; @c false otherwise.
template <typename... S> constexpr bool is_stereo() {
	bool is_stereo = false;
	([&] {
		is_stereo |= S::is_stereo;
	}(), ...);
	return is_stereo;
}

/// @returns @c true if the variadic template arguments are ordered as all stereo sources followed by
/// all mono; @c false otherwise.
template <typename... S> constexpr bool are_properly_ordered() {
	bool is_ordered = true;
	bool is_stereo = true;
	([&] {
		if(S::is_stereo && !is_stereo) {
			is_ordered = false;
		}
		is_stereo &= S::is_stereo;
	}(), ...);
	return is_ordered;
}

/*!
	A CompoundSource adds together the sound generated by multiple individual SampleSources.
	An owner may optionally assign relative volumes.
*/
template <typename... T> class CompoundSource:
	public Outputs::Speaker::BufferSource<CompoundSource<T...>, ::Outputs::Speaker::is_stereo<T...>()> {
	private:
		template <typename... S> class CompoundSourceHolder {
			public:
				template <bool output_stereo>
				void get_samples(std::size_t number_of_samples, typename SampleT<output_stereo>::type *target) {
					// Default-construct all samples, to fill with silence.
					std::fill(target, target + number_of_samples, typename SampleT<output_stereo>::type());
				}

				void set_scaled_volume_range(int16_t, double *, double) {}
				void skip_samples(const std::size_t) {}

				static constexpr std::size_t size() {
					return 0;
				}

				static constexpr bool is_stereo = false;

				double total_scale(double *) const {
					return 0.0;
				}
		};

		template <typename S, typename... R> class CompoundSourceHolder<S, R...> {
			public:
				CompoundSourceHolder(S &source, R &...next) : source_(source), next_source_(next...) {}

				static constexpr bool is_stereo = S::is_stereo || CompoundSourceHolder<R...>::is_stereo;

				template <bool output_stereo>
				void get_samples(std::size_t number_of_samples, typename ::Outputs::Speaker::SampleT<output_stereo>::type *target) {

					// If this is the step at which a mono-to-stereo adaptation happens, apply it.
					if constexpr (output_stereo && !S::is_stereo) {
						// There'll be only one place in the chain that this conversion happens, but it'll
						// happen there often. So avoid continuously reallocating.
						if(conversion_source_.size() < number_of_samples) {
							conversion_source_.resize(number_of_samples);
						}

						// Populate the conversion buffer with this source and all below.
						get_samples<false>(number_of_samples, conversion_source_.data());

						// Map up and return.
						for(std::size_t c = 0; c < number_of_samples; c++) {
							target[c].left = target[c].right = conversion_source_[c];
						}
						return;
					}

					// Get the rest of the output.
					next_source_.template get_samples<output_stereo>(number_of_samples, target);

					if(source_.is_zero_level()) {
						// This component is currently outputting silence; therefore don't add anything to the output
						// audio — just pass the call onward.
						source_.skip_samples(number_of_samples);
						return;
					}

					// Get this component's output.
					typename SampleT<output_stereo>::type local_samples[number_of_samples];
					source_.get_samples(number_of_samples, local_samples);

					// Merge it in.
					while(number_of_samples--) {
						target[number_of_samples] += local_samples[number_of_samples];
					}

					// TODO: accelerate above?
				}

				void skip_samples(const std::size_t number_of_samples) {
					source_.skip_samples(number_of_samples);
					next_source_.skip_samples(number_of_samples);
				}

				void set_scaled_volume_range(int16_t range, double *volumes, double scale) {
					const auto scaled_range = volumes[0] / double(source_.average_output_peak()) * double(range) / scale;
					source_.set_sample_volume_range(int16_t(scaled_range));
					next_source_.set_scaled_volume_range(range, &volumes[1], scale);
				}

				static constexpr std::size_t size() {
					return 1 + CompoundSourceHolder<R...>::size();
				}

				double total_scale(double *volumes) const {
					return (volumes[0] / source_.average_output_peak()) + next_source_.total_scale(&volumes[1]);
				}

			private:
				S &source_;
				CompoundSourceHolder<R...> next_source_;
				std::vector<MonoSample> conversion_source_;
		};

	public:
		using Sample = typename SampleT<::Outputs::Speaker::is_stereo<T...>()>::type;

		// To ensure at most one mono to stereo conversion, require appropriate source ordering.
		static_assert(are_properly_ordered<T...>(), "Sources should be listed with all stereo sources before all mono sources");

		CompoundSource(T &... sources) : source_holder_(sources...) {
			// Default: give all sources equal volume.
			const auto volume = 1.0 / double(source_holder_.size());
			for(std::size_t c = 0; c < source_holder_.size(); ++c) {
				volumes_.push_back(volume);
			}
		}

		void get_samples(std::size_t number_of_samples, Sample *target) {
			source_holder_.template get_samples<::Outputs::Speaker::is_stereo<T...>()>(number_of_samples, target);
		}

		void skip_samples(const std::size_t number_of_samples) {
			source_holder_.skip_samples(number_of_samples);
		}

		/*!
			Sets the total output volume of this CompoundSource.
		*/
		void set_sample_volume_range(int16_t range) {
			volume_range_ = range;
			push_volumes();
		}

		/*!
			Sets the relative volumes of the various sources underlying this
			compound. The caller should ensure that the number of items supplied
			matches the number of sources and that the values in it sum to 1.0.
		*/
		void set_relative_volumes(const std::vector<double> &volumes) {
			assert(volumes.size() == source_holder_.size());
			volumes_ = volumes;
			push_volumes();
			average_output_peak_ = 1.0 / source_holder_.total_scale(volumes_.data());
		}

		/*!
			@returns the average output peak given the sources owned by this CompoundSource and the
				current relative volumes.
		*/
		double average_output_peak() const {
			return average_output_peak_;
		}

	private:
		void push_volumes() {
			const double scale = source_holder_.total_scale(volumes_.data());
			source_holder_.set_scaled_volume_range(volume_range_, volumes_.data(), scale);
		}

		CompoundSourceHolder<T...> source_holder_;
		std::vector<double> volumes_;
		int16_t volume_range_ = 0;
		std::atomic<double> average_output_peak_{1.0};
};

}
