#pragma once
#include "../pch.h"
#include <chrono>
#include <string>

namespace c {
	class chrono {
	public:
		using tchrono = typename std::chrono::steady_clock;
		using tclock = tchrono::time_point;
		tclock start_at, stop_at;
	public:
		/*
		* Set chrono start time point to current time
		* @return tclock
		*/
		forceinline tclock start() { return start_at = tchrono::now(); }
		/*
		* Get chrono time point
		* @return tclock
		*/
		forceinline tclock now() { return tchrono::now(); }
		/*
		* Set chrono stop time point to current time
		* @return tclock
		*/
		forceinline tclock stop() { return stop_at = tchrono::now(); }
		/*
		* Get duration of round (elapsed time between start and now). Set start at now time point
		* @return double in nanoseconds
		*/
		forceinline double round() {
			auto tm_elapsed = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(tchrono::now() - start_at).count();
			start_at = tchrono::now();
			return (double)tm_elapsed;
		}
		/*
		* Get elapsed time adjusted in time units
		* @return double elapsed time (`unit` contain the unit time)
		*/
		inline double elapse(double tm_elapsed, std::string& unit) {
			if (tm_elapsed < 1000.0F) {
				unit = "ns";
				return tm_elapsed;
			}
			else if (tm_elapsed /= 1000.0F; tm_elapsed < 1000.0F) {
				unit = "mks";
				return tm_elapsed;
			}
			else if (tm_elapsed /= 1000.0F; tm_elapsed < 1000.0F) {
				unit = "ms";
				return tm_elapsed;
			}
			tm_elapsed /= 1000.0F;
			unit = "sec";
			return tm_elapsed;
		}
		/*
		* Get elapsed time adjusted in time units
		* @return double elapsed time (`unit` contain the unit time)
		*/
		inline double elapse(tclock tm,std::string& unit) {
			return elapse((double)std::chrono::duration_cast<std::chrono::nanoseconds>(tm - start_at).count(), unit);
		}
		/*
		* Get elapsed time adjusted in time units
		* @return double elapsed time (`unit` contain the unit time)
		*/
		inline double duration(tclock from, tclock to, std::string& unit) {
			return elapse((double)std::chrono::duration_cast<std::chrono::nanoseconds>(to - from).count(), unit);
		}
		/*
		* Get average elapsed time adjusted in time units
		* @return double elapsed time (`unit` contain the unit time)
		*/
		inline double avg(double total_time,size_t num_rounds, std::string& unit) {
			return elapse(total_time / (double)(num_rounds), unit);
		}
	};
}