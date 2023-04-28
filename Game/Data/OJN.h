#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "bms_struct.hpp"
#include <map>
#include "OJM.hpp"

union Event {
	float BPM;
	struct {
		uint16_t Value;
		uint8_t VolPan;
		uint8_t Type;
	};
};

struct Package {
	uint32_t Measure;
	uint16_t Channel;
	uint16_t EventCount;

	std::vector<Event> Events;
};

struct BPMChange {
	float BPM;
	double Measure;
	float TimeSignature;
	float Position;
};

struct NoteEvent {
	uint32_t Channel;

	uint16_t Value;
	float Pan, Vol;
	float Position;
	float PositionEnd;

	uint8_t Type;

	double Measure;
	double MeasureEnd;
};

struct Header {
	int songid;
	char signature[4];
	float encode_version;
	int genre;
	float bpm;
	short level[4];
	int event_count[3];
	int note_count[3];
	int measure_count[3];
	int package_count[3];
	short old_encode_version;
	short old_songid;
	char old_genre[20];
	int bmp_size;
	int old_file_version;
	char title[64];
	char artist[32];
	char noter[32];
	char ojm_file[32];
	int cover_size;
	int time[3];
	int data_offset[4];
};

struct O2Timing {
	double MesStart, MesEnd;
	double MsMarking;
	double MsPerMark;
};

struct O2Note {
	int32_t StartTime;
	int32_t EndTime;

	bool IsLN;
	int LaneIndex;
	int SampleRefId;
};

struct OJNDifficulty {
	std::vector<O2Note> Notes;
	std::vector<O2Note> AutoSamples;
	std::vector<O2Timing> Timings;
	std::vector<O2Sample> Samples;
	std::vector<double> Measures;
};

namespace O2 {
	class OJN {
	public:
		OJN();
		~OJN();

		void Load(std::filesystem::path& filePath);

		std::filesystem::path CurrrentDir;
		Header Header;
		bool IsValid();

		std::map<int, OJNDifficulty> Difficulties = {};
		std::vector<char> BackgroundImage = {};
		std::vector<char> ThumbnailImage = {};
	private:
		bool m_valid = false;
	};
}