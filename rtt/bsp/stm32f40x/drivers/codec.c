#include "stm32f4_discovery_audio_codec.h"

void EVAL_AUDIO_TransferComplete_CallBack(uint32_t pBuffer, uint32_t Size)
{
	// fill 2nd half buffer
}

void EVAL_AUDIO_HalfTransfer_CallBack(uint32_t pBuffer, uint32_t Size)
{
	// fill 1st half buffer
}

uint16_t EVAL_AUDIO_GetSampleCallBack(void)
{
	return (uint16_t)0;
}
