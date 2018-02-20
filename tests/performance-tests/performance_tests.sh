#!/bin/sh
#
# Copyright © 2016 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
#

### This is a wrapper script that runs all Mir's performance tests ###

bin_dir=`dirname $0`

title_banner()
{
    echo "======================================================================="
    echo "|  $*"
    echo "======================================================================="
}

tests_run=0
failures=0
for perf_test in mir_glmark2_performance_test \
                 mir_compositor_performance_test \
                 mir_client_startup_performance_test
do
    title_banner "Mir Performance Tests now running: $perf_test"
    tests_run=$(($tests_run + 1))
    $bin_dir/$perf_test || failures=$(($failures + 1))
done

passes=$(($tests_run - $failures))
title_banner "Mir Performance Tests ran $tests_run tests: $passes passed, $failures failed."
return $failures
