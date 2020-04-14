// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Blind scan program for Availink demodulators
 *
 * Copyright (C) 2020 Availink, Inc. (opensource@availink.com)
 *
 */

#include "avl-scan.h"

#if DVB_API_VERSION < 5 || DVB_API_VERSION_MINOR < 2
#error avl-scan requires Linux DVB driver API version 5.2 and newer!
#endif

#ifndef DTV_STREAM_ID
#define DTV_STREAM_ID DTV_ISDBS_TS_ID
#endif

#ifndef NO_STREAM_ID_FILTER
#define NO_STREAM_ID_FILTER (~0U)
#endif

//----------------------------------------------
//because libdvb got a little carried
//away with making useful things static
struct freqrange_priv {
	unsigned low, high, int_freq, rangeswitch;
	enum dvb_sat_polarization pol;
};

struct lnb_priv {
	struct dvb_sat_lnb desc;

	/* Private members used internally */
	struct freqrange_priv freqrange[4];
};
//----------------------------------------------

struct cl_arguments
{
  int verbose;
  char *lnb_name, *output;
  unsigned adapter, n_adapter, adapter_fe, frontend;
  int lna, lnb, sat_number, freq_bpf, freq_band;
  unsigned diseqc_wait, timeout_multiply;
  const char *cc;

  /* Used by status print */
  unsigned n_status_lines;
};

static const struct argp_option options[] = {
    {"adapter", 'a', N_("adapter#"), 0, N_("use given adapter (default 0)"), 0},
    {"frontend", 'f', N_("frontend#"), 0, N_("use given frontend (default 0)"), 0},
    {"lnbf", 'l', N_("LNBf_type"), 0, N_("type of LNBf to use. 'help' lists the available ones"), 0},
    {"freq_band", 'F', N_("(all, band #)"), 0, N_("Frequency band number (from LNB types list, starting with 0) or 'all'"), 0},
    {"lna", 'w', N_("LNA (on, off, auto)"), 0, N_("enable/disable/auto LNA power"), 0},
    {"sat_number", 'S', N_("satellite_number"), 0, N_("satellite number. If not specified, disable DISEqC"), 0},
    {"freq_bpf", 'U', N_("frequency"), 0, N_("SCR/Unicable band-pass filter frequency to use, in kHz"), 0},
    {"wait", 'W', N_("time"), 0, N_("adds additional wait time for DISEqC command completion"), 0},
    {"verbose", 'v', NULL, 0, N_("be (very) verbose"), 0},
    {"output", 'o', N_("file"), 0, N_("output filename (default: ") DEFAULT_OUTPUT ")", 0},
    {"timeout-multiply", 'T', N_("factor"), 0, N_("Multiply scan timeouts by this factor"), 0},
    {"cc", 'C', N_("country_code"), 0, N_("Set the default country to be used (in ISO 3166-1 two letter code)"), 0},
    {"help", '?', 0, 0, N_("Give this help list"), -1},
    {"usage", -3, 0, 0, N_("Give a short usage message")},
    {"version", 'V', 0, 0, N_("Print program version"), -1},
    {0, 0, 0, 0, 0, 0}};

struct cl_arguments args;

char bs_mode_path[1024] = {0};

void set_blindscan_mode() {
  unsigned int cur_bs_mode;
  FILE *bs_mode_handle = fopen(bs_mode_path, "r+");
  if(!bs_mode_handle) {
    printf("ERROR: could not set blindscan mode\n");
    exit(1);
  }
  if(fscanf(bs_mode_handle,"0x%x",&cur_bs_mode) <= 0) {
    printf("ERROR: could not set blindscan mode\n");
    exit(1);
  }
  cur_bs_mode |= (1<<args.frontend);
  if(fprintf(bs_mode_handle,"0x%.4x",cur_bs_mode) < 0) {
    printf("ERROR: could not set blindscan mode\n");
    exit(1);
  }
  fclose(bs_mode_handle);
}

