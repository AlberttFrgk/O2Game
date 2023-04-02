#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>

namespace BMS {
	struct BMSEvent;
}

namespace Calculation {
	double GetTrackDuration(double bpm, double measure);

	double GetStopDuration(double bpm, double duration);

	struct BPMInfo {
		int Measure;
		double Offset;
		double Value;
	};
	
	struct STOPInfo {
		int Measure;
		double Offset;
		double Value;
	};

	class Timing {
	public:
		Timing(std::vector<BMS::BMSEvent>& currentTrackEvent, std::unordered_map<std::string, double> vbpms, std::unordered_map<std::string, double> vstops);

		double GetStartTimeFromOffset(double bpm, double offset, bool inc = true);
		double GetStopTimeFromOffset(double bpm, double offset, bool inc = true);
		std::vector<std::pair<double, double>> GetTimings(double timePos, double BPM);

	private:
		double currentMeasure = 1.0;

		std::vector<BPMInfo> bpms;
		std::vector<STOPInfo> stops;
	};
}