#!/bin/bash
#
# Copyright (c) 2024 Stefanos Stefanidis.
# All rights reserved.
#

GITREV=`git rev-list HEAD --count`

if [ "x$GITREV" = "x" ]
then
	GITREV="Untracked"
fi

t=`date +'%Y-%m-%d'`
b=`date +'%Y%m%d'`

major="1"
minor="0"
variant="OpenSysV/git"
v="${major}.${minor}"

cat > vers.c << EOF
/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

/*
 * This file is automatically generated from conf/newvers.sh.
 */

#include <sys/utsname.h>

const int version_major = ${major};
const int version_minor = ${minor};
const char version_variant[] = "${variant}";
const char version[] = "Kernel Release ${v}v${b}";
const char osrelease[] = "${major}.${minor}";
const char ostype[] = "SysVr4";
const char bldstr[] = "${b}";

struct utsname utsname = {
	"JadeOS", "unix", "4.0", "3.0", "riscv"
};
EOF