% test_filter.m
% test Root Nyquist (Rasised Root Cosine) filter

M = 5; Nsym = 10; Fs = 8000; Rs = 1600; alpha = 0.31;
hs = gen_rn_coeffs(alpha, 1.0/Fs, Rs, Nsym, 5);

Nsymbols = 100; Nsamples = Nsymbols*M;

% QPSK symbols
tx_sym = 1 - 2*(rand(1,Nsymbols) > 0.5) + j*(1 - 2*(rand(1,Nsymbols) > 0.5));

tx_sym_zero_pad = zeros(1,Nsamples);
tx_sym_zero_pad(1:M:end) = tx_sym;
tx_samples = filter(hs,1,tx_sym_zero_pad);
rx_samples = filter(hs,1,tx_samples);
rx_symbols = rx_samples(1:M:end);

% scatter plot - discard the first few symbols as filter memory is filling
plot(rx_symbols(2*Nsym:end), '+');
