#include "kr.ttf.h"

static const unsigned char kr_ttf[] = {
	0
};

const unsigned char* get_kr_font_data() {
	return kr_ttf;
}

int get_kr_font_size() {
	return sizeof(kr_ttf);
}