void unset_blindscan_mode() {
  unsigned int cur_bs_mode;
  if(strlen(bs_mode_path) > 0) {
    FILE *bs_mode_handle = fopen(bs_mode_path, "r+");
    if(!bs_mode_handle) {
      printf("ERROR: could not unset blindscan mode\n");
      exit(1);
    }
    if(fscanf(bs_mode_handle,"0x%x",&cur_bs_mode) <= 0) {
      printf("ERROR: could not unset blindscan mode\n");
      exit(1);
    }
    cur_bs_mode &= ~(1<<args.frontend);
    if(fprintf(bs_mode_handle,"0x%.4x",cur_bs_mode) < 0) {
      printf("ERROR: could not unset blindscan mode\n");
      exit(1);
    }
    fclose(bs_mode_handle);
  }
}

void sig_handler(int signo)
{
  if (signo == SIGINT) {
    printf("\nCaught SIGINT\n");
    unset_blindscan_mode();
    printf("Exiting...\n");
    exit(0);
  }
}

void print_band(
    const char *prefix,
    const char *postfix,
    int num,
    struct freqrange_priv *band)
{
  printf(_("%s%d: %d MHz to %d MHz, LO %d MHz"),
         prefix,
         num,
         band->low,
         band->high,
         band->int_freq);
  switch (band->pol)
  {
  case POLARIZATION_OFF:
    break;
  case POLARIZATION_H:
    printf(_(", HORIZONTAL"));
    break;
  case POLARIZATION_V:
    printf(_(", VERTICAL"));
    break;
  case POLARIZATION_L:
    printf(_(", LEFT"));
    break;
  case POLARIZATION_R:
    printf(_(", RIGHT"));
    break;
  }
  printf(_("%s"),postfix);
}

int scan(int frontend_fd,
         uint32_t min_rf_hz,
         uint32_t max_rf_hz,
         uint32_t lo_khz,
         char inv_lo,
         struct dvb_file *chans_file,
         struct dvb_v5_fe_parms *parms)
{
  static struct dvb_entry *cur_entry = NULL;

  struct dtv_property p_clear[] = {
      {.cmd = DTV_CLEAR}};

  struct dtv_properties cmdseq_clear = {
      .num = 1,
      .props = p_clear};

  if ((xioctl(frontend_fd, FE_SET_PROPERTY, &cmdseq_clear)) == -1)
  {
    perror("FE_SET_PROPERTY DTV_CLEAR failed");
    return -1;
  }
  usleep(20000);

  struct dvb_frontend_event ev;

#if 0
  // discard stale QPSK events
  while (1)
  {
    if (xioctl(frontend_fd, FE_GET_EVENT, &ev) == -1)
      break;
  }
#endif

