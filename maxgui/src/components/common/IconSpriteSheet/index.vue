<template>
    <v-icon
        :class="iconClass"
        :size="icon === emptyIcon ? 18 : size"
        :style="icon === emptyIcon ? { margin: '0 -2px' } : ''"
        :color="color"
    >
        {{ icon }}
    </v-icon>
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
    name: 'icon-sprite-sheet',
    props: {
        frame: {
            type: [Number, String],
            required: true,
        },
        size: [Number, String],
        color: String,
        frames: [Array, Object],
        colorClasses: [Array, Object],
    },
    data() {
        return {
            emptyIcon: 'bug_report', // icon to be shown if there is a missing "frame"
            sheets: {
                status: {
                    frames: [
                        '$vuetify.icons.statusError',
                        '$vuetify.icons.statusOk',
                        '$vuetify.icons.statusWarning',
                        '$vuetify.icons.statusInfo',
                    ],
                    colorClasses: ['text-error', 'text-success', 'text-warning', 'text-info'],
                },
                logPriorities: {
                    frames: {
                        alert: '$vuetify.icons.alertWarning',
                        error: '$vuetify.icons.statusError',
                        warning: '$vuetify.icons.statusInfo',
                        notice: '$vuetify.icons.reports',
                        info: '$vuetify.icons.statusInfo',
                        debug: 'bug_report',
                    },
                    colorClasses: {
                        alert: 'text-error',
                        error: 'text-error',
                        warning: 'text-warning',
                        notice: 'text-info',
                        info: 'text-info',
                        debug: 'text-accent',
                    },
                },
            },
        }
    },
    computed: {
        sheet() {
            const name = this.$slots.default ? this.$slots.default[0].text.trim() : ''
            const sheet = this.sheets[name] || {}
            const frames = this.frames || sheet.frames || []
            const colorClasses = this.colorClasses || sheet.colorClasses || []

            return { frames, colorClasses }
        },
        icon() {
            if (!this.sheet.frames[this.frame]) return this.emptyIcon

            return this.sheet.frames[this.frame]
        },
        iconClass() {
            // NOTE: if the color is set trough color property color classes are ignored
            if (this.color || !this.sheet.colorClasses[this.frame]) return ''

            return 'color ' + this.sheet.colorClasses[this.frame]
        },
    },
}
</script>
