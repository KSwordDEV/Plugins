// EncoderDecoder.cpp
// Brains... Brains... Need brains!!!

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#ifdef ENCODER
extern "C" __declspec(dllexport)  int __stdcall encode(void* ft, char *input, int size, int randomnr, char **output, int *outputsize) {
	int i;
	char *o = *output;
	
	//encoder
	for (i = 0; i < size; i++) o[i] = input[i] + 1;
	
	return size;
}
#endif



#ifdef DECODER
extern "C" __declspec(dllexport) int __stdcall decode(void* ft, char *input, int size, int randomnr, char **output, int *outputsize) {
	int i;
	char *o = *output;
	
	//decoder
	for (i = 0; i < size; i++) o[i] = input[i] - 1; 
	

	return size;
}
#endif