/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/cmn_err.h>

void
security_init(int stage)
{
	switch (stage) {
		case 0:
			stage0_security();
			break;
		case 1:
			stage1_security();
			break;
		default:
			cmn_err(CE_PANIC, "JadeSec: invalid init stage %d\n", stage);
	}
}
