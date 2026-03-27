#ifndef __XR_ID_H__
#define __XR_ID_H__

/********SOC Short Code***********/
#define SOC_F1      0x01

/********Batch Number Code********/
#define ES_TYPE     0x00
#define CS_TYPE     0x01
#define EC_TYPE     0x02

// Franklin1 chipId
#define F1_ES_CHIPID  ((SOC_F1<<3) | ES_TYPE)
#define F1_CS_CHIPID  ((SOC_F1<<3) | CS_TYPE)
#define F1_EC_CHIPID  ((SOC_F1<<3) | EC_TYPE)

#endif
