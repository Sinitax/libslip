LIBSLIP_1.0 {
	global:
		slip_init;
		slip_deinit;

		slip_set_rx_callback;
		slip_decode;

		slip_encode;
	local: *;
};
