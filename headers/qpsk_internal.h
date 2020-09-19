#pragma once

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif
    
#define OSC_TABLE_SIZE  32
#define TX_SAMPLES_SIZE (33 * 31)
#define RRCLEN          39
    
#ifndef M_PI
#define M_PI        3.14159265358979323846f
#endif

#define TAU         (2.0f * M_PI)
#define ROT45       (M_PI / 4.0f)

#define cmplx(value) (cosf(value) + sinf(value) * I)
#define cmplxconj(value) (cosf(value) + sinf(value) * -I)

/* modem state machine states */
typedef enum {
    search,
    trial,
    synced
} State;

typedef enum {
    unsync,             /* force sync state machine to lose sync, and search for new sync */
    autosync,           /* falls out of sync automatically */
    manualsync          /* fall out of sync only under operator control */
} Sync;

/* phase estimator bandwidth options */

typedef enum {
    low_bw,             /* can only track a narrow freq offset, but accurate         */
    high_bw             /* can track wider freq offset, but less accurate at low SNR */
} PhaseEstBandwidth;

/*
 * User-defined configuration for OFDM modem.  Used to set up
 * constants at init time, e.g. for different bit rate modems.
 */

struct QPSK_CONFIG {
    float centre; /* Centre Audio Frequency */
    float fs;  /* Sample Frequency */
    float rs;  /* Modulation Symbol Rate */
    float ts;  /* symbol duration */
    float tcp; /* Cyclic Prefix duration */
    float timing_mx_thresh;

    int ns;  /* Number of Symbol frames */
    int bps;   /* Bits per Symbol */
    int txtbits; /* number of auxiliary data bits */
    int ftwindowwidth;

    int samples_per_frame;
};

struct QPSK {
    struct QPSK_CONFIG config;
    
    float timing_mx_thresh;
    
    int ns;	/* NS-1 = data symbols between pilots  */
    int bps; 	/* Bits per symbol */
    int m; 	/* duration of each symbol in samples */
    int ncp; 	/* duration of CP in samples */

    int ftwindowwidth;
    int bitsperframe;
    int rowsperframe;
    int samplesperframe;
    int max_samplesperframe;
    int nrxbuf;
    int ntxtbits; /* reserve bits/frame for aux text information */
    int nuwbits; /* Unique word used for positive indication of lock */

    float centre;
    float fs; /* Sample rate */
    float ts; /* Symbol cycle time */
    float rs; /* Symbol rate */
    float tcp; /* Cyclic prefix duration */
    float inv_m; /* 1/m */
    float doc; /* division of radian circle */
    
    // Pointers
    
    complex float *pilot_samples;
    complex float *rxbuf;
    complex float *pilot;
    complex float **rx_sym;
    complex float *rx_np;
    complex float *tx_uw_syms;
    
    float *rx_amp;
    float *aphase_est_pilot_log;

    uint8_t *tx_uw;
    int *uw_ind;
    int *uw_ind_sym;

    // State enums
    State sync_state;
    State last_sync_state;
    State sync_state_interleaver;
    State last_sync_state_interleaver;

    // Sync enums
    Sync sync_mode;

    // Phase enums
    PhaseEstBandwidth phase_est_bandwidth;

    int phase_est_bandwidth_mode;

    // Complex
    complex float foff_metric;
     
    // Float
    float foff_est_gain;
    float foff_est_hz;
    float timing_mx;
    float coarse_foff_est_hz;
    float timing_norm;
    float sig_var;
    float noise_var;
    float mean_amp;

    // Integer
    int clock_offset_counter;
    int verbose;
    int sample_point;
    int timing_est;
    int timing_valid;
    int nin;
    int uw_errors;
    int sync_counter;
    int frame_count;
    int frame_count_interleaver;

    // Boolean
    bool sync_start;
    bool sync_end;
    bool timing_en;
    bool foff_est_en;
    bool phase_est_en;
    bool tx_bpf_en;
    bool dpsk;
};

/* Prototypes */

complex float qpsk_mod(int *);
void qpsk_demod(complex float, int *);
void qpsk_txframe(struct QPSK *, complex float *, complex float []);
void qpsk_assemble_modem_frame(struct QPSK *, uint8_t [], uint8_t [], uint8_t []);
void qpsk_assemble_modem_frame_symbols(struct QPSK *, complex float [], complex float [], uint8_t []);
void qpsk_disassemble_modem_frame(struct QPSK *, uint8_t [], complex float [], float [], short []);
void qpsk_rand(uint16_t [], int);
void qpsk_generate_payload_data_bits(uint8_t [], int);
int qpsk_get_phase_est_bandwidth_mode(struct QPSK *);
void qpsk_set_phase_est_bandwidth_mode(struct QPSK *, int);

#ifdef __cplusplus
}
#endif

