<template>
    <portal to="page-header">
        <div class="d-flex align-center">
            <div class="d-inline-flex align-center">
                <h4
                    style="line-height: normal;"
                    class="ml-1 mb-0 color text-navigation text-h4 text-capitalize"
                >
                    {{ $t($route.name) }}
                </h4>
            </div>
        </div>
    </portal>
</template>

<script>
/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-07-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

export default {
    name: 'page-header',
}
</script>
