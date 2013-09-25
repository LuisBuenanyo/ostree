#!/bin/bash
#
# Copyright (C) 2011 Colin Walters <walters@verbum.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

set -e

. $(dirname $0)/libtest.sh

echo '1..11'

setup_test_repository "archive-z2"
echo "ok setup"

. ${SRCDIR}/archive-test.sh

cd ${test_tmpdir}
mkdir repo2
${CMD_PREFIX} ostree --repo=repo2 init
${CMD_PREFIX} ostree --repo=repo2 remote add aremote file://$(pwd)/repo test2
ostree --repo=repo2 pull --no-verify-commits aremote
ostree --repo=repo2 rev-parse aremote/test2
ostree --repo=repo2 fsck
echo "ok pull with from file:/// uri"
