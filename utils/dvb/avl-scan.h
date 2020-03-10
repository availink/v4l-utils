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

#define AVL62X1_BS_CTRL_CMD			DTV_ISDBT_SB_SEGMENT_IDX

#define AVL62X1_BS_STREAM_INVALID_SHIFT	30
#define AVL62X1_BS_IS_T2MI_SHIFT	29
#define AVL62X1_BS_T2MI_PID_SHIFT	16
#define AVL62X1_BS_T2MI_PLP_ID_SHIFT	8

#define AVL62X1_BS_NEW_TUNE		(uint32_t)-1
#define AVL62X1_BS_MORE_RESULTS		(uint32_t)-2

#define MAX_TIME		10	/* 1.0 seconds */

#define MILLION			1000000u
#define THOUSAND		1000u

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