  uint32_t cur_rf_hz = min_rf_hz;
  uint32_t bs_ctrl = AVL62X1_BS_CTRL_NEW_TUNE_MASK; //new rf freq
  do
  {
    struct dtv_property p_carr_search[] = {
        {.cmd = DTV_FREQUENCY, .u.data = cur_rf_hz / 1000},
        {.cmd = AVL62X1_BS_CTRL_CMD, .u.data = bs_ctrl},
        {.cmd = DTV_SYMBOL_RATE, .u.data = 55000000}, //set to any valid rate
        {.cmd = DTV_TUNE},
    };
    struct dtv_properties cmdseq_carr_search = {
        .num = ARRAY_SIZE(p_carr_search),
        .props = p_carr_search};

    if (xioctl(frontend_fd, FE_SET_PROPERTY, &cmdseq_carr_search) == -1)
    {
      perror("FE_SET_PROPERTY TUNE failed");
    }
    usleep(200000);

    // wait for zero status indicating start of tunning
    do
    {
      xioctl(frontend_fd, FE_GET_EVENT, &ev);
    } while (ev.status != 0);

    if (xioctl(frontend_fd, FE_GET_EVENT, &ev) == -1)
    {
      ev.status = 0;
    }

    int i;
    fe_status_t status;
    for (i = 0; i < 20; i++)
    {
      if (xioctl(frontend_fd, FE_READ_STATUS, &status) == -1)
      {
        perror("FE_READ_STATUS failed");
      }

      if (status & FE_HAS_LOCK || status & FE_TIMEDOUT)
        break;
      else
        sleep(1);
    }

    struct dtv_property p[] = {
        {.cmd = AVL62X1_BS_CTRL_CMD},
        {.cmd = DTV_FREQUENCY},
        {.cmd = DTV_SYMBOL_RATE},
        {.cmd = DTV_DELIVERY_SYSTEM},
        {.cmd = DTV_PILOT},
        {.cmd = DTV_STREAM_ID}};

    struct dtv_properties p_status = {
        .num = ARRAY_SIZE(p),
        .props = p};

    if ((xioctl(frontend_fd, FE_GET_PROPERTY, &p_status)) == -1)
    {
      perror("FE_GET_PROPERTY failed");
      return -1;
    }

    unsigned int stream_id = p[p_status.num - 1].u.data;

    printf("\n\n%sFtune %.3f MHz%s\n", C_INFO, cur_rf_hz/1e6f, C_RESET);
    
    if (p[0].u.data & AVL62X1_BS_CTRL_VALID_STREAM_MASK)
    {
      if (cur_entry == NULL)
      {
        cur_entry = (struct dvb_entry *)calloc(1, sizeof(struct dvb_entry));
        if (!cur_entry)
        {
          printf("ERROR: calloc() failed\n");
          exit(-1);
        }
        chans_file->first_entry = cur_entry;
        chans_file->n_entries = 1;
      }
      else
      {
        cur_entry->next = (struct dvb_entry *)calloc(1, sizeof(struct dvb_entry));
        if (!cur_entry->next)
        {
          printf("ERROR: calloc() failed\n");
          exit(-1);
        }
        cur_entry = cur_entry->next;
        chans_file->n_entries++;
      }
      cur_entry->sat_number = -1;

      //convert frequency to khz and add LO
      printf(C_NOTE);
      //printf("IF %.6f MHz\n", p[1].u.data/1e6f);
      //p[1].u.data /= 1000;
      printf("IF %.6f MHz\n", p[1].u.data/1e3f);
      if(inv_lo) {
        p[1].u.data = lo_khz - p[1].u.data;
      } else {
        p[1].u.data += lo_khz;
      }

      memcpy(cur_entry->props,
             &(p[1]),
             (p_status.num - 1) * sizeof(struct dtv_property));
      cur_entry->n_props = p_status.num - 1;

      uint32_t pol;
      dvb_fe_retrieve_parm(parms, DTV_POLARIZATION, &pol);
      dvb_store_entry_prop(cur_entry, DTV_POLARIZATION, pol);

      printf("Freq %.6f MHz\n", p[1].u.data/1e3f);
      printf("Symrate %.3f Msps\n", p[2].u.data/1e6f);
      printf("ISI %d\n", stream_id & 0xFF);
      if ((stream_id >> AVL62X1_BS_IS_T2MI_SHIFT) & 1)
      {
        printf("T2MI PID 0x%.4x\n", (stream_id >> AVL62X1_BS_T2MI_PID_SHIFT) & 0x1FFF);
        printf("T2MI PLP ID %d\n", (stream_id >> AVL62X1_BS_T2MI_PLP_ID_SHIFT) & 0xFF);
      }
      printf("STD %s\n", (p[3].u.data == SYS_DVBS2) ? "S2" : "S");
    }
    else
    {
      printf(C_OKAY "No streams\n" C_RESET);
    }
    // for(int i=0; i<p_status.num; i++) {
    //   printf("%d => %d\n",p[i].cmd,p[i].u.data);
    // }

    printf(C_RESET);

    if (p_status.props[0].u.data & AVL62X1_BS_CTRL_MORE_RESULTS_MASK)
    {
      //more streams in this carrier
      bs_ctrl = 0;
    }
    else
    {
      if (cur_rf_hz >= max_rf_hz)
        break;
      cur_rf_hz +=
          (p_status.props[0].u.data & AVL62X1_BS_CTRL_TUNER_STEP_MASK) *
          1000; //move RF freq
      cur_rf_hz = MIN(cur_rf_hz, max_rf_hz);
      bs_ctrl = AVL62X1_BS_CTRL_NEW_TUNE_MASK;
      printf("Step tuner by %.3f MHz\n",
             (p_status.props[0].u.data & AVL62X1_BS_CTRL_TUNER_STEP_MASK) / 1e3f);
    }

  } while (cur_rf_hz <= max_rf_hz);

