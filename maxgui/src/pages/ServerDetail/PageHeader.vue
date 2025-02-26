<template>
    <details-page-title>
        <template v-slot:setting-menu>
            <details-icon-group-wrapper multiIcons>
                <template v-slot:body>
                    <v-tooltip
                        bottom
                        transition="slide-y-transition"
                        content-class="shadow-drop color text-navigation py-1 px-4"
                    >
                        <template v-slot:activator="{ on }">
                            <v-btn
                                class="maintain-btn"
                                text
                                color="primary"
                                :disabled="stateMode === 'maintenance'"
                                v-on="on"
                                @click="handleClick('maintain')"
                            >
                                <v-icon size="22">
                                    $vuetify.icons.paused
                                </v-icon>
                            </v-btn>
                        </template>
                        <span>{{ $t('maintain') }} {{ $tc('servers', 1) }} </span>
                    </v-tooltip>
                    <v-tooltip
                        bottom
                        transition="slide-y-transition"
                        content-class="shadow-drop color text-navigation py-1 px-4"
                    >
                        <template v-slot:activator="{ on }">
                            <v-btn
                                class="clear-btn"
                                text
                                color="primary"
                                :disabled="stateMode !== 'maintenance' && stateMode !== 'drained'"
                                v-on="on"
                                @click="handleClick('clear')"
                            >
                                <v-icon size="22">
                                    $vuetify.icons.restart
                                </v-icon>
                            </v-btn>
                        </template>
                        <span>{{ $t('clear') }} {{ $t('currentServerState') }} </span>
                    </v-tooltip>
                </template>
            </details-icon-group-wrapper>
            <details-icon-group-wrapper>
                <template v-slot:body>
                    <v-tooltip
                        bottom
                        transition="slide-y-transition"
                        content-class="shadow-drop color text-navigation py-1 px-4"
                    >
                        <template v-slot:activator="{ on }">
                            <v-btn
                                class="drain-btn"
                                color="primary"
                                :disabled="stateMode === 'maintenance' || stateMode === 'drained'"
                                text
                                v-on="on"
                                @click="handleClick('drain')"
                            >
                                <v-icon size="22">
                                    $vuetify.icons.drain
                                </v-icon>
                            </v-btn>
                        </template>
                        <span>{{ $t('drain') }} {{ $tc('servers', 1) }}</span>
                    </v-tooltip>
                </template>
            </details-icon-group-wrapper>
            <details-icon-group-wrapper>
                <template v-slot:body>
                    <v-tooltip
                        bottom
                        transition="slide-y-transition"
                        content-class="shadow-drop color text-navigation py-1 px-4"
                    >
                        <template v-slot:activator="{ on }">
                            <v-btn
                                class="delete-btn"
                                text
                                color="error"
                                v-on="on"
                                @click="handleClick('delete')"
                            >
                                <v-icon size="18">
                                    $vuetify.icons.delete
                                </v-icon>
                            </v-btn>
                        </template>
                        <span>{{ $t('delete') }} {{ $tc('servers', 1) }}</span>
                    </v-tooltip>
                </template>
            </details-icon-group-wrapper>
        </template>
        <template v-slot:append>
            <confirm-dialog
                v-model="isConfDlgOpened"
                :title="dialogTitle"
                :type="dialogType"
                :smallInfo="smallInfo"
                :item="currentServer"
                :onSave="confirmSave"
            >
                <template
                    v-if="currentStateMode === 'maintenance' && type === 'set'"
                    v-slot:body-append
                >
                    <v-checkbox
                        v-model="forceClosing"
                        class="small mt-2 mb-4"
                        :label="$t('forceClosing')"
                        color="primary"
                        hide-details
                    />
                </template>
            </confirm-dialog>
            <icon-sprite-sheet size="13" class="status-icon mr-1" :frame="stateIconFrame">
                status
            </icon-sprite-sheet>
            <span class="color text-navigation text-body-2 server-healthy">
                {{ serverHealthy }}
            </span>
            <span v-if="version_string" class="color text-field-text text-body-2">
                |
                <span class="version-string">{{ $t('version') }} {{ version_string }}</span>
            </span>
        </template>
    </details-page-title>
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

import { mapActions } from 'vuex'
import goBack from 'mixins/goBack'

export default {
    name: 'page-header',
    mixins: [goBack],
    props: {
        onEditSucceeded: { type: Function, required: true },
        currentServer: { type: Object, required: true },
    },
    data() {
        return {
            dialogTitle: '',
            dialogType: 'unlink',
            smallInfo: '',
            type: 'set', //set or clear
            currentStateMode: '',
            forceClosing: false,
            isConfDlgOpened: false,
        }
    },
    computed: {
        version_string: function() {
            return this.currentServer.attributes.version_string
        },
        serverState: function() {
            return this.currentServer.attributes.state
        },
        /**
         * @returns {Number} returns a number: 0,1,2
         */
        stateIconFrame: function() {
            return this.$help.serverStateIcon(this.serverState)
        },
        serverHealthy: function() {
            switch (this.stateIconFrame) {
                case 0:
                    return this.$t('unHealthy')
                case 1:
                    return this.$t('healthy')
                default:
                    return this.$t('warning')
            }
        },
        stateMode: function() {
            let currentState = this.serverState.toLowerCase()
            if (currentState.indexOf(',') > 0) {
                currentState = currentState.slice(0, currentState.indexOf(','))
            }
            return currentState
        },
    },
    methods: {
        ...mapActions('server', ['destroyServer', 'setOrClearServerState']),

        handleClick(type) {
            this.dialogType = type
            this.dialogTitle = `${this.$t(type)} ${this.$tc('servers', 1)}`
            switch (type) {
                case 'delete':
                    {
                        this.type = 'delete'
                        this.smallInfo = ''
                    }
                    break
                case 'drain':
                    {
                        this.type = 'set'
                        this.currentStateMode = 'drain'
                        this.smallInfo = this.$t(`info.serverDrain`)
                    }
                    break
                case 'clear':
                    {
                        this.type = 'clear'
                        this.currentStateMode =
                            this.stateMode === 'drained' ? 'drain' : this.stateMode
                        this.smallInfo = ''
                    }
                    break
                case 'maintain':
                    {
                        this.type = 'set'
                        this.currentStateMode = 'maintenance'
                        this.smallInfo = this.$t(`info.serverMaintenance`)
                    }
                    break
            }
            this.isConfDlgOpened = true
        },

        async confirmSave() {
            switch (this.type) {
                case 'delete':
                    await this.destroyServer(this.currentServer.id)
                    this.goBack()
                    break
                case 'set':
                case 'clear':
                    await this.performAsyncLoadingAction()
                    break
            }
        },
        async performAsyncLoadingAction() {
            let payload = {
                id: this.currentServer.id,
                stateMode: this.currentStateMode,
                type: this.type,
                callback: this.onEditSucceeded,
            }
            if (this.forceClosing) {
                payload.forceClosing = true
            }
            await this.setOrClearServerState(payload)
        },
    },
}
</script>
