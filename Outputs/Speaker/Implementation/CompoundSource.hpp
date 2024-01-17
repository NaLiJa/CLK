//
//  CompoundSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "SampleSource.hpp"

#include <cassert>
#include <cstring>
#include <atomic>

namespace Outputs::Speaker {

/*!
	A CompoundSource adds together the sound generated by multiple individual SampleSources.
	An owner may optionally assign relative volumes.
*/
template <typename... T> class CompoundSource:
	public Outputs::Speaker::SampleSource {
	public:
		CompoundSource(T &... sources) : source_holder_(sources...) {
			// Default: give all sources equal volume.
			const auto volume = 1.0 / double(source_holder_.size());
			for(std::size_t c = 0; c < source_holder_.size(); ++c) {
				volumes_.push_back(volume);
			}
		}

		void get_samples(std::size_t number_of_samples, std::int16_t *target) {
			source_holder_.template get_samples<get_is_stereo()>(number_of_samples, target);
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
			@returns true if any of the sources owned by this CompoundSource is stereo.
		*/
		static constexpr bool get_is_stereo() { return CompoundSourceHolder<T...>::get_is_stereo(); }

		/*!
			@returns the average output peak given the sources owned by this CompoundSource and the
				current relative volumes.
		*/
		double get_average_output_peak() const {
			return average_output_peak_;
		}

	private:
		void push_volumes() {
			const double scale = source_holder_.total_scale(volumes_.data());
			source_holder_.set_scaled_volume_range(volume_range_, volumes_.data(), scale);
		}

		template <typename... S> class CompoundSourceHolder: public Outputs::Speaker::SampleSource {
			public:
				template <bool output_stereo> void get_samples(std::size_t number_of_samples, std::int16_t *target) {
					std::memset(target, 0, sizeof(std::int16_t) * number_of_samples);
				}

				void set_scaled_volume_range(int16_t, double *, double) {}

				static constexpr std::size_t size() {
					return 0;
				}

				static constexpr bool get_is_stereo() {
					return false;
				}

				double total_scale(double *) const {
					return 0.0;
				}
		};

		template <typename S, typename... R> class CompoundSourceHolder<S, R...> {
			public:
				CompoundSourceHolder(S &source, R &...next) : source_(source), next_source_(next...) {}

				template <bool output_stereo> void get_samples(std::size_t number_of_samples, std::int16_t *target) {
					// Get the rest of the output.
					next_source_.template get_samples<output_stereo>(number_of_samples, target);

					if(source_.is_zero_level()) {
						// This component is currently outputting silence; therefore don't add anything to the output
						// audio — just pass the call onward.
						source_.skip_samples(number_of_samples);
						return;
					}

					// Get this component's output.
					auto buffer_size = number_of_samples * (output_stereo ? 2 : 1);
					int16_t local_samples[buffer_size];
					source_.get_samples(number_of_samples, local_samples);

					// Merge it in; furthermore if total output is stereo but this source isn't,
					// map it to stereo.
					if constexpr (output_stereo == S::get_is_stereo()) {
						while(buffer_size--) {
							target[buffer_size] += local_samples[buffer_size];
						}
					} else {
						// This will happen only if mapping from mono to stereo, never in the
						// other direction, because the compound source outputs stereo if any
						// subcomponent does. So it outputs mono only if no stereo devices are
						// in the mixing chain.
						while(buffer_size--) {
							target[buffer_size] += local_samples[buffer_size >> 1];
						}
					}

					// TODO: accelerate above?
				}

				void skip_samples(const std::size_t number_of_samples) {
					source_.skip_samples(number_of_samples);
					next_source_.skip_samples(number_of_samples);
				}

				void set_scaled_volume_range(int16_t range, double *volumes, double scale) {
					const auto scaled_range = volumes[0] / double(source_.get_average_output_peak()) * double(range) / scale;
					source_.set_sample_volume_range(int16_t(scaled_range));
					next_source_.set_scaled_volume_range(range, &volumes[1], scale);
				}

				static constexpr std::size_t size() {
					return 1 + CompoundSourceHolder<R...>::size();
				}

				static constexpr bool get_is_stereo() {
					return S::get_is_stereo() || CompoundSourceHolder<R...>::get_is_stereo();
				}

				double total_scale(double *volumes) const {
					return (volumes[0] / source_.get_average_output_peak()) + next_source_.total_scale(&volumes[1]);
				}

			private:
				S &source_;
				CompoundSourceHolder<R...> next_source_;
		};

		CompoundSourceHolder<T...> source_holder_;
		std::vector<double> volumes_;
		int16_t volume_range_ = 0;
		std::atomic<double> average_output_peak_{1.0};
};

}