  return 0;
}

static error_t parse_opt(int k, char *optarg, struct argp_state *state)
{
  struct cl_arguments *args = state->input;
  switch (k)
  {
  case 'a':
    args->adapter = strtoul(optarg, NULL, 0);
    args->n_adapter++;
    break;
  case 'f':
    args->frontend = strtoul(optarg, NULL, 0);
    args->adapter_fe = args->adapter;
    break;
  case 'w':
    if (!strcasecmp(optarg, "on"))
    {
      args->lna = 1;
    }
    else if (!strcasecmp(optarg, "off"))
    {
      args->lna = 0;
    }
    else if (!strcasecmp(optarg, "auto"))
    {
      args->lna = LNA_AUTO;
    }
    else
    {
      int val = strtoul(optarg, NULL, 0);
      if (!val)
        args->lna = 0;
      else if (val > 0)
        args->lna = 1;
      else
        args->lna = LNA_AUTO;
    }
    break;
  case 'l':
    args->lnb_name = optarg;
    break;
  case 'F':
    if(!strncmp("all",optarg,3) || !strncmp("ALL",optarg,3)) {
      args->freq_band = -1;
    } else {
      args->freq_band = strtoul(optarg, NULL, 0);
    }
    break;
  case 'S':
    args->sat_number = strtoul(optarg, NULL, 0);
    break;
  case 'U':
    args->freq_bpf = strtoul(optarg, NULL, 0);
    break;
  case 'W':
    args->diseqc_wait = strtoul(optarg, NULL, 0);
    break;
  case 'v':
    args->verbose++;
    break;
  case 'T':
    args->timeout_multiply = strtoul(optarg, NULL, 0);
    break;
  case 'o':
    args->output = optarg;
    break;
  case 'C':
    args->cc = strndup(optarg, 2);
    break;
  case '?':
    argp_state_help(state, state->out_stream,
                    ARGP_HELP_SHORT_USAGE | ARGP_HELP_LONG | ARGP_HELP_DOC);
    fprintf(state->out_stream, _("\nReport bugs to %s.\n"), argp_program_bug_address);
    exit(0);
  case 'V':
    fprintf(state->out_stream, "%s\n", argp_program_version);
    exit(0);
  case -3:
    argp_state_help(state, state->out_stream, ARGP_HELP_USAGE);
    exit(0);
  default:
    return ARGP_ERR_UNKNOWN;
  };
  return 0;
}

void dedupe_channels(struct dvb_file *chans) {
  //this is O(n^2) and dumb, but n is really small...
  for (struct dvb_entry *entry = chans->first_entry;
       entry != NULL;
       entry = entry->next)
  {
    struct dvb_entry *oe; //other entry
    struct dvb_entry *poe; //prev other entry
    for (poe = entry, oe = entry->next;
       oe != NULL;
       poe = poe->next, oe = oe->next)
    {
      uint32_t f1, f2, sr1, sr2, pol1, pol2, isi1, isi2;
      dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &f1); //khz
      dvb_retrieve_entry_prop(oe, DTV_FREQUENCY, &f2);

      dvb_retrieve_entry_prop(entry, DTV_SYMBOL_RATE, &sr1); //sps
      dvb_retrieve_entry_prop(oe, DTV_SYMBOL_RATE, &sr2);

      dvb_retrieve_entry_prop(entry, DTV_POLARIZATION, &pol1);
      dvb_retrieve_entry_prop(oe, DTV_POLARIZATION, &pol2);

      dvb_retrieve_entry_prop(entry, DTV_STREAM_ID, &isi1);
      dvb_retrieve_entry_prop(oe, DTV_STREAM_ID, &isi2);

      if ((pol1 == pol2) &&             //same polaritzation (or none)
          (abs(f1 - f2) <= 5000) &&     //freqs within 0.5MHz
          (abs(sr1 - sr2) <= 500000) && //symrates within 0.5Msps
          (isi1 == isi2)) //this contains ISI, PLP ID, T2MI PID
      {
        //duplicate entry - remove it
        printf(_("Removing channel duplicate: %.6f MHz, %.3f Msps, Pol %d, ISI %d\n"),
               f2 / 1e3f, sr2 / 1e6f, pol2, isi2); fflush(stdout);
        struct dvb_entry *delme = oe;
        poe->next = delme->next;
        free(delme);
      }
    }
  }
}

