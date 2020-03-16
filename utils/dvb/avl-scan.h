// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Blind scan program for Availink demodulators
 *
 * Copyright (C) 2020 Availink, Inc. (opensource@availink.com)
 *
 */

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/time.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#include <termios.h>
#include <libdvbv5/dvb-file.h>
#include <argp.h>
#include "libdvbv5/dvb-dev.h"
#include "libdvbv5/dvb-dev-priv.h"

#ifdef ENABLE_NLS
# define _(string) gettext(string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# define _(string) string
#endif

# define N_(string) string

#define PROGRAM_NAME	"avl-scan"
#define DEFAULT_OUTPUT  "channels.conf"


#define AVL62X1_BS_CTRL_PROP              isdbt_sb_segment_idx
//isdbt_sb_segment_idx fields
#define AVL62X1_BS_CTRL_VALID_STREAM_MASK (0x80000000)
#define AVL62X1_BS_CTRL_NEW_TUNE_MASK     (0x40000000)
#define AVL62X1_BS_CTRL_MORE_RESULTS_MASK (0x20000000)
#define AVL62X1_BS_CTRL_TUNER_STEP_MASK   (0x0001FFFF) //128k kHz

//stream_id fields
#define AVL62X1_BS_IS_T2MI_SHIFT      29
#define AVL62X1_BS_T2MI_PID_SHIFT     16
#define AVL62X1_BS_T2MI_PLP_ID_SHIFT  8

#define MAX_TIME		10	/* 1.0 seconds */

#define MILLION			1000000u
#define THOUSAND		1000u

#define C_F_BLACK		"\033[30m"
#define C_F_RED			"\033[31m"
#define C_F_GREEN		"\033[32m"
#define C_F_ORANGE	"\033[33m"
#define C_F_BLUE		"\033[34m"
#define C_F_MAGENTA	"\033[35m"
#define C_F_CYAN		"\033[36m"
#define C_F_LTGREY	"\033[37m"
#define C_F_RESET		"\033[39m"

#define C_B_BLACK		"\033[40m"
#define C_B_RED			"\033[41m"
#define C_B_GREEN		"\033[42m"
#define C_B_ORANGE	"\033[43m"
#define C_B_BLUE		"\033[44m"
#define C_B_MAGENTA	"\033[45m"
#define C_B_CYAN		"\033[46m"
#define C_B_LTGREY	"\033[47m"
#define C_B_RESET		"\033[49m"

#define C_B_DK_GRAY		"\033[100m"
#define C_B_LT_RED		"\033[101m"
#define C_B_LT_GREEN	"\033[102m"
#define C_B_YELLOW		"\033[103m"
#define C_B_LT_BLUE		"\033[104m"
#define C_B_LT_PURPLE	"\033[105m"
#define C_B_TEAL			"\033[106m"
#define C_B_WHITE			"\033[107m"

#define C_NOTE				C_F_GREEN
#define C_INFO				C_F_BLACK C_B_CYAN
#define C_GOOD				C_F_GREEN
#define C_OKAY				C_F_ORANGE
#define C_BAD					C_F_RED
#define C_RESET				C_F_RESET C_B_RESET


#define xioctl(fh, request, arg...) ({                      \
	int __rc;                                                 \
	struct timespec __start, __end;                           \
                                                            \
	clock_gettime(CLOCK_MONOTONIC, &__start);                 \
	do                                                        \
	{                                                         \
		__rc = ioctl(fh, request, ##arg);                       \
		if (__rc != -1)                                         \
			break;                                                \
		if ((errno != EINTR) && (errno != EAGAIN))              \
			break;                                                \
		clock_gettime(CLOCK_MONOTONIC, &__end);                 \
		if (__end.tv_sec * 10 + __end.tv_nsec / 100000000 >     \
				__start.tv_sec * 10 + __start.tv_nsec / 100000000 + \
						MAX_TIME)                                       \
			break;                                                \
	} while (1);                                              \
                                                            \
	__rc;                                                     \
})
