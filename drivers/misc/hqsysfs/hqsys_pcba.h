#ifndef __HQSYS_PCBA_
#define __HQSYS_PCBA_
#if defined(PROJECT_ROCK)
typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_L19C_P0_1_IN = 1,
	PCBA_L19C_P0_1_GLOBAL,
	PCBA_L19A_P0_1_IN,

	PCBA_L19C_P1_IN = 4,
	PCBA_L19C_P1_GLOBAL,
	PCBA_L19A_P1_IN,

	PCBA_L19C_P1_1_IN = 7,
	PCBA_L19C_P1_1_GLOBAL,
	PCBA_L19A_P1_1_IN,

	PCBA_L19C_P2_IN = 10,
	PCBA_L19C_P2_GLOBAL,
	PCBA_L19A_P2_IN,

	PCBA_L19C_MP_IN = 13,
	PCBA_L19C_MP_GLOBAL,
	PCBA_L19A_MP_IN,

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

#elif defined(PROJECT_DIAMOND)
typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_M6_P0_1_GLOBAL = 1,
	PCBA_M6_P0_1_GLOBAL_NEW,
	PCBA_M6_P0_1_LA,
	PCBA_M6_P0_1_GLOBAL_OP,

	PCBA_M6_P1_GLOBAL = 5,
	PCBA_M6_P1_GLOBAL_NEW,
	PCBA_M6_P1_LA,
	PCBA_M6_P1_GLOBAL_OP,

	PCBA_M6_P1_1_GLOBAL = 9,
	PCBA_M6_P1_1_GLOBAL_NEW,
	PCBA_M6_P1_1_LA,
	PCBA_M6_P1_1_GLOBAL_OP,
	
	PCBA_M6_P1_2_GLOBAL = 13,
	PCBA_M6_P1_2_GLOBAL_NEW,
	PCBA_M6_P1_2_LA,
	PCBA_M6_P1_2_GLOBAL_OP,

	PCBA_M6_P2_GLOBAL = 17,
	PCBA_M6_P2_GLOBAL_NEW,
	PCBA_M6_P2_LA,
	PCBA_M6_P2_GLOBAL_OP,

	PCBA_M6_MP_GLOBAL = 21,
	PCBA_M6_MP_GLOBAL_NEW,
	PCBA_M6_MP_LA,
	PCBA_M6_MP_GLOBAL_OP,

	PCBA_INFO_END,
} PCBA_INFO;

typedef enum
{
	STAGE_UNKNOW = 0,
	P0_1,
	P1,
	P1_1,
	P1_2,
	P2,
	MP,
} PROJECT_STAGE;
#endif


#if defined(PROJECT_ROCK)
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
#elif defined(PROJECT_DIAMOND)
struct project_stage {
	int voltage_min;
	int voltage_max;
	PROJECT_STAGE project_stage;
	char hwc_level[20];
} stage_map[] = {
	{ 130,  225,   P0_1,   "P0.1",},
	{ 226,  315,   P1,     "P1",  },
	{ 316,  405,   P1_1,   "P1.1",},
	{ 406,  485,   P1_2,   "P1.2",  },
	{ 486,  565,   P2,     "P2",  },
	{ 916,  1010,  MP,     "MP",  },
};
#endif

#if defined(PROJECT_ROCK)
struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
	char hwc_name[8];
	char hwversion[16];
} pcba_map[] = {

	{PCBA_L19C_P0_1_IN,     "PCBA_L19C_P0-1_IN",     "India",  "6138E.20.1"},
	{PCBA_L19C_P0_1_GLOBAL, "PCBA_L19C_P0-1_GLOBAL", "Global", "6138E.10.1"},
	{PCBA_L19A_P0_1_IN,     "PCBA_L19A_P0-1_IN",     "India",  "6138D.20.1"},

	{PCBA_L19C_P1_IN,       "PCBA_L19C_P1_IN",       "India",  "6138E.21.0"},
	{PCBA_L19C_P1_GLOBAL,   "PCBA_L19C_P1_GLOBAL",   "Global", "6138E.11.0"},
	{PCBA_L19A_P1_IN,       "PCBA_L19A_P1_IN",       "India",  "6138D.21.0"},

	{PCBA_L19C_P1_1_IN,     "PCBA_L19C_P1-1_IN",     "India",  "6138E.21.1"},
	{PCBA_L19C_P1_1_GLOBAL, "PCBA_L19C_P1-1_GLOBAL", "Global", "6138E.11.1"},
	{PCBA_L19A_P1_1_IN,     "PCBA_L19A_P1-1_IN",     "India",  "6138D.21.1"},

	{PCBA_L19C_P2_IN,       "PCBA_L19C_P2_IN",       "India",  "6138E.22.0"},
	{PCBA_L19C_P2_GLOBAL,   "PCBA_L19C_P2_GLOBAL",   "Global", "6138E.12.0"},
	{PCBA_L19A_P2_IN,       "PCBA_L19A_P2_IN",       "India",  "6138D.22.0"},

	{PCBA_L19C_MP_IN,       "PCBA_L19C_MP_IN",       "India",  "6138E.29.0"},
	{PCBA_L19C_MP_GLOBAL,   "PCBA_L19C_MP_GLOBAL",   "Global", "6138E.19.0"},
	{PCBA_L19A_MP_IN,       "PCBA_L19A_MP_IN",       "India",  "6138D.29.0"},
};
#elif defined(PROJECT_DIAMOND)
struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
	char hwc_name[8];
	char hwversion[16];
} pcba_map[] = {

	{PCBA_M6_P0_1_GLOBAL,       "PCBA_M6_P0-1_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_P0_1_GLOBAL_NEW,   "PCBA_M6_P0-1_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_P0_1_LA,          "PCBA_M6_P0-1_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_P0_1_GLOBAL_OP,   "PCBA_M6_P0-1_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},

	{PCBA_M6_P1_GLOBAL,       "PCBA_M6_P1_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_P1_GLOBAL_NEW,   "PCBA_M6_P1_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_P1_LA,          "PCBA_M6_P1_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_P1_GLOBAL_OP,   "PCBA_M6_P1_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},

	{PCBA_M6_P1_1_GLOBAL,       "PCBA_M6_P1-1_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_P1_1_GLOBAL_NEW,   "PCBA_M6_P1-1_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_P1_1_LA,          "PCBA_M6_P1-1_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_P1_1_GLOBAL_OP,   "PCBA_M6_P1-1_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},

	{PCBA_M6_P1_2_GLOBAL,       "PCBA_M6_P1-2_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_P1_2_GLOBAL_NEW,   "PCBA_M6_P1-2_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_P1_2_LA,          "PCBA_M6_P1-2_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_P1_2_GLOBAL_OP,   "PCBA_M6_P1-2_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},

	{PCBA_M6_P2_GLOBAL,       "PCBA_M6_P2_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_P2_GLOBAL_NEW,   "PCBA_M6_P2_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_P2_LA,          "PCBA_M6_P2_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_P2_GLOBAL_OP,   "PCBA_M6_P2_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},

	{PCBA_M6_MP_GLOBAL,       "PCBA_M6_MP_GLOBAL",        "Global",  "6138E.20.1"},
	{PCBA_M6_MP_GLOBAL_NEW,   "PCBA_M6_MP_GLOBAL_NEW",    "Global",  "6138E.10.1"},
	{PCBA_M6_MP_LA,          "PCBA_M6_MP_LA",           "Latin",   "6138D.20.1"},
	{PCBA_M6_MP_GLOBAL_OP,   "PCBA_M6_MP_GLOBAL_OP",     "GLOBAL",  "6138D.20.1"},
};
#endif

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