int main(int argc, char *argv[])
{
  int err, lnb = -1, idx = -1;
  struct dvb_device *dvb;
  struct dvb_dev_list *dvb_dev;
  struct dvb_v5_fe_parms *parms;
  struct dvb_open_descriptor *fe_handle;
  const struct argp argp = {
      .options = options,
      .parser = parse_opt,
      .doc = N_("scan DVB services using the channel file"),
      .args_doc = N_(""),
  };
  struct lnb_priv *lnb_p;
  int n_freq_bands = 0;
  struct dvb_file chans_file;

  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("\nCan't catch SIGINT\n");

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  memset(&args, 0, sizeof(args));
  args.sat_number = -1;
  args.output = DEFAULT_OUTPUT;
  args.timeout_multiply = 1;
  args.adapter = (unsigned)-1;
  args.lna = LNA_AUTO;
  args.freq_band = -1;

  if (argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, &idx, &args))
  {
    argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, PROGRAM_NAME);
    return -1;
  }

  if (args.timeout_multiply == 0)
    args.timeout_multiply = 1;

  if (args.n_adapter == 1)
  {
    args.adapter_fe = args.adapter;
  }

  if (args.lnb_name)
  {
    lnb = dvb_sat_search_lnb(args.lnb_name);
    if (lnb < 0)
    {
      printf(_(C_OKAY "Please select one of the LNBf's below:\n" C_RESET));
      dvb_print_all_lnb();
      exit(1);
    }
    else
    {
      lnb_p = (struct lnb_priv *)dvb_sat_get_lnb(lnb);
      
      for (int i = 0; i < ARRAY_SIZE(lnb_p->freqrange) && lnb_p->freqrange[i].low; i++)
        n_freq_bands++;
      if ((args.freq_band >= 0) && (args.freq_band >= n_freq_bands))
      {
        printf(_(C_BAD "Invalid LNB frequency band %d.\n" C_RESET), args.freq_band);
        printf(_("LNB %s has %d frequency bands:\n"),
               lnb_p->desc.alias, n_freq_bands);
        for (int i = 0; i < n_freq_bands; i++)
        {
          print_band("\t","\n",i, &(lnb_p->freqrange[i]));
        }
        exit(1);
      }

      printf(_("Using LNB '%s'. Frequency bands:\n"), lnb_p->desc.alias);
      for (int i = 0; i < n_freq_bands; i++) {
        print_band("\t","\n",i, &(lnb_p->freqrange[i]));
      }
    }
  } else {
    printf(_(C_OKAY "Please select one of the LNBf's below:\n" C_RESET));
    dvb_print_all_lnb();
    exit(1);
  }

  dvb = dvb_dev_alloc();
  if (!dvb)
    return -1;
  dvb_dev_set_log(dvb, args.verbose, NULL);
  dvb_dev_find(dvb, NULL, NULL);
  parms = dvb->fe_parms;

  dvb_dev = dvb_dev_seek_by_adapter(dvb, args.adapter_fe, args.frontend,
                                    DVB_DEVICE_FRONTEND);
  if (!dvb_dev)
    return -1;

  fe_handle = dvb_dev_open(dvb, dvb_dev->sysname, O_RDWR);
  if (!fe_handle)
  {
    return -1;
  }
  if (lnb >= 0)
    parms->lnb = dvb_sat_get_lnb(lnb);
  if (args.sat_number >= 0)
    parms->sat_number = args.sat_number;
  parms->diseqc_wait = args.diseqc_wait;
  parms->freq_bpf = args.freq_bpf;
  parms->lna = args.lna;
  parms->verbose = args.verbose;
  err = dvb_fe_set_default_country(parms, args.cc);
  if (err < 0)
    fprintf(stderr, _(C_BAD "Failed to set the country code:%s\n" C_RESET), args.cc);

  struct dvb_frontend_info info;
  if ((xioctl(fe_handle->fd, FE_GET_INFO, &info)) == -1)
  {
    perror(C_BAD "FE_GET_INFO failed\n" C_RESET);
    return -1;
  }

  
  printf("\nFrontend: %s\n\tFmin  %d MHz\n\tFmax  %d MHz\n\tSRmin %d Ksps\n\tSRmax %d Ksps\n",
         info.name,
         info.frequency_min / THOUSAND,
         info.frequency_max / THOUSAND,
         info.symbol_rate_min / THOUSAND,
         info.symbol_rate_max / THOUSAND);


  //put kernel module into blindscan mode
  if(!strcmp(info.name, "Availink avl62x1")) {
    sprintf(bs_mode_path, "/sys/module/avl62x1/parameters/bs_mode");
  } else if(!strcmp(info.name, "Availink avl68x2")) {
    sprintf(bs_mode_path, "/sys/module/avl68x2/parameters/bs_mode");
  } else {
    printf(C_BAD "Frontend '%s' not supported.  Only 'Availink avl62x1' and 'Availink avl68x2' supported." C_RESET,info.name);
    exit(1);
  }

  set_blindscan_mode();

  int first_band, last_band;
  if(args.freq_band == -1) {
    first_band = 0;
    last_band = n_freq_bands-1;
  } else {
    first_band = args.freq_band;
    last_band = args.freq_band;
  }

  for (int i = first_band; i <= last_band; i++) {

    char inv_lo = (lnb_p->freqrange[i].int_freq > lnb_p->freqrange[i].low);

    unsigned int start_freq_hz, stop_freq_hz;
    if(inv_lo) {
      start_freq_hz = abs(lnb_p->freqrange[i].high -
                    lnb_p->freqrange[i].int_freq) *
                    MILLION;
      stop_freq_hz = abs(lnb_p->freqrange[i].low -
                    lnb_p->freqrange[i].int_freq) *
                  MILLION;
    } else {
      start_freq_hz = abs(lnb_p->freqrange[i].low -
                    lnb_p->freqrange[i].int_freq) *
                    MILLION;
      stop_freq_hz = abs(lnb_p->freqrange[i].high -
                    lnb_p->freqrange[i].int_freq) *
                  MILLION;
    }
    
    start_freq_hz = MAX(info.frequency_min * THOUSAND, start_freq_hz);
    stop_freq_hz = MIN(info.frequency_max * THOUSAND, stop_freq_hz);

    printf(_("\n\n%sScanning frequency band"),C_INFO);
    print_band(" ",C_RESET "\n",i,&(lnb_p->freqrange[i]));
    printf(_(C_INFO "IF from %d MHz to %d MHz%s\n"),
          start_freq_hz / MILLION,
          stop_freq_hz / MILLION,
          C_RESET);

    dvb_fe_store_parm(parms, DTV_POLARIZATION, lnb_p->freqrange[i].pol);
    dvb_fe_store_parm(parms, DTV_FREQUENCY,
                      ((lnb_p->freqrange[i].high +
                        lnb_p->freqrange[i].low) /
                      2) *
                          THOUSAND);

    dvb_sat_set_parms(parms);

    scan(fe_handle->fd,
        start_freq_hz,
        stop_freq_hz,
        lnb_p->freqrange[i].int_freq * THOUSAND,
        inv_lo,
        &chans_file,
        parms);
  }

  //don't mess around trying to deal with any frequeny overlaps
  //  up front.  just remove any duplicate channels afterwards
  dedupe_channels(&chans_file);

  printf("\n%sFound a total of %d channels%s\n",C_INFO,chans_file.n_entries,C_RESET);

  dvb_write_file(args.output, &chans_file);

  dvb_dev_free(dvb);

  unset_blindscan_mode();

  printf("Exiting...\n");
  return 0;
}
