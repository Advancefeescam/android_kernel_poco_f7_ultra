#ifndef __HQSYS_PCBA_
#define __HQSYS_PCBA_

typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_P6_P0_1_GL,

	PCBA_P6_P1_GL,

	PCBA_P6_P1_1_GL,

	PCBA_P6_P2_GL,

	PCBA_P6_MP_GL,

	PCBA_INFO_END,
} PCBA_INFO;

typedef enum
{
	STAGE_UNKNOW = 0,
	P0_1,
	P1,
	P1_1,
	P2,
	MP,
} PROJECT_STAGE;

struct project_stage {
	int voltage_min;
	int voltage_max;
	PROJECT_STAGE project_stage;
	char hwc_level[20];
} stage_map[] = {
	{ 130,  225,   P0_1,   "P0.1",},
	{ 226,  315,   P1,     "P1",  },
	{ 316,  405,   P1_1,   "P1.1",},
	{ 406,  485,   P2,     "P2",  },
	{ 486,  565,   MP,     "MP",  },
};

struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
} pcba_map[] = {

	{PCBA_P6_P0_1_GL,          "PCBA_P6_P0-1_GL"},

	{PCBA_P6_P1_GL,            "PCBA_P6_P1_GL"},

	{PCBA_P6_P1_1_GL,          "PCBA_P6_P1-1_GL"},

	{PCBA_P6_P2_GL,            "PCBA_P6_P2_GL"},

	{PCBA_P6_MP_GL,            "PCBA_P6_MP_GL"},
};

struct PCBA_MSG {
	PCBA_INFO huaqin_pcba_config;
	PROJECT_STAGE pcba_stage;
	unsigned int pcba_config;
	unsigned int pcba_config_count;
	const char *rsc;
	const char *sku;
};

struct PCBA_MSG* get_pcba_msg(void);

#endif
